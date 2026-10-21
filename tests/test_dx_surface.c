/* test_dx_surface.c - The DX single-tool surface contract.
 *
 * The envelope may not be skipped, the request type must be known, and the
 * committed type is what gets answered. The schema's enum and the dispatch
 * table must not drift apart, so the schema is parsed and compared here.
 *
 * See dx/F1-SURFACE.md for the contract these tests pin.
 */
#include "test_framework.h"

#include <mcp/dx_surface.h>
#include <yyjson/yyjson.h>

#include <stdlib.h>
#include <string.h>

/* A complete, valid envelope. Tests mutate a copy of it. */
static const char *DX_VALID =
    "{\"question\":\"how does a request reach the database?\","
    "\"draft\":{\"reason\":\"a symbol lookup is the smallest answer\","
    "\"type\":\"search\"},"
    "\"review\":\"the draft was right\","
    "\"request\":{\"type\":\"search\",\"query\":\"handleRequest\",\"limit\":10}}";

/* Build an envelope with one top-level field replaced. The caller frees the
 * result. The four envelope fields are all top-level, so a splice of the
 * valid envelope is enough and keeps this test free of a JSON writer. */
static char *dx_envelope(const char *field, const char *value_json) {
    static const struct {
        const char *key;
        const char *needle;
    } fields[] = {
        {"question", "\"question\":\"how does a request reach the database?\""},
        {"draft",
         "\"draft\":{\"reason\":\"a symbol lookup is the smallest answer\",\"type\":\"search\"}"},
        {"review", "\"review\":\"the draft was right\""},
        {"request", "\"request\":{\"type\":\"search\",\"query\":\"handleRequest\",\"limit\":10}"},
    };

    for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); i++) {
        if (strcmp(fields[i].key, field) != 0) {
            continue;
        }
        const char *found = strstr(DX_VALID, fields[i].needle);
        if (!found) {
            return NULL;
        }
        size_t head = (size_t)(found - DX_VALID);
        size_t tail = strlen(DX_VALID) - head - strlen(fields[i].needle);
        char *out = (char *)malloc(head + strlen(value_json) + tail + 1);
        if (!out) {
            return NULL;
        }
        memcpy(out, DX_VALID, head);
        memcpy(out + head, value_json, strlen(value_json));
        memcpy(out + head + strlen(value_json), found + strlen(fields[i].needle), tail + 1);
        return out;
    }
    return NULL;
}

/* Walk a dotted path of object keys. Returns NULL when a step is missing or
 * is not an object. */
static yyjson_val *dx_path(yyjson_val *root, const char *path) {
    yyjson_val *cur = root;
    const char *p = path;
    while (cur && *p) {
        const char *dot = strchr(p, '.');
        size_t len = dot ? (size_t)(dot - p) : strlen(p);
        char key[64];
        if (len >= sizeof(key)) {
            return NULL;
        }
        memcpy(key, p, len);
        key[len] = '\0';
        cur = yyjson_obj_get(cur, key);
        p = dot ? dot + 1 : p + len;
    }
    return cur;
}

/* ------------------------------------------------------------------ cases */

TEST(dx_envelope_with_all_four_fields_parses) {
    dx_surface_request_t req;
    char *err = NULL;
    int rc = dx_surface_parse(DX_VALID, &req, &err);
    ASSERT_EQ(rc, 0);
    ASSERT_NULL(err);
    ASSERT_NOT_NULL(req.question);
    ASSERT_NOT_NULL(req.draft_reason);
    ASSERT_NOT_NULL(req.review);
    ASSERT_NOT_NULL(req.args_json);
    ASSERT_STR_EQ(req.question, "how does a request reach the database?");
    ASSERT_EQ(req.draft_type, DX_REQ_SEARCH);
    ASSERT_EQ(req.request_type, DX_REQ_SEARCH);
    ASSERT_STR_EQ(req.base_tool, "search_graph");
    dx_surface_request_free(&req);
    PASS();
}

TEST(dx_missing_question_is_rejected) {
    dx_surface_request_t req;
    char *err = NULL;
    char *args = dx_envelope("question", "\"question\":\"\"");
    ASSERT_NOT_NULL(args);
    int rc = dx_surface_parse(args, &req, &err);
    ASSERT_EQ(rc, -1);
    ASSERT_NOT_NULL(err);
    ASSERT_TRUE(strstr(err, "question") != NULL);
    free(err);
    free(args);
    PASS();
}

TEST(dx_missing_draft_is_rejected) {
    dx_surface_request_t req;
    char *err = NULL;
    char *args = dx_envelope("draft", "\"draft\":null");
    ASSERT_NOT_NULL(args);
    int rc = dx_surface_parse(args, &req, &err);
    ASSERT_EQ(rc, -1);
    ASSERT_NOT_NULL(err);
    ASSERT_TRUE(strstr(err, "draft") != NULL);
    free(err);
    free(args);
    PASS();
}

TEST(dx_missing_review_is_rejected) {
    dx_surface_request_t req;
    char *err = NULL;
    char *args = dx_envelope("review", "\"review\":\"\"");
    ASSERT_NOT_NULL(args);
    int rc = dx_surface_parse(args, &req, &err);
    ASSERT_EQ(rc, -1);
    ASSERT_NOT_NULL(err);
    ASSERT_TRUE(strstr(err, "review") != NULL);
    free(err);
    free(args);
    PASS();
}

TEST(dx_missing_request_is_rejected) {
    dx_surface_request_t req;
    char *err = NULL;
    char *args = dx_envelope("request", "\"request\":null");
    ASSERT_NOT_NULL(args);
    int rc = dx_surface_parse(args, &req, &err);
    ASSERT_EQ(rc, -1);
    ASSERT_NOT_NULL(err);
    ASSERT_TRUE(strstr(err, "request") != NULL);
    free(err);
    free(args);
    PASS();
}

TEST(dx_unknown_draft_type_is_rejected) {
    dx_surface_request_t req;
    char *err = NULL;
    const char *args =
        "{\"question\":\"q\",\"draft\":{\"reason\":\"r\",\"type\":\"nonsense\"},"
        "\"review\":\"v\",\"request\":{\"type\":\"search\"}}";
    int rc = dx_surface_parse(args, &req, &err);
    ASSERT_EQ(rc, -1);
    ASSERT_NOT_NULL(err);
    ASSERT_TRUE(strstr(err, "draft.type") != NULL);
    free(err);
    PASS();
}

TEST(dx_unknown_request_type_is_rejected) {
    dx_surface_request_t req;
    char *err = NULL;
    const char *args =
        "{\"question\":\"q\",\"draft\":{\"reason\":\"r\",\"type\":\"search\"},"
        "\"review\":\"v\",\"request\":{\"type\":\"nonsense\"}}";
    int rc = dx_surface_parse(args, &req, &err);
    ASSERT_EQ(rc, -1);
    ASSERT_NOT_NULL(err);
    ASSERT_TRUE(strstr(err, "request.type") != NULL);
    free(err);
    PASS();
}

TEST(dx_escape_is_a_first_class_request) {
    dx_surface_request_t req;
    char *err = NULL;
    const char *args =
        "{\"question\":\"what is the weather\",\"draft\":{\"reason\":\"not a graph question\","
        "\"type\":\"escape\"},\"review\":\"agreed\",\"request\":{\"type\":\"escape\"}}";
    int rc = dx_surface_parse(args, &req, &err);
    ASSERT_EQ(rc, 0);
    ASSERT_NULL(err);
    ASSERT_EQ(req.request_type, DX_REQ_ESCAPE);
    ASSERT_NULL(req.base_tool);
    ASSERT_TRUE(dx_surface_escape_text() != NULL);
    ASSERT_TRUE(strlen(dx_surface_escape_text()) > 0);
    dx_surface_request_free(&req);
    PASS();
}

TEST(dx_review_may_change_the_committed_type) {
    dx_surface_request_t req;
    char *err = NULL;
    const char *args =
        "{\"question\":\"who calls the handler\",\"draft\":{\"reason\":\"search first\","
        "\"type\":\"search\"},\"review\":\"a trace is the smaller answer now\","
        "\"request\":{\"type\":\"trace\",\"start\":\"handleRequest\"}}";
    int rc = dx_surface_parse(args, &req, &err);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(req.draft_type, DX_REQ_SEARCH);
    ASSERT_EQ(req.request_type, DX_REQ_TRACE);
    ASSERT_STR_EQ(req.base_tool, "trace_path");

    char *audit = dx_surface_audit_text(&req, "the indexed graph");
    ASSERT_NOT_NULL(audit);
    ASSERT_TRUE(strstr(audit, "search") != NULL);
    ASSERT_TRUE(strstr(audit, "trace") != NULL);
    free(audit);
    dx_surface_request_free(&req);
    PASS();
}

TEST(dx_type_specific_fields_are_forwarded_verbatim) {
    dx_surface_request_t req;
    char *err = NULL;
    int rc = dx_surface_parse(DX_VALID, &req, &err);
    ASSERT_EQ(rc, 0);

    yyjson_doc *doc = yyjson_read(req.args_json, strlen(req.args_json), 0);
    ASSERT_NOT_NULL(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    ASSERT_TRUE(yyjson_is_obj(root));

    yyjson_val *q = yyjson_obj_get(root, "query");
    ASSERT_TRUE(yyjson_is_str(q));
    ASSERT_STR_EQ(yyjson_get_str(q), "handleRequest");

    yyjson_val *limit = yyjson_obj_get(root, "limit");
    ASSERT_TRUE(yyjson_is_num(limit));
    ASSERT_EQ((long long)yyjson_get_num(limit), 10);

    /* "type" is ours, not the base tool's: it must not be forwarded. */
    ASSERT_NULL(yyjson_obj_get(root, "type"));

    yyjson_doc_free(doc);
    dx_surface_request_free(&req);
    PASS();
}

TEST(dx_project_is_forwarded_verbatim) {
    dx_surface_request_t req;
    char *err = NULL;
    const char *args =
        "{\"question\":\"q\",\"draft\":{\"reason\":\"r\",\"type\":\"search\"},\"review\":\"v\","
        "\"request\":{\"type\":\"search\",\"project\":\"my-project\",\"query\":\"x\"}}";
    int rc = dx_surface_parse(args, &req, &err);
    ASSERT_EQ(rc, 0);

    yyjson_doc *doc = yyjson_read(req.args_json, strlen(req.args_json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *p = yyjson_obj_get(root, "project");
    ASSERT_TRUE(yyjson_is_str(p));
    ASSERT_STR_EQ(yyjson_get_str(p), "my-project");
    yyjson_doc_free(doc);
    dx_surface_request_free(&req);
    PASS();
}

TEST(dx_include_source_is_not_injected_when_absent) {
    dx_surface_request_t req;
    char *err = NULL;
    int rc = dx_surface_parse(DX_VALID, &req, &err);
    ASSERT_EQ(rc, 0);
    ASSERT_TRUE(strstr(req.args_json, "include_source") == NULL);
    dx_surface_request_free(&req);
    PASS();
}

TEST(dx_every_known_type_maps_to_a_base_tool) {
    static const struct {
        dx_request_type_t type;
        const char *name;
        const char *tool;
    } expected[] = {
        {DX_REQ_SEARCH, "search", "search_graph"},
        {DX_REQ_TRACE, "trace", "trace_path"},
        {DX_REQ_IMPACT, "impact", "trace_path"},
        {DX_REQ_DETAILS, "details", "get_code_snippet"},
        {DX_REQ_OUTLINE, "outline", "get_file_outline"},
        {DX_REQ_ARCHITECTURE, "architecture", "get_architecture"},
        {DX_REQ_SCHEMA, "schema", "get_graph_schema"},
        {DX_REQ_COVERAGE, "coverage", "check_index_coverage"},
        {DX_REQ_CHANGES, "changes", "detect_changes"},
        {DX_REQ_ESCAPE, "escape", NULL},
    };
    ASSERT_EQ((long long)(sizeof(expected) / sizeof(expected[0])), (long long)DX_REQ_TYPE_COUNT);
    for (size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); i++) {
        ASSERT_STR_EQ(dx_surface_type_name(expected[i].type), expected[i].name);
        dx_request_type_t parsed;
        ASSERT_TRUE(dx_surface_type_from_name(expected[i].name, &parsed));
        ASSERT_EQ(parsed, expected[i].type);
        const char *tool = dx_surface_base_tool(expected[i].type);
        if (expected[i].tool) {
            ASSERT_NOT_NULL(tool);
            ASSERT_STR_EQ(tool, expected[i].tool);
        } else {
            ASSERT_NULL(tool);
        }
    }
    PASS();
}

TEST(dx_unknown_type_name_does_not_parse) {
    dx_request_type_t parsed = DX_REQ_SEARCH;
    ASSERT_FALSE(dx_surface_type_from_name("nonsense", &parsed));
    ASSERT_FALSE(dx_surface_type_from_name(NULL, &parsed));
    ASSERT_FALSE(dx_surface_type_from_name("", &parsed));
    PASS();
}

TEST(dx_schema_enum_matches_the_dispatch_table) {
    const char *schema = dx_surface_tool_schema();
    ASSERT_NOT_NULL(schema);

    yyjson_doc *doc = yyjson_read(schema, strlen(schema), 0);
    ASSERT_NOT_NULL(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    ASSERT_TRUE(yyjson_is_obj(root));

    /* The request type's enum. */
    yyjson_val *enum_val = dx_path(root, "properties.request.properties.type.enum");
    ASSERT_TRUE(yyjson_is_arr(enum_val));
    ASSERT_EQ((long long)yyjson_arr_size(enum_val), (long long)DX_REQ_TYPE_COUNT);
    for (size_t i = 0; i < (size_t)DX_REQ_TYPE_COUNT; i++) {
        yyjson_val *item = yyjson_arr_get(enum_val, i);
        ASSERT_TRUE(yyjson_is_str(item));
        ASSERT_STR_EQ(yyjson_get_str(item), dx_surface_type_name((dx_request_type_t)i));
    }

    /* The draft type's enum must be the same list. */
    yyjson_val *denum = dx_path(root, "properties.draft.properties.type.enum");
    ASSERT_TRUE(yyjson_is_arr(denum));
    ASSERT_EQ((long long)yyjson_arr_size(denum), (long long)DX_REQ_TYPE_COUNT);
    for (size_t i = 0; i < (size_t)DX_REQ_TYPE_COUNT; i++) {
        yyjson_val *item = yyjson_arr_get(denum, i);
        ASSERT_TRUE(yyjson_is_str(item));
        ASSERT_STR_EQ(yyjson_get_str(item), dx_surface_type_name((dx_request_type_t)i));
    }

    yyjson_doc_free(doc);
    PASS();
}

TEST(dx_schema_requires_the_whole_envelope) {
    const char *schema = dx_surface_tool_schema();
    yyjson_doc *doc = yyjson_read(schema, strlen(schema), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    yyjson_val *required = yyjson_obj_get(root, "required");
    ASSERT_TRUE(yyjson_is_arr(required));
    ASSERT_EQ((long long)yyjson_arr_size(required), 4);
    static const char *expect[] = {"question", "draft", "review", "request"};
    for (size_t i = 0; i < 4; i++) {
        yyjson_val *item = yyjson_arr_get(required, i);
        ASSERT_TRUE(yyjson_is_str(item));
        ASSERT_STR_EQ(yyjson_get_str(item), expect[i]);
    }
    yyjson_doc_free(doc);
    PASS();
}

TEST(dx_wrapped_result_carries_audit_and_next) {
    dx_surface_request_t req;
    char *err = NULL;
    int rc = dx_surface_parse(DX_VALID, &req, &err);
    ASSERT_EQ(rc, 0);

    char *audit = dx_surface_audit_text(&req, "the indexed graph");
    ASSERT_NOT_NULL(audit);

    const char *inner = "{\"content\":[{\"type\":\"text\",\"text\":\"three results\"}]}";
    char *wrapped = dx_surface_wrap_result(inner, audit, DX_NEXT_ANSWER, "you have enough");
    ASSERT_NOT_NULL(wrapped);

    yyjson_doc *doc = yyjson_read(wrapped, strlen(wrapped), 0);
    ASSERT_NOT_NULL(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    ASSERT_TRUE(yyjson_is_obj(root));

    /* The original payload survives. */
    ASSERT_NOT_NULL(yyjson_obj_get(root, "content"));

    yyjson_val *audit_val = yyjson_obj_get(root, "audit");
    ASSERT_TRUE(yyjson_is_str(audit_val));

    yyjson_val *next = yyjson_obj_get(root, "next");
    ASSERT_TRUE(yyjson_is_obj(next));
    yyjson_val *kind = yyjson_obj_get(next, "kind");
    ASSERT_TRUE(yyjson_is_str(kind));
    ASSERT_STR_EQ(yyjson_get_str(kind), "answer");
    yyjson_val *detail = yyjson_obj_get(next, "detail");
    ASSERT_TRUE(yyjson_is_str(detail));

    yyjson_doc_free(doc);
    free(wrapped);
    free(audit);
    dx_surface_request_free(&req);
    PASS();
}

TEST(dx_wrapped_result_rejects_a_non_object) {
    ASSERT_NULL(dx_surface_wrap_result("not json", "audit", DX_NEXT_ANSWER, "d"));
    ASSERT_NULL(dx_surface_wrap_result("[1,2,3]", "audit", DX_NEXT_ANSWER, "d"));
    ASSERT_NULL(dx_surface_wrap_result(NULL, "audit", DX_NEXT_ANSWER, "d"));
    PASS();
}

TEST(dx_next_kinds_are_the_three_agreed_values) {
    ASSERT_STR_EQ(DX_NEXT_ANSWER, "answer");
    ASSERT_STR_EQ(DX_NEXT_INSPECT, "inspect");
    ASSERT_STR_EQ(DX_NEXT_ESCAPE, "escape");
    PASS();
}

SUITE(dx_surface) {
    RUN_TEST(dx_envelope_with_all_four_fields_parses);
    RUN_TEST(dx_missing_question_is_rejected);
    RUN_TEST(dx_missing_draft_is_rejected);
    RUN_TEST(dx_missing_review_is_rejected);
    RUN_TEST(dx_missing_request_is_rejected);
    RUN_TEST(dx_unknown_draft_type_is_rejected);
    RUN_TEST(dx_unknown_request_type_is_rejected);
    RUN_TEST(dx_escape_is_a_first_class_request);
    RUN_TEST(dx_review_may_change_the_committed_type);
    RUN_TEST(dx_type_specific_fields_are_forwarded_verbatim);
    RUN_TEST(dx_project_is_forwarded_verbatim);
    RUN_TEST(dx_include_source_is_not_injected_when_absent);
    RUN_TEST(dx_every_known_type_maps_to_a_base_tool);
    RUN_TEST(dx_unknown_type_name_does_not_parse);
    RUN_TEST(dx_schema_enum_matches_the_dispatch_table);
    RUN_TEST(dx_schema_requires_the_whole_envelope);
    RUN_TEST(dx_wrapped_result_carries_audit_and_next);
    RUN_TEST(dx_wrapped_result_rejects_a_non_object);
    RUN_TEST(dx_next_kinds_are_the_three_agreed_values);
}
