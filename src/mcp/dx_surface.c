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
 * impact direction is one of that tool's own parameters, not a second tool. */
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
    "\"architecture\",\"schema\",\"coverage\",\"changes\",\"escape\"],\"description\":\"The request "
    "type being considered.\"}},"
    "\"description\":\"The drafted plan: a request type, plus the reason it looks smallest.\"},"
    "\"review\":{\"type\":\"string\",\"minLength\":1,\"description\":\"Self-correct the draft. If the "
    "draft was wrong or too broad, name the better type here.\"},"
    "\"request\":{\"type\":\"object\",\"required\":[\"type\"],\"additionalProperties\":true,"
    "\"properties\":{"
    "\"type\":{\"type\":\"string\",\"enum\":[\"search\",\"trace\",\"impact\",\"details\",\"outline\","
    "\"architecture\",\"schema\",\"coverage\",\"changes\",\"escape\"],\"description\":\"The committed "
    "request type. The other fields are forwarded to the matching base tool unchanged.\"}},"
    "\"description\":\"The committed request. Its type-specific fields are the base tool's own "
    "parameters and are forwarded verbatim.\"}},"
    "\"additionalProperties\":false,"
    "\"description\":\"The envelope is not optional. Answering requires all four fields; a call that "
    "omits one is rejected rather than answered from a default.\"}";

const char *dx_surface_tool_schema(void) {
    return DX_TOOL_SCHEMA;
}

/* ------------------------------------------------------------------ utils */

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
     * base tool's, so it is dropped. */
    yyjson_mut_doc *adoc = yyjson_mut_doc_new(NULL);
    if (!adoc) {
        if (err) {
            *err = dx_msg("dx_codebase: out of memory");
        }
        goto fail;
    }
    yyjson_mut_val *aroot = yyjson_mut_obj(adoc);
    yyjson_mut_doc_set_root(adoc, aroot);

    size_t idx;
    size_t max;
    yyjson_val *key;
    yyjson_val *val;
    yyjson_obj_foreach(request, idx, max, key, val) {
        if (yyjson_equals_str(key, "type")) {
            continue;
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

/* ------------------------------------------------------------------- wrap */

char *dx_surface_wrap_result(const char *result_json, const char *audit_text,
                              const char *next_kind, const char *next_detail) {
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
