/* test_dx_memory.c - The anchored-memory contract.
 *
 * The rule every case here pins: a memory that cannot be checked is reported
 * as unanchored, never as valid. "gone" and "unanchored" are different
 * answers and are never merged.
 */
#include "test_framework.h"

#include <mcp/dx_memory.h>
#include <yyjson/yyjson.h>

#include <stdlib.h>
#include <string.h>

/* A string member of a parsed object, or NULL. */
static const char *member_str(yyjson_val *obj, const char *key) {
    yyjson_val *v = yyjson_obj_get(obj, key);
    return yyjson_is_str(v) ? yyjson_get_str(v) : NULL;
}

/* ------------------------------------------------------------------ hash */

TEST(dx_the_hash_is_stable_and_hexadecimal) {
    char *a = dx_memory_hash("int main(void) { return 0; }");
    char *b = dx_memory_hash("int main(void) { return 0; }");
    ASSERT_NOT_NULL(a);
    ASSERT_NOT_NULL(b);
    ASSERT_STR_EQ(a, b);
    ASSERT_EQ((long long)strlen(a), 16);
    for (const char *p = a; *p; p++) {
        ASSERT_TRUE((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f'));
    }
    free(a);
    free(b);
    PASS();
}

TEST(dx_different_text_hashes_differently) {
    char *a = dx_memory_hash("one");
    char *b = dx_memory_hash("two");
    ASSERT_TRUE(strcmp(a, b) != 0);
    free(a);
    free(b);
    PASS();
}

TEST(dx_the_hash_of_nothing_is_nothing) {
    ASSERT_NULL(dx_memory_hash(NULL));
    PASS();
}

/* ------------------------------------------------------------- the state */

TEST(dx_an_unchanged_anchor_is_valid) {
    char *h = dx_memory_hash("the same text");
    const char *state = dx_memory_anchor_state(h, "the same text", false, false);
    ASSERT_STR_EQ(state, DX_ANCHOR_VALID);
    free(h);
    PASS();
}

TEST(dx_a_changed_anchor_is_changed) {
    char *h = dx_memory_hash("the old text");
    const char *state = dx_memory_anchor_state(h, "the new text", false, false);
    ASSERT_STR_EQ(state, DX_ANCHOR_CHANGED);
    free(h);
    PASS();
}

TEST(dx_a_missing_anchor_is_gone) {
    char *h = dx_memory_hash("whatever");
    const char *state = dx_memory_anchor_state(h, NULL, true, false);
    ASSERT_STR_EQ(state, DX_ANCHOR_GONE);
    free(h);
    PASS();
}

TEST(dx_an_unreadable_anchor_is_unanchored_never_valid) {
    char *h = dx_memory_hash("whatever");
    const char *state = dx_memory_anchor_state(h, NULL, false, true);
    ASSERT_STR_EQ(state, DX_ANCHOR_UNANCHORED);
    free(h);
    PASS();
}

TEST(dx_gone_and_unanchored_are_different_answers) {
    char *h = dx_memory_hash("whatever");
    const char *gone = dx_memory_anchor_state(h, NULL, true, false);
    const char *unanchored = dx_memory_anchor_state(h, NULL, false, true);
    ASSERT_TRUE(strcmp(gone, unanchored) != 0);
    free(h);
    PASS();
}

TEST(dx_a_memory_with_no_recorded_hash_cannot_be_certified) {
    const char *state = dx_memory_anchor_state(NULL, "some text", false, false);
    ASSERT_STR_EQ(state, DX_ANCHOR_UNANCHORED);
    PASS();
}

/* ---------------------------------------------------------------- recall */

TEST(dx_recall_reports_each_memory_with_its_state) {
    char *h = dx_memory_hash("unchanged");
    char memories[512];
    snprintf(memories, sizeof(memories),
             "[{\"id\":\"m1\",\"kind\":\"decision\",\"title\":\"t\",\"body\":\"b\","
             "\"anchor\":{\"hash\":\"%s\",\"current\":\"unchanged\",\"missing\":false,"
             "\"unreadable\":false}},"
             "{\"id\":\"m2\",\"kind\":\"decision\",\"title\":\"t\",\"body\":\"b\","
             "\"anchor\":{\"hash\":\"%s\",\"current\":\"different\",\"missing\":false,"
             "\"unreadable\":false}},"
             "{\"id\":\"m3\",\"kind\":\"decision\",\"title\":\"t\",\"body\":\"b\","
             "\"anchor\":{\"hash\":\"%s\",\"current\":null,\"missing\":true,\"unreadable\":false}},"
             "{\"id\":\"m4\",\"kind\":\"decision\",\"title\":\"t\",\"body\":\"b\"}]",
             h, h, h);
    char *out = dx_memory_recall(memories);
    ASSERT_NOT_NULL(out);
    yyjson_doc *doc = yyjson_read(out, strlen(out), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    yyjson_val *list = yyjson_obj_get(root, "memories");
    ASSERT_EQ((long long)yyjson_arr_size(list), 4);
    ASSERT_STR_EQ(member_str(yyjson_arr_get(list, 0), "state"), DX_ANCHOR_VALID);
    ASSERT_STR_EQ(member_str(yyjson_arr_get(list, 1), "state"), DX_ANCHOR_CHANGED);
    ASSERT_STR_EQ(member_str(yyjson_arr_get(list, 2), "state"), DX_ANCHOR_GONE);
    ASSERT_STR_EQ(member_str(yyjson_arr_get(list, 3), "state"), DX_ANCHOR_UNANCHORED);

    /* The original fields survive: a memory is reported with its state, not
     * replaced by it. */
    ASSERT_STR_EQ(member_str(yyjson_arr_get(list, 0), "id"), "m1");
    ASSERT_STR_EQ(member_str(yyjson_arr_get(list, 0), "kind"), "decision");

    /* Every memory carries a reason. A verdict without a reason is a claim. */
    for (size_t i = 0; i < 4; i++) {
        const char *reason = member_str(yyjson_arr_get(list, i), "reason");
        ASSERT_NOT_NULL(reason);
        ASSERT_TRUE(strlen(reason) > 0);
    }

    yyjson_val *summary = yyjson_obj_get(root, "summary");
    ASSERT_EQ(yyjson_get_int(yyjson_obj_get(summary, "valid")), 1);
    ASSERT_EQ(yyjson_get_int(yyjson_obj_get(summary, "changed")), 1);
    ASSERT_EQ(yyjson_get_int(yyjson_obj_get(summary, "gone")), 1);
    ASSERT_EQ(yyjson_get_int(yyjson_obj_get(summary, "unanchored")), 1);

    yyjson_doc_free(doc);
    free(out);
    free(h);
    PASS();
}

TEST(dx_recall_of_nothing_is_an_empty_summary) {
    char *out = dx_memory_recall("[]");
    ASSERT_NOT_NULL(out);
    yyjson_doc *doc = yyjson_read(out, strlen(out), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    ASSERT_EQ((long long)yyjson_arr_size(yyjson_obj_get(root, "memories")), 0);
    yyjson_val *summary = yyjson_obj_get(root, "summary");
    ASSERT_EQ(yyjson_get_int(yyjson_obj_get(summary, "valid")), 0);
    ASSERT_EQ(yyjson_get_int(yyjson_obj_get(summary, "unanchored")), 0);
    yyjson_doc_free(doc);
    free(out);
    PASS();
}

TEST(dx_recall_rejects_a_non_array) {
    ASSERT_NULL(dx_memory_recall("{\"a\":1}"));
    ASSERT_NULL(dx_memory_recall("not json"));
    ASSERT_NULL(dx_memory_recall(NULL));
    PASS();
}

SUITE(dx_memory) {
    RUN_TEST(dx_the_hash_is_stable_and_hexadecimal);
    RUN_TEST(dx_different_text_hashes_differently);
    RUN_TEST(dx_the_hash_of_nothing_is_nothing);
    RUN_TEST(dx_an_unchanged_anchor_is_valid);
    RUN_TEST(dx_a_changed_anchor_is_changed);
    RUN_TEST(dx_a_missing_anchor_is_gone);
    RUN_TEST(dx_an_unreadable_anchor_is_unanchored_never_valid);
    RUN_TEST(dx_gone_and_unanchored_are_different_answers);
    RUN_TEST(dx_a_memory_with_no_recorded_hash_cannot_be_certified);
    RUN_TEST(dx_recall_reports_each_memory_with_its_state);
    RUN_TEST(dx_recall_of_nothing_is_an_empty_summary);
    RUN_TEST(dx_recall_rejects_a_non_array);
}
