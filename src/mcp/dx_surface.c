/* dx_surface.c - The DX single-tool surface over the code graph.
 *
 * See dx_surface.h and dx/F1-SURFACE.md for the contract. This file
 * implements the envelope validation, the request-type dispatch table and the
 * response wrapping. It deliberately depends on nothing but yyjson and the C
 * standard library, so the contract is testable without building the server.
 */
#include "mcp/dx_surface.h"

#include <yyjson/yyjson.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ types */

/* The dispatch table. One row per request type: the schema discriminator and
 * the base tool it forwards to. "trace" and "impact" share trace_path - the
 * impact direction is one of that tool's own parameters, not a second tool.
 * The governance rows have no base tool: they are answered by the DX layer. */
typedef struct {
    const char *name;
    const char *base_tool;
} dx_type_def_t;

static const dx_type_def_t DX_TYPES[DX_REQ_TYPE_COUNT] = {
    {"search", "search_graph"},
    {"trace", "trace_path"},
    {"impact", "trace_path"},
    {"details", "get_code_snippet"},
    {"outline", "get_file_outline"},
    {"architecture", "get_architecture"},
    {"schema", "get_graph_schema"},
    {"coverage", "check_index_coverage"},
    {"changes", "detect_changes"},
    {"escape", NULL},
    {"verify", NULL},
    {"certificate", NULL},
    {"enforce", NULL},
};

const char *dx_surface_type_name(dx_request_type_t type) {
    if (type < 0 || type >= DX_REQ_TYPE_COUNT) {
        return "unknown";
    }
    return DX_TYPES[type].name;
}

bool dx_surface_type_from_name(const char *name, dx_request_type_t *out) {
    if (!name) {
        return false;
    }
    for (int i = 0; i < DX_REQ_TYPE_COUNT; i++) {
        if (strcmp(DX_TYPES[i].name, name) == 0) {
            if (out) {
                *out = (dx_request_type_t)i;
            }
            return true;
        }
    }
    return false;
}

const char *dx_surface_base_tool(dx_request_type_t type) {
    if (type < 0 || type >= DX_REQ_TYPE_COUNT) {
        return NULL;
    }
    return DX_TYPES[type].base_tool;
}

bool dx_surface_type_is_governance(dx_request_type_t type) {
    return type == DX_REQ_VERIFY || type == DX_REQ_CERTIFICATE || type == DX_REQ_ENFORCE;
}

/* ----------------------------------------------------------------- schema */

/* The advertised schema. The "type" enum is checked against DX_TYPES by
 * tests/test_dx_surface.c, so the schema and the dispatch table cannot
 * drift apart without a test failing. */
static const char *DX_TOOL_SCHEMA =
    "{\"type\":\"object\","
    "\"required\":[\"question\",\"draft\",\"review\",\"request\"],"
    "\"properties\":{"
    "\"question\":{\"type\":\"string\",\"minLength\":1,\"description\":\"The code question, in "
    "the caller's own words. Do not pre-shape it into symbol names.\"},"
    "\"draft\":{\"type\":\"object\",\"required\":[\"reason\",\"type\"],\"additionalProperties\":false,"
    "\"properties\":{"
    "\"reason\":{\"type\":\"string\",\"minLength\":1,\"description\":\"Why this request type looks "
    "like the smallest answer.\"},"
    "\"type\":{\"type\":\"string\",\"enum\":[\"search\",\"trace\",\"impact\",\"details\",\"outline\","
    "\"architecture\",\"schema\",\"coverage\",\"changes\",\"escape\",\"verify\",\"certificate\","
    "\"enforce\"],\"description\":\"The request "
    "type being considered.\"}},"
    "\"description\":\"The drafted plan: a request type, plus the reason it looks smallest.\"},"
    "\"review\":{\"type\":\"string\",\"minLength\":1,\"description\":\"Self-correct the draft. If the "
    "draft was wrong or too broad, name the better type here.\"},"
    "\"request\":{\"type\":\"object\",\"required\":[\"type\"],\"additionalProperties\":true,"
    "\"properties\":{"
    "\"type\":{\"type\":\"string\",\"enum\":[\"search\",\"trace\",\"impact\",\"details\",\"outline\","
    "\"architecture\",\"schema\",\"coverage\",\"changes\",\"escape\",\"verify\",\"certificate\","
    "\"enforce\"],\"description\":\"The committed "
    "request type. The other fields are forwarded to the matching base tool unchanged.\"},"
    "\"source\":{\"type\":\"string\",\"enum\":[\"outline\",\"full\"],\"default\":\"outline\","
    "\"description\":\"Whether the answer may carry source bodies. 'outline' - the default - answers "
    "with relationships and a file:line pointer, so the answer does not grow with the repository. "
    "'full' asks for the source text and costs what the source costs.\"},"
    "\"max_tokens\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":1000000,"
    "\"description\":\"The token budget for the answer. An answer that does not fit is REFUSED with "
    "the excess named, never silently truncated. Omit it to use the base tool's own default.\"}},"
    "\"description\":\"The committed request. Its type-specific fields are the base tool's own "
    "parameters and are forwarded verbatim.\"}},"
    "\"additionalProperties\":false,"
    "\"description\":\"The envelope is not optional. Answering requires all four fields; a call that "
    "omits one is rejected rather than answered from a default.\"}";

const char *dx_surface_tool_schema(void) {
    return DX_TOOL_SCHEMA;
}

/* ------------------------------------------------------------------ utils */

/* The base's own byte-per-token rate. The base states it in mcp.c
 * (MCP_OUTPUT_BYTES_PER_TOKEN_ESTIMATE); a budget that means two different
 * things in two files is not a budget. */
#define DX_BYTES_PER_TOKEN 4

/* Build a heap message. Returns NULL on OOM. */
static char *dx_msg(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    va_list ap2;
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) {
        va_end(ap2);
        return NULL;
    }
    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) {
        va_end(ap2);
        return NULL;
    }
    vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return buf;
}

/* A non-empty string field, or NULL. */
static const char *dx_str_field(yyjson_val *obj, const char *key) {
    yyjson_val *v = yyjson_obj_get(obj, key);
    if (!yyjson_is_str(v) || yyjson_get_len(v) == 0) {
        return NULL;
    }
    return yyjson_get_str(v);
}

/* A positive integer field, or 0 when absent or not a positive integer. */
static long long dx_pos_int_field(yyjson_val *obj, const char *key) {
    yyjson_val *v = yyjson_obj_get(obj, key);
    if (!yyjson_is_num(v)) {
        return 0;
    }
    double d = yyjson_get_num(v);
    if (d < 1.0 || d > 1000000.0) {
        return 0;
    }
    return (long long)d;
}

/* ------------------------------------------------------------------- parse */

int dx_surface_parse(const char *args_json, dx_surface_request_t *out, char **err) {
    if (err) {
        *err = NULL;
    }
    if (!out) {
        if (err) {
            *err = dx_msg("dx_codebase: internal error, no request slot");
        }
        return -1;
    }
    memset(out, 0, sizeof(*out));

    if (!args_json) {
        if (err) {
            *err = dx_msg("dx_codebase: no arguments were supplied");
        }
        return -1;
    }

    yyjson_doc *doc = yyjson_read(args_json, strlen(args_json), 0);
    if (!doc) {
        if (err) {
            *err = dx_msg("dx_codebase: the arguments are not valid JSON");
        }
        return -1;
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_obj(root)) {
        if (err) {
            *err = dx_msg("dx_codebase: the arguments must be a JSON object");
        }
        yyjson_doc_free(doc);
        return -1;
    }

    /* The four envelope fields. Each is required: a required field on a typed
     * schema is the one thing a model cannot skip. */
    const char *question = dx_str_field(root, "question");
    if (!question) {
        if (err) {
            *err = dx_msg("dx_codebase: 'question' is required and must be a non-empty string");
        }
        goto fail;
    }

    yyjson_val *draft = yyjson_obj_get(root, "draft");
    if (!yyjson_is_obj(draft)) {
        if (err) {
            *err = dx_msg("dx_codebase: 'draft' is required and must be an object with 'reason' and "
                          "'type'");
        }
        goto fail;
    }
    const char *draft_reason = dx_str_field(draft, "reason");
    if (!draft_reason) {
        if (err) {
            *err = dx_msg("dx_codebase: 'draft.reason' is required and must be a non-empty string");
        }
        goto fail;
    }
    const char *draft_type_name = dx_str_field(draft, "type");
    dx_request_type_t draft_type;
    if (!draft_type_name || !dx_surface_type_from_name(draft_type_name, &draft_type)) {
        if (err) {
            *err = dx_msg("dx_codebase: 'draft.type' is required and must be one of search, trace, "
                          "impact, details, outline, architecture, schema, coverage, changes, escape");
        }
        goto fail;
    }

    const char *review = dx_str_field(root, "review");
    if (!review) {
        if (err) {
            *err = dx_msg("dx_codebase: 'review' is required and must be a non-empty string");
        }
        goto fail;
    }

    yyjson_val *request = yyjson_obj_get(root, "request");
    if (!yyjson_is_obj(request)) {
        if (err) {
            *err = dx_msg("dx_codebase: 'request' is required and must be an object with a 'type'");
        }
        goto fail;
    }
    const char *request_type_name = dx_str_field(request, "type");
    dx_request_type_t request_type;
    if (!request_type_name || !dx_surface_type_from_name(request_type_name, &request_type)) {
        if (err) {
            *err = dx_msg("dx_codebase: 'request.type' is required and must be one of search, trace, "
                          "impact, details, outline, architecture, schema, coverage, changes, escape");
        }
        goto fail;
    }

    /* The request's own fields, forwarded verbatim. "type" is ours, not the
     * base tool's, so it is dropped. "source" and "max_tokens" are ours too:
     * they are the answer policy, and the base expresses them in its own
     * vocabulary (source_mode, max_output_tokens). */
    /* The source policy. The base's source_mode is only understood by
     * get_code_snippet, so this is the one request type the DX layer can
     * honour. Asking for "full" anywhere else would be a silent no-op - and a
     * silent no-op is exactly what this tool exists to prevent - so it is
     * refused, and the refusal names the types that work. */
    dx_source_policy_t source_policy = DX_SOURCE_OUTLINE;
    const char *source_name = dx_str_field(request, "source");
    if (source_name) {
        if (strcmp(source_name, "outline") == 0) {
            source_policy = DX_SOURCE_OUTLINE;
        } else if (strcmp(source_name, "full") == 0) {
            if (request_type != DX_REQ_DETAILS) {
                if (err) {
                    *err = dx_msg("dx_codebase: 'request.source' is 'full', which this request "
                                  "type cannot honour; only 'details' can ask for source bodies");
                }
                goto fail;
            }
            source_policy = DX_SOURCE_FULL;
        } else {
            if (err) {
                *err = dx_msg("dx_codebase: 'request.source' must be 'outline' or 'full'");
            }
            goto fail;
        }
    }

    /* The budget. A budget that is present but malformed is an error, not an
     * absent budget: silently ignoring it would hand back an unbounded answer
     * to a caller who believes they set a limit. */
    long long max_tokens = 0;
    bool max_tokens_set = false;
    yyjson_val *budget_val = yyjson_obj_get(request, "max_tokens");
    if (budget_val) {
        if (!yyjson_is_num(budget_val)) {
            if (err) {
                *err = dx_msg("dx_codebase: 'request.max_tokens' must be a positive integer");
            }
            goto fail;
        }
        max_tokens = dx_pos_int_field(request, "max_tokens");
        if (max_tokens <= 0) {
            if (err) {
                *err = dx_msg("dx_codebase: 'request.max_tokens' must be between 1 and 1000000");
            }
            goto fail;
        }
        max_tokens_set = true;
    }

    yyjson_mut_doc *adoc = yyjson_mut_doc_new(NULL);
    if (!adoc) {
        if (err) {
            *err = dx_msg("dx_codebase: out of memory");
        }
        goto fail;
    }
    yyjson_mut_val *aroot = yyjson_mut_obj(adoc);
    yyjson_mut_doc_set_root(adoc, aroot);

    bool caller_set_max_output_tokens = false;
    size_t idx;
    size_t max;
    yyjson_val *key;
    yyjson_val *val;
    yyjson_obj_foreach(request, idx, max, key, val) {
        if (yyjson_equals_str(key, "type") || yyjson_equals_str(key, "source") ||
            yyjson_equals_str(key, "max_tokens")) {
            continue;
        }
        if (yyjson_equals_str(key, "max_output_tokens")) {
            caller_set_max_output_tokens = true;
        }
        yyjson_mut_val *copy = yyjson_val_mut_copy(adoc, val);
        if (!copy) {
            yyjson_mut_doc_free(adoc);
            if (err) {
                *err = dx_msg("dx_codebase: out of memory");
            }
            goto fail;
        }
        if (!yyjson_mut_obj_add_val(adoc, aroot, yyjson_get_str(key), copy)) {
            yyjson_mut_doc_free(adoc);
            if (err) {
                *err = dx_msg("dx_codebase: out of memory");
            }
            goto fail;
        }
    }

    /* The budget, in the base's vocabulary. Every base tool the DX surface can
     * reach advertises max_output_tokens. A caller who set it directly keeps
     * their value: an explicit parameter beats a default. */
    if (max_tokens_set && !caller_set_max_output_tokens) {
        yyjson_mut_val *budget = yyjson_mut_int(adoc, (int64_t)max_tokens);
        if (!budget || !yyjson_mut_obj_add_val(adoc, aroot, "max_output_tokens", budget)) {
            yyjson_mut_doc_free(adoc);
            if (err) {
                *err = dx_msg("dx_codebase: out of memory");
            }
            goto fail;
        }
    }

    /* The source policy, in the base's vocabulary. Only get_code_snippet takes
     * source_mode; the other tools answer with relationships already. */
    if (source_policy == DX_SOURCE_OUTLINE && request_type == DX_REQ_DETAILS) {
        yyjson_mut_val *mode = yyjson_mut_str(adoc, "outline");
        if (!mode || !yyjson_mut_obj_add_val(adoc, aroot, "source_mode", mode)) {
            yyjson_mut_doc_free(adoc);
            if (err) {
                *err = dx_msg("dx_codebase: out of memory");
            }
            goto fail;
        }
    }

    out->args_json = yyjson_mut_write(adoc, 0, NULL);
    yyjson_mut_doc_free(adoc);
    if (!out->args_json) {
        if (err) {
            *err = dx_msg("dx_codebase: out of memory");
        }
        goto fail;
    }

    out->question = (char *)malloc(strlen(question) + 1);
    out->draft_reason = (char *)malloc(strlen(draft_reason) + 1);
    out->review = (char *)malloc(strlen(review) + 1);
    if (!out->question || !out->draft_reason || !out->review) {
        if (err) {
            *err = dx_msg("dx_codebase: out of memory");
        }
        goto fail;
    }
    memcpy(out->question, question, strlen(question) + 1);
    memcpy(out->draft_reason, draft_reason, strlen(draft_reason) + 1);
    memcpy(out->review, review, strlen(review) + 1);
    out->draft_type = draft_type;
    out->request_type = request_type;
    out->base_tool = dx_surface_base_tool(request_type);
    out->source_policy = source_policy;
    out->max_tokens = max_tokens;
    out->max_tokens_set = max_tokens_set;

    yyjson_doc_free(doc);
    return 0;

fail:
    yyjson_doc_free(doc);
    dx_surface_request_free(out);
    return -1;
}

void dx_surface_request_free(dx_surface_request_t *req) {
    if (!req) {
        return;
    }
    free(req->question);
    free(req->draft_reason);
    free(req->review);
    free(req->args_json);
    req->question = NULL;
    req->draft_reason = NULL;
    req->review = NULL;
    req->args_json = NULL;
}

const char *dx_surface_escape_text(void) {
    return "This is not a graph question. The code graph answers structural questions - who calls "
           "what, what a change reaches, what a file contains, how the architecture fits together. "
           "For a question about text, a value, a message or a file's contents, use your own file "
           "and search tools; the graph will not have the answer.";
}

/* ------------------------------------------------------------------- audit */

char *dx_surface_audit_text(const dx_surface_request_t *req, const char *checked) {
    if (!req) {
        return NULL;
    }
    const char *committed = dx_surface_type_name(req->request_type);
    const char *drafted = dx_surface_type_name(req->draft_type);
    const char *source = checked && checked[0] ? checked : "the indexed graph";
    if (strcmp(drafted, committed) == 0) {
        return dx_msg("Answered from %s. The committed request type was '%s', which the draft also "
                      "proposed. The graph is the only source consulted; where it does not know, "
                      "the answer says so.",
                      source, committed);
    }
    return dx_msg("Answered from %s. The draft proposed '%s'; the review changed it to '%s', and "
                  "that is the type that was answered. The graph is the only source consulted; "
                  "where it does not know, the answer says so.",
                  source, drafted, committed);
}

/* --------------------------------------------------------------- budget */

/* A tool result carrying an error, in the base's own shape for an error with a
 * non-object payload: the text in content, and {"error": text} in
 * structuredContent. A refusal that looked different from a base error would
 * read as a different kind of failure, and the agent would not know how to
 * treat it. */
static char *dx_tool_result_error(const char *text) {
    if (!text) {
        return NULL;
    }
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    if (!doc) {
        return NULL;
    }
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);

    yyjson_mut_val *content = yyjson_mut_arr(doc);
    yyjson_mut_val *item = yyjson_mut_obj(doc);
    (void)yyjson_mut_obj_add_str(doc, item, "type", "text");
    (void)yyjson_mut_obj_add_str(doc, item, "text", text);
    (void)yyjson_mut_arr_add_val(content, item);
    (void)yyjson_mut_obj_add_val(doc, root, "content", content);

    yyjson_mut_val *structured = yyjson_mut_obj(doc);
    (void)yyjson_mut_obj_add_str(doc, structured, "error", text);
    (void)yyjson_mut_obj_add_val(doc, root, "structuredContent", structured);

    char *out = yyjson_mut_write(doc, 0, NULL);
    yyjson_mut_doc_free(doc);
    return out;
}

long long dx_surface_estimate_tokens(const char *text) {
    if (!text) {
        return 0;
    }
    size_t bytes = strlen(text);
    return (long long)((bytes + (size_t)DX_BYTES_PER_TOKEN - 1U) / (size_t)DX_BYTES_PER_TOKEN);
}

/* Count the source bodies in a tool result. The body lives under a "source"
 * key in the structured content; the rendered text is not scanned, because the
 * base already decides that shape and rewriting a rendered tree would be
 * guesswork. */
/* The base writes a placeholder rather than omitting the key when a source
 * body is not available, so key presence alone would over-report: an answer
 * with no body at all would be reported as carrying one. A body counts only
 * when the value is a real, non-empty string. */
#define DX_SOURCE_PLACEHOLDER "(source not available)"

static bool dx_is_real_body(yyjson_val *value) {
    if (!yyjson_is_str(value)) {
        return false;
    }
    const char *text = yyjson_get_str(value);
    if (!text || !text[0]) {
        return false;
    }
    if (strcmp(text, DX_SOURCE_PLACEHOLDER) == 0) {
        return false;
    }
    return true;
}

/* Count the source bodies anywhere in a payload. The base carries a body
 * under the key "source" at the top of a snippet result and on the rows of a
 * tree, so a single top-level lookup would miss the tree ones and the shape
 * report would under-count. */
static int dx_count_bodies_in(yyjson_val *val) {
    if (!val) {
        return 0;
    }
    int count = 0;
    if (yyjson_is_obj(val)) {
        size_t idx;
        size_t max;
        yyjson_val *key;
        yyjson_val *child;
        yyjson_obj_foreach(val, idx, max, key, child) {
            if (yyjson_equals_str(key, "source")) {
                if (dx_is_real_body(child)) {
                    count++;
                }
                continue; /* a body is text; there is nothing under it */
            }
            count += dx_count_bodies_in(child);
        }
    } else if (yyjson_is_arr(val)) {
        size_t idx;
        size_t max;
        yyjson_val *child;
        yyjson_arr_foreach(val, idx, max, child) {
            count += dx_count_bodies_in(child);
        }
    }
    return count;
}

int dx_surface_count_bodies(const char *result_json) {
    if (!result_json) {
        return 0;
    }
    yyjson_doc *doc = yyjson_read(result_json, strlen(result_json), 0);
    if (!doc) {
        return 0;
    }
    int count = 0;
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (yyjson_is_obj(root)) {
        yyjson_val *structured = yyjson_obj_get(root, "structuredContent");
        /* The base's own results carry the payload under structuredContent.
         * A result that does not have one is walked from the top instead,
         * so nothing is under-counted just because the shape differs. */
        count = dx_count_bodies_in(structured ? structured : root);
    }
    yyjson_doc_free(doc);
    return count;
}

char *dx_surface_shape_json(const dx_answer_shape_t *shape) {
    if (!shape) {
        return NULL;
    }
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    if (!doc) {
        return NULL;
    }
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);

    (void)yyjson_mut_obj_add_str(doc, root, "source",
                                 shape->source_policy == DX_SOURCE_FULL ? "full" : "outline");
    (void)yyjson_mut_obj_add_int(doc, root, "tokens", (int64_t)shape->tokens);
    if (shape->budget > 0) {
        (void)yyjson_mut_obj_add_int(doc, root, "budget", (int64_t)shape->budget);
    }
    (void)yyjson_mut_obj_add_bool(doc, root, "over_budget", shape->over_budget);
    if (shape->over_budget) {
        (void)yyjson_mut_obj_add_int(doc, root, "excess", (int64_t)shape->excess);
    }
    (void)yyjson_mut_obj_add_int(doc, root, "bodies_included", (int64_t)shape->bodies_included);

    char *out = yyjson_mut_write(doc, 0, NULL);
    yyjson_mut_doc_free(doc);
    return out;
}

char *dx_surface_apply_budget(const char *result_json, const dx_surface_request_t *req,
                               dx_answer_shape_t *shape) {
    if (!shape) {
        return NULL;
    }
    memset(shape, 0, sizeof(*shape));
    shape->source_policy = DX_SOURCE_OUTLINE;

    if (!result_json) {
        return NULL;
    }
    if (req) {
        shape->source_policy = req->source_policy;
        shape->budget = req->max_tokens_set ? req->max_tokens : 0;
    }
    shape->tokens = dx_surface_estimate_tokens(result_json);
    shape->bodies_included = dx_surface_count_bodies(result_json);

    if (shape->budget > 0 && shape->tokens > shape->budget) {
        shape->over_budget = true;
        shape->excess = shape->tokens - shape->budget;
        /* A refusal, not a truncation. An answer that silently lost rows reads
         * as a complete answer, and the agent cannot tell the difference. */
        char *text = dx_msg(
            "The answer was %lld tokens and the budget was %lld, so it is refused rather than "
            "truncated: a shortened answer reads as a complete one. Raise max_tokens, narrow the "
            "request, or ask for one symbol at a time.",
            shape->tokens, shape->budget);
        if (!text) {
            return NULL;
        }
        char *out = dx_tool_result_error(text);
        free(text);
        return out;
    }
    /* Always a fresh buffer: the caller frees this, and a borrowed pointer
     * that is sometimes owned and sometimes not is a bug waiting to happen. */
    size_t len = strlen(result_json);
    char *copy = (char *)malloc(len + 1);
    if (!copy) {
        return NULL;
    }
    memcpy(copy, result_json, len + 1);
    return copy;
}

/* ------------------------------------------------------------------- wrap */


char *dx_surface_wrap_result(const char *result_json, const char *audit_text,
                              const char *shape_json, const char *next_kind,
                              const char *next_detail) {
    if (!result_json) {
        return NULL;
    }

    yyjson_doc *doc = yyjson_read(result_json, strlen(result_json), 0);
    yyjson_val *root = doc ? yyjson_doc_get_root(doc) : NULL;
    if (!yyjson_is_obj(root)) {
        if (doc) {
            yyjson_doc_free(doc);
        }
        return NULL;
    }

    yyjson_mut_doc *mdoc = yyjson_mut_doc_new(NULL);
    if (!mdoc) {
        yyjson_doc_free(doc);
        return NULL;
    }
    yyjson_mut_val *mroot = yyjson_mut_obj(mdoc);
    yyjson_mut_doc_set_root(mdoc, mroot);

    size_t idx;
    size_t max;
    yyjson_val *key;
    yyjson_val *val;
    yyjson_obj_foreach(root, idx, max, key, val) {
        /* The DX envelope owns these three keys. If the base result already
         * carries one, the DX value replaces it rather than sitting beside
         * it: a JSON object with the same key twice is ambiguous, and a
         * parser is free to take either. */
        if (yyjson_equals_str(key, "audit") || yyjson_equals_str(key, "shape") ||
            yyjson_equals_str(key, "next")) {
            continue;
        }
        yyjson_mut_val *copy = yyjson_val_mut_copy(mdoc, val);
        if (!copy) {
            yyjson_mut_doc_free(mdoc);
            yyjson_doc_free(doc);
            return NULL;
        }
        if (!yyjson_mut_obj_add_val(mdoc, mroot, yyjson_get_str(key), copy)) {
            yyjson_mut_doc_free(mdoc);
            yyjson_doc_free(doc);
            return NULL;
        }
    }

    if (audit_text) {
        (void)yyjson_mut_obj_add_str(mdoc, mroot, "audit", audit_text);
    }

    if (shape_json) {
        yyjson_doc *shape_doc = yyjson_read(shape_json, strlen(shape_json), 0);
        yyjson_val *shape_root = shape_doc ? yyjson_doc_get_root(shape_doc) : NULL;
        if (yyjson_is_obj(shape_root)) {
            yyjson_mut_val *shape = yyjson_val_mut_copy(mdoc, shape_root);
            if (shape) {
                (void)yyjson_mut_obj_add_val(mdoc, mroot, "shape", shape);
            }
        }
        if (shape_doc) {
            yyjson_doc_free(shape_doc);
        }
    }

    yyjson_mut_val *next = yyjson_mut_obj(mdoc);
    (void)yyjson_mut_obj_add_str(mdoc, next, "kind", next_kind ? next_kind : DX_NEXT_ANSWER);
    if (next_detail && next_detail[0]) {
        (void)yyjson_mut_obj_add_str(mdoc, next, "detail", next_detail);
    }
    (void)yyjson_mut_obj_add_val(mdoc, mroot, "next", next);

    char *out = yyjson_mut_write(mdoc, 0, NULL);
    yyjson_mut_doc_free(mdoc);
    yyjson_doc_free(doc);
    return out;
}
