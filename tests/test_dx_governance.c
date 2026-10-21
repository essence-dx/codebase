/* test_dx_governance.c - The DX governance verdict contracts.
 *
 * The rule every case here pins: **never certify what cannot be checked.**
 * An unresolved subject is "unverifiable", not "safe". A path that was already
 * reachable is not an opening. A frozen kind the baseline already accepts is
 * adopted, not a new failure.
 */
#include "test_framework.h"

#include <mcp/dx_governance.h>
#include <yyjson/yyjson.h>

#include <stdlib.h>
#include <string.h>

/* Read a string member of a parsed object. */
static const char *member_str(yyjson_val *obj, const char *key) {
    yyjson_val *v = yyjson_obj_get(obj, key);
    return yyjson_is_str(v) ? yyjson_get_str(v) : NULL;
}

/* ------------------------------------------------------------ verify_claim */

TEST(dx_a_resolved_claim_with_a_citation_is_confirmed) {
    char *out = dx_governance_verify_claim("process() calls validate()", true,
                                            "src/app.py:42", NULL);
    ASSERT_NOT_NULL(out);
    yyjson_doc *doc = yyjson_read(out, strlen(out), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    ASSERT_STR_EQ(member_str(root, "verdict"), DX_VERDICT_CONFIRMED);
    ASSERT_STR_EQ(member_str(root, "citation"), "src/app.py:42");
    ASSERT_NOT_NULL(yyjson_obj_get(root, "receipt"));
    yyjson_doc_free(doc);
    free(out);
    PASS();
}

TEST(dx_an_unresolved_claim_is_unverifiable_not_refuted) {
    char *out = dx_governance_verify_claim("process() calls validate()", false, NULL, NULL);
    ASSERT_NOT_NULL(out);
    yyjson_doc *doc = yyjson_read(out, strlen(out), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    ASSERT_STR_EQ(member_str(root, "verdict"), DX_VERDICT_UNVERIFIABLE);
    /* The reason must say this is not evidence of falsehood. */
    const char *reason = member_str(root, "reason");
    ASSERT_NOT_NULL(reason);
    ASSERT_TRUE(strstr(reason, "not evidence") != NULL);
    yyjson_doc_free(doc);
    free(out);
    PASS();
}

TEST(dx_a_resolved_but_uncited_claim_is_unverifiable) {
    char *out = dx_governance_verify_claim("process() calls validate()", true, NULL, NULL);
    yyjson_doc *doc = yyjson_read(out, strlen(out), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    ASSERT_STR_EQ(member_str(root, "verdict"), DX_VERDICT_UNVERIFIABLE);
    yyjson_doc_free(doc);
    free(out);
    PASS();
}

TEST(dx_the_receipt_carries_the_claim_the_verdict_and_the_citation) {
    char *out = dx_governance_verify_claim("a claim", true, "p.c:1", NULL);
    yyjson_doc *doc = yyjson_read(out, strlen(out), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *receipt = yyjson_obj_get(root, "receipt");
    ASSERT_TRUE(yyjson_is_obj(receipt));
    ASSERT_STR_EQ(member_str(receipt, "claim"), "a claim");
    ASSERT_STR_EQ(member_str(receipt, "verdict"), DX_VERDICT_CONFIRMED);
    ASSERT_STR_EQ(member_str(receipt, "citation"), "p.c:1");
    yyjson_doc_free(doc);
    free(out);
    PASS();
}

/* ------------------------------------------------- certify_public_surface */

TEST(dx_each_export_gets_a_verdict_and_its_consumers) {
    const char *exports =
        "[{\"qualified_name\":\"a.b\",\"kind\":\"function\","
        "\"compat\":\"breaking\","
        "\"consumers\":[{\"path\":\"src/c.c\",\"symbol\":\"caller\",\"line\":7,\"via\":\"CALLS\"}]},"
        "{\"qualified_name\":\"a.c\",\"compat\":\"non-breaking\"}]";
    char *out = dx_governance_certify_surface(exports);
    ASSERT_NOT_NULL(out);
    yyjson_doc *doc = yyjson_read(out, strlen(out), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    yyjson_val *list = yyjson_obj_get(root, "exports");
    ASSERT_EQ((long long)yyjson_arr_size(list), 2);
    yyjson_val *first = yyjson_arr_get(list, 0);
    ASSERT_STR_EQ(member_str(first, "verdict"), DX_COMPAT_BREAKING);
    ASSERT_EQ(yyjson_get_int(yyjson_obj_get(first, "consumer_count")), 1);
    yyjson_val *consumers = yyjson_obj_get(first, "consumers");
    ASSERT_EQ((long long)yyjson_arr_size(consumers), 1);

    yyjson_val *summary = yyjson_obj_get(root, "summary");
    ASSERT_EQ(yyjson_get_int(yyjson_obj_get(summary, "breaking")), 1);
    ASSERT_EQ(yyjson_get_int(yyjson_obj_get(summary, "non-breaking")), 1);
    ASSERT_EQ(yyjson_get_int(yyjson_obj_get(summary, "consumer_count")), 1);
    yyjson_doc_free(doc);
    free(out);
    PASS();
}

TEST(dx_an_unknown_compat_is_potentially_breaking_never_safe) {
    const char *exports = "[{\"qualified_name\":\"a.b\",\"compat\":\"who-knows\"}]";
    char *out = dx_governance_certify_surface(exports);
    yyjson_doc *doc = yyjson_read(out, strlen(out), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *first = yyjson_arr_get(yyjson_obj_get(root, "exports"), 0);
    ASSERT_STR_EQ(member_str(first, "verdict"), DX_COMPAT_POTENTIALLY_BREAKING);
    yyjson_doc_free(doc);
    free(out);
    PASS();
}

TEST(dx_a_missing_compat_is_potentially_breaking) {
    const char *exports = "[{\"qualified_name\":\"a.b\"}]";
    char *out = dx_governance_certify_surface(exports);
    yyjson_doc *doc = yyjson_read(out, strlen(out), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *first = yyjson_arr_get(yyjson_obj_get(root, "exports"), 0);
    ASSERT_STR_EQ(member_str(first, "verdict"), DX_COMPAT_POTENTIALLY_BREAKING);
    yyjson_doc_free(doc);
    free(out);
    PASS();
}

TEST(dx_a_breaking_export_with_no_known_consumer_is_still_breaking) {
    const char *exports = "[{\"qualified_name\":\"a.b\",\"compat\":\"breaking\"}]";
    char *out = dx_governance_certify_surface(exports);
    yyjson_doc *doc = yyjson_read(out, strlen(out), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *first = yyjson_arr_get(yyjson_obj_get(root, "exports"), 0);
    ASSERT_STR_EQ(member_str(first, "verdict"), DX_COMPAT_BREAKING);
    ASSERT_EQ(yyjson_get_int(yyjson_obj_get(first, "consumer_count")), 0);
    yyjson_doc_free(doc);
    free(out);
    PASS();
}

TEST(dx_certify_rejects_a_non_array) {
    ASSERT_NULL(dx_governance_certify_surface("{\"a\":1}"));
    ASSERT_NULL(dx_governance_certify_surface("not json"));
    ASSERT_NULL(dx_governance_certify_surface(NULL));
    PASS();
}

/* ----------------------------------------------- change_impact_certificate */

TEST(dx_a_before_that_is_not_an_array_is_refused_not_read_as_empty) {
    /* "before" that is not an array of paths must not be read as "nothing was
     * reachable before". That reading would make every path reachable after
     * and inside a boundary look NEWLY OPENED - the maximal verdict, produced
     * by a malformed input. "after" was already type-checked; "before" was
     * not, and the failure was silent. */
    const char *bounds = "[{\"name\":\"auth\",\"path_prefix\":\"src/auth\"}]";
    ASSERT_NULL(dx_governance_certificate("\"garbage\"", "[\"src/auth/token.c\"]", bounds));
    ASSERT_NULL(dx_governance_certificate("{\"not\":\"an array\"}", "[\"src/auth/token.c\"]", bounds));
    ASSERT_NULL(dx_governance_certificate("42", "[\"src/auth/token.c\"]", bounds));
    PASS();
}

TEST(dx_a_malformed_boundary_list_is_refused_not_read_as_no_boundaries) {
    /* A boundary list the code cannot read means NO boundaries, and no
     * boundaries mean nothing is opened, and nothing opened means the
     * certificate says CLEAN. A malformed boundary list would silently
     * switch off the very check it exists to perform. */
    const char *before = "[\"src/other.c\"]";
    const char *after = "[\"src/auth/token.c\"]";
    ASSERT_NULL(dx_governance_certificate(before, after, "\"garbage\""));
    ASSERT_NULL(dx_governance_certificate(before, after, "{\"not\":\"an array\"}"));
    ASSERT_NULL(dx_governance_certificate(before, after, "42"));
    /* An ABSENT boundary list is not malformed: the empty array is the
     * absent case, and it is accepted. */
    char *clean = dx_governance_certificate(before, after, "[]");
    ASSERT_NOT_NULL(clean);
    free(clean);
    PASS();
}

TEST(dx_a_malformed_baseline_is_refused_not_read_as_an_empty_baseline) {
    /* A baseline the code cannot read freezes NOTHING, so every frozen
     * finding counts as newly frozen and the gate holds - a build fails for
     * a reason its author cannot see. */
    const char *findings =
        "[{\"action\":\"frozen\",\"kind\":\"api_surface\",\"file\":\"src/a.c\"}]";
    ASSERT_NULL(dx_governance_enforce(findings, "\"garbage\""));
    ASSERT_NULL(dx_governance_enforce(findings, "{\"not\":\"an array\"}"));
    ASSERT_NULL(dx_governance_enforce(findings, "42"));
    /* An absent baseline is not malformed. */
    char *gate = dx_governance_enforce(findings, NULL);
    ASSERT_NOT_NULL(gate);
    free(gate);
    PASS();
}

TEST(dx_a_well_formed_baseline_still_adopts_the_debt) {
    /* The established contract: "held" is true when NOTHING blocks - a
     * blocking finding or a newly frozen one holds the gate, and adopted
     * debt does not. */
    const char *findings =
        "[{\"id\":\"f1\",\"message\":\"old debt\",\"action\":\"frozen\",\"kind\":\"api_surface\"}]";
    const char *baseline =
        "[{\"id\":\"b1\",\"message\":\"accepted\",\"action\":\"frozen\",\"kind\":\"api_surface\"}]";
    char *gate = dx_governance_enforce(findings, baseline);
    ASSERT_NOT_NULL(gate);
    yyjson_doc *doc = yyjson_read(gate, strlen(gate), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    ASSERT_TRUE(yyjson_get_bool(yyjson_obj_get(root, "held")));
    ASSERT_EQ((long long)yyjson_arr_size(yyjson_obj_get(root, "frozen_accepted")), 1);
    ASSERT_EQ((long long)yyjson_arr_size(yyjson_obj_get(root, "frozen_new")), 0);
    yyjson_doc_free(doc);
    free(gate);
    PASS();
}

TEST(dx_a_well_formed_certificate_still_works) {
    /* The guard must not break the ordinary case. */
    const char *bounds = "[{\"name\":\"auth\",\"path_prefix\":\"src/auth\"}]";
    char *out = dx_governance_certificate("[\"src/other.c\"]", "[\"src/auth/token.c\"]", bounds);
    ASSERT_NOT_NULL(out);
    yyjson_doc *doc = yyjson_read(out, strlen(out), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    ASSERT_STR_EQ(yyjson_get_str(yyjson_obj_get(root, "verdict")), "opened");
    yyjson_doc_free(doc);
    free(out);
    PASS();
}

TEST(dx_a_newly_reachable_path_inside_a_boundary_is_an_opening) {
    const char *before = "[\"src/other.c\"]";
    const char *after = "[\"src/other.c\",\"src/auth/token.c\"]";
    const char *bounds = "[{\"name\":\"auth\",\"path_prefix\":\"src/auth\"}]";
    char *out = dx_governance_certificate(before, after, bounds);
    ASSERT_NOT_NULL(out);
    yyjson_doc *doc = yyjson_read(out, strlen(out), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    ASSERT_STR_EQ(member_str(root, "verdict"), "opened");
    yyjson_val *opened = yyjson_obj_get(root, "opened");
    ASSERT_EQ((long long)yyjson_arr_size(opened), 1);
    ASSERT_STR_EQ(member_str(yyjson_arr_get(opened, 0), "path"), "src/auth/token.c");
    ASSERT_STR_EQ(member_str(yyjson_arr_get(opened, 0), "boundary"), "auth");
    yyjson_doc_free(doc);
    free(out);
    PASS();
}

TEST(dx_a_path_that_was_already_reachable_is_not_an_opening) {
    /* Nothing new became reachable, so nothing was opened. */
    const char *before = "[\"src/auth/token.c\"]";
    const char *after = "[\"src/auth/token.c\"]";
    const char *bounds = "[{\"name\":\"auth\",\"path_prefix\":\"src/auth\"}]";
    char *out = dx_governance_certificate(before, after, bounds);
    yyjson_doc *doc = yyjson_read(out, strlen(out), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    ASSERT_STR_EQ(member_str(root, "verdict"), "clean");
    ASSERT_EQ((long long)yyjson_arr_size(yyjson_obj_get(root, "opened")), 0);
    yyjson_doc_free(doc);
    free(out);
    PASS();
}

TEST(dx_only_the_new_path_counts_when_a_boundary_gains_one) {
    /* The already-reachable path is not counted; the new one is. */
    const char *before = "[\"src/auth/token.c\"]";
    const char *after = "[\"src/auth/token.c\",\"src/auth/other.c\"]";
    const char *bounds = "[{\"name\":\"auth\",\"path_prefix\":\"src/auth\"}]";
    char *out = dx_governance_certificate(before, after, bounds);
    yyjson_doc *doc = yyjson_read(out, strlen(out), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    ASSERT_STR_EQ(member_str(root, "verdict"), "opened");
    yyjson_val *opened = yyjson_obj_get(root, "opened");
    ASSERT_EQ((long long)yyjson_arr_size(opened), 1);
    ASSERT_STR_EQ(member_str(yyjson_arr_get(opened, 0), "path"), "src/auth/other.c");
    yyjson_doc_free(doc);
    free(out);
    PASS();
}

TEST(dx_a_new_path_outside_every_boundary_is_not_an_opening) {
    const char *before = "[]";
    const char *after = "[\"src/util/helper.c\"]";
    const char *bounds = "[{\"name\":\"auth\",\"path_prefix\":\"src/auth\"}]";
    char *out = dx_governance_certificate(before, after, bounds);
    yyjson_doc *doc = yyjson_read(out, strlen(out), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    ASSERT_STR_EQ(member_str(root, "verdict"), "clean");
    yyjson_doc_free(doc);
    free(out);
    PASS();
}

TEST(dx_a_boundary_prefix_matches_on_a_segment_not_a_substring) {
    /* "/src/api" must match "/src/api/x.c" and NOT "/src/apix.c". */
    const char *before = "[]";
    const char *after = "[\"/src/apix.c\"]";
    const char *bounds = "[{\"name\":\"api\",\"path_prefix\":\"/src/api\"}]";
    char *out = dx_governance_certificate(before, after, bounds);
    yyjson_doc *doc = yyjson_read(out, strlen(out), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    ASSERT_STR_EQ(member_str(root, "verdict"), "clean");
    yyjson_doc_free(doc);
    free(out);
    PASS();
}

TEST(dx_a_boundary_with_no_prefix_covers_the_whole_tree) {
    const char *before = "[]";
    const char *after = "[\"anything/at/all.c\"]";
    const char *bounds = "[{\"name\":\"all\",\"path_prefix\":\"/\"}]";
    char *out = dx_governance_certificate(before, after, bounds);
    yyjson_doc *doc = yyjson_read(out, strlen(out), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    ASSERT_STR_EQ(member_str(root, "verdict"), "opened");
    yyjson_doc_free(doc);
    free(out);
    PASS();
}

TEST(dx_no_boundaries_means_nothing_is_opened) {
    const char *before = "[]";
    const char *after = "[\"src/auth/token.c\"]";
    char *out = dx_governance_certificate(before, after, "[]");
    yyjson_doc *doc = yyjson_read(out, strlen(out), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    ASSERT_STR_EQ(member_str(root, "verdict"), "clean");
    yyjson_doc_free(doc);
    free(out);
    PASS();
}

/* ------------------------------------------------------------------ enforce */

TEST(dx_a_blocking_finding_holds_the_gate) {
    const char *findings = "[{\"id\":\"f1\",\"message\":\"opened a path\",\"action\":\"blocking\"}]";
    char *out = dx_governance_enforce(findings, "[]");
    ASSERT_NOT_NULL(out);
    yyjson_doc *doc = yyjson_read(out, strlen(out), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    ASSERT_FALSE(yyjson_get_bool(yyjson_obj_get(root, "held")));
    ASSERT_EQ(yyjson_get_int(yyjson_obj_get(root, "blocking_count")), 1);
    ASSERT_EQ((long long)yyjson_arr_size(yyjson_obj_get(root, "blocking")), 1);
    yyjson_doc_free(doc);
    free(out);
    PASS();
}

TEST(dx_a_frozen_finding_the_baseline_already_accepts_is_adopted_not_new) {
    const char *findings =
        "[{\"id\":\"f1\",\"message\":\"old debt\",\"action\":\"frozen\",\"kind\":\"dead-code\"}]";
    const char *baseline =
        "[{\"id\":\"b1\",\"message\":\"accepted\",\"action\":\"frozen\",\"kind\":\"dead-code\"}]";
    char *out = dx_governance_enforce(findings, baseline);
    yyjson_doc *doc = yyjson_read(out, strlen(out), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    /* Adopted debt does not fail the build. */
    ASSERT_TRUE(yyjson_get_bool(yyjson_obj_get(root, "held")));
    ASSERT_EQ((long long)yyjson_arr_size(yyjson_obj_get(root, "frozen_accepted")), 1);
    ASSERT_EQ((long long)yyjson_arr_size(yyjson_obj_get(root, "frozen_new")), 0);
    yyjson_doc_free(doc);
    free(out);
    PASS();
}

TEST(dx_a_frozen_finding_of_a_new_kind_holds_the_gate) {
    const char *findings =
        "[{\"id\":\"f1\",\"message\":\"new debt\",\"action\":\"frozen\",\"kind\":\"dead-code\"}]";
    const char *baseline =
        "[{\"id\":\"b1\",\"message\":\"accepted\",\"action\":\"frozen\",\"kind\":\"other\"}]";
    char *out = dx_governance_enforce(findings, baseline);
    yyjson_doc *doc = yyjson_read(out, strlen(out), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    ASSERT_FALSE(yyjson_get_bool(yyjson_obj_get(root, "held")));
    ASSERT_EQ((long long)yyjson_arr_size(yyjson_obj_get(root, "frozen_new")), 1);
    yyjson_doc_free(doc);
    free(out);
    PASS();
}

TEST(dx_an_advisory_finding_does_not_hold_the_gate) {
    const char *findings = "[{\"id\":\"f1\",\"message\":\"note\",\"action\":\"advisory\"}]";
    char *out = dx_governance_enforce(findings, "[]");
    yyjson_doc *doc = yyjson_read(out, strlen(out), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    ASSERT_TRUE(yyjson_get_bool(yyjson_obj_get(root, "held")));
    ASSERT_EQ((long long)yyjson_arr_size(yyjson_obj_get(root, "advisory")), 1);
    yyjson_doc_free(doc);
    free(out);
    PASS();
}

TEST(dx_an_unknown_action_is_advisory_and_never_blocks) {
    const char *findings = "[{\"id\":\"f1\",\"message\":\"mystery\",\"action\":\"who-knows\"}]";
    char *out = dx_governance_enforce(findings, "[]");
    yyjson_doc *doc = yyjson_read(out, strlen(out), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    /* An unknown policy must not fail a build. */
    ASSERT_TRUE(yyjson_get_bool(yyjson_obj_get(root, "held")));
    ASSERT_EQ((long long)yyjson_arr_size(yyjson_obj_get(root, "advisory")), 1);
    yyjson_doc_free(doc);
    free(out);
    PASS();
}

TEST(dx_an_off_finding_is_not_reported_as_a_failure) {
    const char *findings = "[{\"id\":\"f1\",\"message\":\"muted\",\"action\":\"off\"}]";
    char *out = dx_governance_enforce(findings, "[]");
    yyjson_doc *doc = yyjson_read(out, strlen(out), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    ASSERT_TRUE(yyjson_get_bool(yyjson_obj_get(root, "held")));
    ASSERT_EQ((long long)yyjson_arr_size(yyjson_obj_get(root, "off")), 1);
    yyjson_doc_free(doc);
    free(out);
    PASS();
}

TEST(dx_an_empty_finding_set_holds_the_gate) {
    char *out = dx_governance_enforce("[]", "[]");
    yyjson_doc *doc = yyjson_read(out, strlen(out), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    ASSERT_TRUE(yyjson_get_bool(yyjson_obj_get(root, "held")));
    yyjson_doc_free(doc);
    free(out);
    PASS();
}

SUITE(dx_governance) {
    RUN_TEST(dx_a_resolved_claim_with_a_citation_is_confirmed);
    RUN_TEST(dx_an_unresolved_claim_is_unverifiable_not_refuted);
    RUN_TEST(dx_a_resolved_but_uncited_claim_is_unverifiable);
    RUN_TEST(dx_the_receipt_carries_the_claim_the_verdict_and_the_citation);
    RUN_TEST(dx_each_export_gets_a_verdict_and_its_consumers);
    RUN_TEST(dx_an_unknown_compat_is_potentially_breaking_never_safe);
    RUN_TEST(dx_a_missing_compat_is_potentially_breaking);
    RUN_TEST(dx_a_breaking_export_with_no_known_consumer_is_still_breaking);
    RUN_TEST(dx_certify_rejects_a_non_array);
    RUN_TEST(dx_a_before_that_is_not_an_array_is_refused_not_read_as_empty);
    RUN_TEST(dx_a_malformed_boundary_list_is_refused_not_read_as_no_boundaries);
    RUN_TEST(dx_a_malformed_baseline_is_refused_not_read_as_an_empty_baseline);
    RUN_TEST(dx_a_well_formed_baseline_still_adopts_the_debt);
    RUN_TEST(dx_a_well_formed_certificate_still_works);
    RUN_TEST(dx_a_newly_reachable_path_inside_a_boundary_is_an_opening);
    RUN_TEST(dx_a_path_that_was_already_reachable_is_not_an_opening);
    RUN_TEST(dx_only_the_new_path_counts_when_a_boundary_gains_one);
    RUN_TEST(dx_a_new_path_outside_every_boundary_is_not_an_opening);
    RUN_TEST(dx_a_boundary_prefix_matches_on_a_segment_not_a_substring);
    RUN_TEST(dx_a_boundary_with_no_prefix_covers_the_whole_tree);
    RUN_TEST(dx_no_boundaries_means_nothing_is_opened);
    RUN_TEST(dx_a_blocking_finding_holds_the_gate);
    RUN_TEST(dx_a_frozen_finding_the_baseline_already_accepts_is_adopted_not_new);
    RUN_TEST(dx_a_frozen_finding_of_a_new_kind_holds_the_gate);
    RUN_TEST(dx_an_advisory_finding_does_not_hold_the_gate);
    RUN_TEST(dx_an_unknown_action_is_advisory_and_never_blocks);
    RUN_TEST(dx_an_off_finding_is_not_reported_as_a_failure);
    RUN_TEST(dx_an_empty_finding_set_holds_the_gate);
}
