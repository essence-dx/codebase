/* dx_governance.c - The DX governance verdicts.
 *
 * See dx_governance.h for the contract. The module is the POLICY; the graph
 * access lives in the caller. It depends on yyjson and the C standard library
 * only, so every verdict below is verifiable without a store.
 */
#include "mcp/dx_governance.h"

#include <yyjson/yyjson.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ utils */

/* Copy a JSON value into a mutable document. Returns NULL on failure. */
static yyjson_mut_val *dx_copy_val(yyjson_mut_doc *doc, yyjson_val *val) {
    if (!val) {
        return NULL;
    }
    return yyjson_val_mut_copy(doc, val);
}

/* Add a string member, or a member with an explicit null when the string is
 * absent. A key that is present but null says "we looked and there was
 * none"; a key that is missing says "we did not look". */
static void dx_add_str_or_null(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key,
                               const char *value) {
    if (value && value[0]) {
        (void)yyjson_mut_obj_add_str(doc, obj, key, value);
    } else {
        (void)yyjson_mut_obj_add_null(doc, obj, key);
    }
}

/* A string member of a JSON object, or NULL. */
static const char *dx_get_str(yyjson_val *obj, const char *key) {
    yyjson_val *v = yyjson_obj_get(obj, key);
    if (!yyjson_is_str(v)) {
        return NULL;
    }
    return yyjson_get_str(v);
}

/* Does a path fall inside a boundary prefix? A prefix of "" or "/" matches
 * everything, which is how a caller says "the whole repository is a
 * boundary". A prefix match is on path segments, so "/src/api" matches
 * "/src/api/x.c" and not "/src/apix.c". */
static bool dx_path_in_boundary(const char *path, const char *prefix) {
    if (!path) {
        return false;
    }
    if (!prefix || !prefix[0] || strcmp(prefix, "/") == 0) {
        return true;
    }
    size_t len = strlen(prefix);
    if (strncmp(path, prefix, len) != 0) {
        return false;
    }
    if (path[len] == '\0') {
        return true;
    }
    /* The prefix must end on a segment boundary. */
    if (prefix[len - 1] == '/') {
        return true;
    }
    return path[len] == '/';
}

/* ---------------------------------------------------------- verify_claim */

char *dx_governance_verify_claim(const char *claim, bool resolved, const char *citation,
                                  const char *reason) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    if (!doc) {
        return NULL;
    }
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);

    const char *verdict;
    const char *why;
    if (!resolved) {
        /* The graph does not know. That is not a refutation, and it is not a
         * confirmation either. */
        verdict = DX_VERDICT_UNVERIFIABLE;
        why = reason && reason[0]
                  ? reason
                  : "The graph does not contain the subject of the claim, so the claim can be "
                    "neither confirmed nor refuted. This is not evidence that the claim is false.";
    } else if (citation && citation[0]) {
        verdict = DX_VERDICT_CONFIRMED;
        why = reason && reason[0] ? reason
                                  : "The graph resolves the subject of the claim and the evidence "
                                    "is cited.";
    } else {
        /* Resolved but uncited: the claim is not backed by a location. */
        verdict = DX_VERDICT_UNVERIFIABLE;
        why = reason && reason[0]
                  ? reason
                  : "The subject resolves but no evidence was located, so the claim is not "
                    "backed by a citation.";
    }

    dx_add_str_or_null(doc, root, "claim", claim);
    (void)yyjson_mut_obj_add_str(doc, root, "verdict", verdict);
    dx_add_str_or_null(doc, root, "citation", citation);
    dx_add_str_or_null(doc, root, "reason", why);

    /* The receipt: the claim, the verdict and the citation, so a decision can
     * be quoted without re-deriving it. */
    yyjson_mut_val *receipt = yyjson_mut_obj(doc);
    dx_add_str_or_null(doc, receipt, "claim", claim);
    (void)yyjson_mut_obj_add_str(doc, receipt, "verdict", verdict);
    dx_add_str_or_null(doc, receipt, "citation", citation);
    (void)yyjson_mut_obj_add_val(doc, root, "receipt", receipt);

    char *out = yyjson_mut_write(doc, 0, NULL);
    yyjson_mut_doc_free(doc);
    return out;
}

/* -------------------------------------------------- certify_public_surface */

char *dx_governance_certify_surface(const char *exports_json) {
    yyjson_doc *doc = exports_json ? yyjson_read(exports_json, strlen(exports_json), 0) : NULL;
    yyjson_val *root = doc ? yyjson_doc_get_root(doc) : NULL;
    if (!yyjson_is_arr(root)) {
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
    yyjson_mut_val *out_exports = yyjson_mut_arr(mdoc);

    int breaking = 0;
    int non_breaking = 0;
    int potentially_breaking = 0;
    int total_consumers = 0;

    size_t idx;
    size_t max;
    yyjson_val *item;
    yyjson_arr_foreach(root, idx, max, item) {
        if (!yyjson_is_obj(item)) {
            continue;
        }
        const char *compat = dx_get_str(item, "compat");
        const char *verdict = compat ? compat : DX_COMPAT_POTENTIALLY_BREAKING;

        /* An export whose compat is not one of the three is not certified as
         * safe: it is treated as potentially breaking, because the whole point
         * of the verdict is that nothing is called safe on a shrug. */
        if (strcmp(verdict, DX_COMPAT_BREAKING) == 0) {
            breaking++;
        } else if (strcmp(verdict, DX_COMPAT_NON_BREAKING) == 0) {
            non_breaking++;
        } else {
            verdict = DX_COMPAT_POTENTIALLY_BREAKING;
            potentially_breaking++;
        }

        yyjson_mut_val *entry = yyjson_mut_obj(mdoc);
        dx_add_str_or_null(mdoc, entry, "qualified_name", dx_get_str(item, "qualified_name"));
        dx_add_str_or_null(mdoc, entry, "kind", dx_get_str(item, "kind"));
        (void)yyjson_mut_obj_add_str(mdoc, entry, "verdict", verdict);

        yyjson_val *consumers = yyjson_obj_get(item, "consumers");
        int consumer_count = 0;
        if (yyjson_is_arr(consumers)) {
            yyjson_mut_val *mconsumers = yyjson_mut_arr(mdoc);
            size_t ci;
            size_t cmax;
            yyjson_val *citem;
            yyjson_arr_foreach(consumers, ci, cmax, citem) {
                if (!yyjson_is_obj(citem)) {
                    continue;
                }
                yyjson_mut_val *copy = dx_copy_val(mdoc, citem);
                if (copy) {
                    (void)yyjson_mut_arr_add_val(mconsumers, copy);
                    consumer_count++;
                }
            }
            (void)yyjson_mut_obj_add_val(mdoc, entry, "consumers", mconsumers);
        }
        (void)yyjson_mut_obj_add_int(mdoc, entry, "consumer_count", (int64_t)consumer_count);
        (void)yyjson_mut_arr_add_val(out_exports, entry);
        total_consumers += consumer_count;
    }

    (void)yyjson_mut_obj_add_val(mdoc, mroot, "exports", out_exports);

    yyjson_mut_val *summary = yyjson_mut_obj(mdoc);
    (void)yyjson_mut_obj_add_int(mdoc, summary, "breaking", (int64_t)breaking);
    (void)yyjson_mut_obj_add_int(mdoc, summary, "non-breaking", (int64_t)non_breaking);
    (void)yyjson_mut_obj_add_int(mdoc, summary, "potentially-breaking", (int64_t)potentially_breaking);
    (void)yyjson_mut_obj_add_int(mdoc, summary, "consumer_count", (int64_t)total_consumers);
    (void)yyjson_mut_obj_add_val(mdoc, mroot, "summary", summary);

    char *out = yyjson_mut_write(mdoc, 0, NULL);
    yyjson_mut_doc_free(mdoc);
    yyjson_doc_free(doc);
    return out;
}

/* ----------------------------------------------- change_impact_certificate */

/* Is a path present in a JSON array of strings? */
static bool dx_path_in_list(yyjson_val *list, const char *path) {
    if (!yyjson_is_arr(list) || !path) {
        return false;
    }
    size_t idx;
    size_t max;
    yyjson_val *item;
    yyjson_arr_foreach(list, idx, max, item) {
        if (yyjson_is_str(item) && strcmp(yyjson_get_str(item), path) == 0) {
            return true;
        }
    }
    return false;
}

char *dx_governance_certificate(const char *before_json, const char *after_json,
                                 const char *boundaries_json) {
    yyjson_doc *before_doc = before_json ? yyjson_read(before_json, strlen(before_json), 0) : NULL;
    yyjson_doc *after_doc = after_json ? yyjson_read(after_json, strlen(after_json), 0) : NULL;
    yyjson_doc *bounds_doc =
        boundaries_json ? yyjson_read(boundaries_json, strlen(boundaries_json), 0) : NULL;

    yyjson_val *before = before_doc ? yyjson_doc_get_root(before_doc) : NULL;
    yyjson_val *after = after_doc ? yyjson_doc_get_root(after_doc) : NULL;
    yyjson_val *bounds = bounds_doc ? yyjson_doc_get_root(bounds_doc) : NULL;

    if (!yyjson_is_arr(after)) {
        if (before_doc) {
            yyjson_doc_free(before_doc);
        }
        if (after_doc) {
            yyjson_doc_free(after_doc);
        }
        if (bounds_doc) {
            yyjson_doc_free(bounds_doc);
        }
        return NULL;
    }

    yyjson_mut_doc *mdoc = yyjson_mut_doc_new(NULL);
    if (!mdoc) {
        if (before_doc) {
            yyjson_doc_free(before_doc);
        }
        if (after_doc) {
            yyjson_doc_free(after_doc);
        }
        if (bounds_doc) {
            yyjson_doc_free(bounds_doc);
        }
        return NULL;
    }
    yyjson_mut_val *mroot = yyjson_mut_obj(mdoc);
    yyjson_mut_doc_set_root(mdoc, mroot);

    yyjson_mut_val *opened = yyjson_mut_arr(mdoc);
    yyjson_mut_val *boundary_rows = yyjson_mut_arr(mdoc);
    int opened_total = 0;

    size_t bidx;
    size_t bmax;
    yyjson_val *bitem;
    yyjson_arr_foreach(bounds, bidx, bmax, bitem) {
        if (!yyjson_is_obj(bitem)) {
            continue;
        }
        const char *name = dx_get_str(bitem, "name");
        const char *prefix = dx_get_str(bitem, "path_prefix");
        int opened_here = 0;

        /* The differential: reachable after, not before, inside the boundary. */
        size_t aidx;
        size_t amax;
        yyjson_val *aitem;
        yyjson_arr_foreach(after, aidx, amax, aitem) {
            if (!yyjson_is_str(aitem)) {
                continue;
            }
            const char *path = yyjson_get_str(aitem);
            if (!dx_path_in_boundary(path, prefix)) {
                continue;
            }
            if (dx_path_in_list(before, path)) {
                continue; /* already reachable - not opened by this change */
            }
            yyjson_mut_val *row = yyjson_mut_obj(mdoc);
            dx_add_str_or_null(mdoc, row, "boundary", name);
            (void)yyjson_mut_obj_add_str(mdoc, row, "path", path);
            (void)yyjson_mut_arr_add_val(opened, row);
            opened_here++;
            opened_total++;
        }

        yyjson_mut_val *brow = yyjson_mut_obj(mdoc);
        dx_add_str_or_null(mdoc, brow, "name", name);
        dx_add_str_or_null(mdoc, brow, "path_prefix", prefix);
        (void)yyjson_mut_obj_add_int(mdoc, brow, "opened", (int64_t)opened_here);
        (void)yyjson_mut_arr_add_val(boundary_rows, brow);
    }

    (void)yyjson_mut_obj_add_val(mdoc, mroot, "opened", opened);
    (void)yyjson_mut_obj_add_val(mdoc, mroot, "boundaries", boundary_rows);
    (void)yyjson_mut_obj_add_str(mdoc, mroot, "verdict", opened_total > 0 ? "opened" : "clean");

    char *out = yyjson_mut_write(mdoc, 0, NULL);
    yyjson_mut_doc_free(mdoc);
    if (before_doc) {
        yyjson_doc_free(before_doc);
    }
    if (after_doc) {
        yyjson_doc_free(after_doc);
    }
    if (bounds_doc) {
        yyjson_doc_free(bounds_doc);
    }
    return out;
}

/* --------------------------------------------------------------- enforce */

/* Does the baseline already freeze this kind? */
static bool dx_baseline_freezes(const char *baseline, const char *kind) {
    if (!baseline || !kind) {
        return false;
    }
    yyjson_doc *doc = yyjson_read(baseline, strlen(baseline), 0);
    if (!doc) {
        return false;
    }
    yyjson_val *root = yyjson_doc_get_root(doc);
    bool found = false;
    size_t idx;
    size_t max;
    yyjson_val *item;
    yyjson_arr_foreach(root, idx, max, item) {
        if (!yyjson_is_obj(item)) {
            continue;
        }
        const char *action = dx_get_str(item, "action");
        const char *k = dx_get_str(item, "kind");
        if (action && k && strcmp(action, DX_ACTION_FROZEN) == 0 && strcmp(k, kind) == 0) {
            found = true;
            break;
        }
    }
    yyjson_doc_free(doc);
    return found;
}

char *dx_governance_enforce(const char *findings_json, const char *baseline_json) {
    yyjson_doc *doc = findings_json ? yyjson_read(findings_json, strlen(findings_json), 0) : NULL;
    yyjson_val *root = doc ? yyjson_doc_get_root(doc) : NULL;
    if (!yyjson_is_arr(root)) {
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

    yyjson_mut_val *blocking = yyjson_mut_arr(mdoc);
    yyjson_mut_val *frozen_new = yyjson_mut_arr(mdoc);
    yyjson_mut_val *frozen_accepted = yyjson_mut_arr(mdoc);
    yyjson_mut_val *advisory = yyjson_mut_arr(mdoc);
    yyjson_mut_val *off = yyjson_mut_arr(mdoc);

    int blocking_count = 0;
    int frozen_new_count = 0;

    size_t idx;
    size_t max;
    yyjson_val *item;
    yyjson_arr_foreach(root, idx, max, item) {
        if (!yyjson_is_obj(item)) {
            continue;
        }
        const char *action = dx_get_str(item, "action");
        const char *kind = dx_get_str(item, "kind");
        const char *id = dx_get_str(item, "id");
        const char *message = dx_get_str(item, "message");

        yyjson_mut_val *row = yyjson_mut_obj(mdoc);
        dx_add_str_or_null(mdoc, row, "id", id);
        dx_add_str_or_null(mdoc, row, "message", message);
        dx_add_str_or_null(mdoc, row, "kind", kind);

        if (action && strcmp(action, DX_ACTION_BLOCKING) == 0) {
            (void)yyjson_mut_arr_add_val(blocking, row);
            blocking_count++;
        } else if (action && strcmp(action, DX_ACTION_FROZEN) == 0) {
            if (dx_baseline_freezes(baseline_json, kind)) {
                /* The baseline already accepts this kind: adopted, not new. */
                (void)yyjson_mut_arr_add_val(frozen_accepted, row);
            } else {
                (void)yyjson_mut_arr_add_val(frozen_new, row);
                frozen_new_count++;
            }
        } else if (action && strcmp(action, DX_ACTION_OFF) == 0) {
            (void)yyjson_mut_arr_add_val(off, row);
        } else {
            /* An unrecognised action is advisory, never blocking: an unknown
             * policy must not fail a build. */
            (void)yyjson_mut_obj_add_str(mdoc, row, "action", DX_ACTION_ADVISORY);
            (void)yyjson_mut_arr_add_val(advisory, row);
        }
    }

    (void)yyjson_mut_obj_add_val(mdoc, mroot, "blocking", blocking);
    (void)yyjson_mut_obj_add_val(mdoc, mroot, "frozen_new", frozen_new);
    (void)yyjson_mut_obj_add_val(mdoc, mroot, "frozen_accepted", frozen_accepted);
    (void)yyjson_mut_obj_add_val(mdoc, mroot, "advisory", advisory);
    (void)yyjson_mut_obj_add_val(mdoc, mroot, "off", off);
    (void)yyjson_mut_obj_add_bool(mdoc, mroot, "held",
                                  blocking_count == 0 && frozen_new_count == 0);
    (void)yyjson_mut_obj_add_int(mdoc, mroot, "blocking_count", (int64_t)blocking_count);
    (void)yyjson_mut_obj_add_int(mdoc, mroot, "frozen_new_count", (int64_t)frozen_new_count);

    char *out = yyjson_mut_write(mdoc, 0, NULL);
    yyjson_mut_doc_free(mdoc);
    yyjson_doc_free(doc);
    return out;
}
