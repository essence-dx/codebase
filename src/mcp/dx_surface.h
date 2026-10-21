/* dx_surface.h - The DX single-tool surface over the code graph.
 *
 * The base ships 17 MCP tools and a Cypher query dialect. An agent wired to
 * the server without the install-time skill file never reaches the relation
 * graph - it greps instead (measured: the MCP was called zero times and every
 * run fell back to the shell). The surface, not the graph, is what loses.
 *
 * This module replaces the menu with one tool whose input schema carries the
 * reasoning as required arguments. A required field on a typed schema cannot be
 * skipped the way a line of prompt can be ignored.
 *
 * The envelope is:
 *
 *   question : string                     the code question, in the caller's words
 *   draft    : { reason, type }           the smallest plan, and why it looks smallest
 *   review   : string                     self-correction; may name a different type
 *   request  : { type, ...fields }        the committed request
 *
 * The request's type-specific fields are forwarded verbatim to the underlying
 * base tool, so this module owns no per-tool field mapping and cannot drift
 * from the base tools' own schemas.
 *
 * The module depends on yyjson and the C standard library only - no store, no
 * tree-sitter, no SQLite - so its contract is verifiable without the full
 * build. See tests/test_dx_surface.c.
 */
#ifndef CBM_MCP_DX_SURFACE_H
#define CBM_MCP_DX_SURFACE_H

#include <stdbool.h>
#include <stddef.h>

/* The request types the envelope accepts. The order is the order the schema
 * advertises them and the order the help text lists them. */
typedef enum {
    DX_REQ_SEARCH = 0,
    DX_REQ_TRACE,
    DX_REQ_IMPACT,
    DX_REQ_DETAILS,
    DX_REQ_OUTLINE,
    DX_REQ_ARCHITECTURE,
    DX_REQ_SCHEMA,
    DX_REQ_COVERAGE,
    DX_REQ_CHANGES,
    DX_REQ_ESCAPE,
    /* The governance requests. These are DX features, not base tools, so they
     * have no entry in the base-tool table; the server handles them itself
     * and the verdict policy lives in dx_governance.c. */
    DX_REQ_VERIFY,
    DX_REQ_CERTIFICATE,
    DX_REQ_ENFORCE,
    DX_REQ_TYPE_COUNT
} dx_request_type_t;

/* What to do with the answer. */
#define DX_NEXT_ANSWER "answer"
#define DX_NEXT_INSPECT "inspect"
#define DX_NEXT_ESCAPE "escape"

/* Whether the answer may carry source bodies.
 *
 * The base already knows how to answer with relationships instead of bodies -
 * get_code_snippet's source_mode "outline" - but it decides for itself
 * (source_mode "auto" returns the body for anything that is not a large
 * container). The DX surface makes the choice explicit and defaults it to the
 * cheap shape, because a token cap spent on source text is still a token cap
 * spent on the wrong thing.
 */
typedef enum {
    DX_SOURCE_OUTLINE = 0, /* relationships only - the default */
    DX_SOURCE_FULL = 1,    /* the caller explicitly asked for bodies */
} dx_source_policy_t;

/* A parsed and validated envelope. */
typedef struct {
    char *question;         /* the caller's question, verbatim */
    char *draft_reason;     /* why the draft looked smallest */
    dx_request_type_t draft_type;   /* the type the draft considered */
    char *review;           /* the self-correction, verbatim */
    dx_request_type_t request_type; /* the committed type */
    char *args_json;        /* the request object minus the DX fields, verbatim */
    const char *base_tool;  /* the underlying base tool; NULL for escape */
    dx_source_policy_t source_policy; /* what the answer may carry */
    long long max_tokens;   /* the caller's budget; 0 means the base's own */
    bool max_tokens_set;    /* whether the caller set one at all */
} dx_surface_request_t;

/* What the answer turned out to be, and what it cost. */
typedef struct {
    dx_source_policy_t source_policy; /* what the caller asked for */
    long long tokens;                /* estimated tokens of the composed answer */
    long long budget;                /* the caller's budget; 0 means unlimited */
    bool over_budget;                /* the answer did not fit and was refused */
    long long excess;                /* how far over it was */
    int bodies_included;             /* source bodies in the answer */
} dx_answer_shape_t;


/* The JSON schema the tool advertises. Static lifetime; do not free. */
const char *dx_surface_tool_schema(void);

/* Name <-> type. The name is the schema discriminator. Both are static
 * strings; do not free. */
const char *dx_surface_type_name(dx_request_type_t type);
bool dx_surface_type_from_name(const char *name, dx_request_type_t *out);

/* The base tool a request type dispatches to, or NULL for escape and for the
 * governance requests, which the server handles itself. */
const char *dx_surface_base_tool(dx_request_type_t type);

/* Is this one of the governance requests? They are answered by the DX layer
 * rather than by a base tool, and they are the only types whose answer is a
 * verdict about a claim rather than a fact about the code. */
bool dx_surface_type_is_governance(dx_request_type_t type);

/* Parse and validate an envelope.
 *
 * Returns 0 on success. On failure returns -1 and stores a heap-allocated
 * message in *err naming the field and the rule (caller frees). The envelope
 * may not be skipped: a call missing any of the four fields is rejected, and
 * no default type is ever assumed. */
int dx_surface_parse(const char *args_json, dx_surface_request_t *out, char **err);

/* Release what dx_surface_parse allocated. Safe on a zeroed struct. */
void dx_surface_request_free(dx_surface_request_t *req);

/* The bail-out text for an escape request: the question is not a graph
 * question and the caller's own tools are the right answer. */
const char *dx_surface_escape_text(void);

/* Build the audit text: what the server checked this answer against, and
 * which type was committed after the review. Returns a heap-allocated plain
 * string (caller frees), or NULL on OOM. */
char *dx_surface_audit_text(const dx_surface_request_t *req, const char *checked);

/* Wrap a base tool result with the audit, shape and next fields.
 *
 * result_json is a tool-result object. The result gains three top-level keys:
 * "audit" (a string), "shape" (a JSON object string) and "next" (an object with
 * "kind" and "detail").
 * Returns a heap-allocated JSON string (caller frees), or NULL on OOM. */
char *dx_surface_wrap_result(const char *result_json, const char *audit_text,
                              const char *shape_json, const char *next_kind,
                              const char *next_detail);

/* Estimate a token count at the base's own rate: four UTF-8 bytes per token.
 * The base states the same contract in mcp.c (MCP_OUTPUT_BYTES_PER_TOKEN_ESTIMATE),
 * and the two must agree, or a budget means two different things. */
long long dx_surface_estimate_tokens(const char *text);

/* Count the source bodies in a tool result. */
int dx_surface_count_bodies(const char *result_json);

/* Apply the answer policy to a composed result.
 *
 * The budget is a GATE, not a warning: an answer that does not fit is refused
 * with the excess named, because a silently truncated answer reads as a
 * complete one. The shape is filled in either way, so the response can always
 * say what it got and what it cost.
 *
 * Returns a heap-allocated JSON string (caller frees) - the result, or the
 * refusal when the budget was exceeded. Returns NULL only on OOM. */
char *dx_surface_apply_budget(const char *result_json, const dx_surface_request_t *req,
                               dx_answer_shape_t *shape);

/* The shape, as a JSON object: what the answer carries and what it cost. */
char *dx_surface_shape_json(const dx_answer_shape_t *shape);

#endif /* CBM_MCP_DX_SURFACE_H */
