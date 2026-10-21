/* dx_memory.c - Memory that knows when it went stale.
 *
 * See dx_memory.h. The module is pure: it reads no files and calls no graph.
 * The caller supplies the current content of the anchored span; the module
 * decides what that means.
 */
#include "mcp/dx_memory.h"

#include <xxhash/xxhash.h>
#include <yyjson/yyjson.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ hash */

char *dx_memory_hash(const char *text) {
    if (!text) {
        return NULL;
    }
    size_t len = strlen(text);
    XXH64_hash_t h = XXH64(text, len, 0);
    char buf[17];
    snprintf(buf, sizeof(buf), "%016llx", (unsigned long long)h);
    /* A heap copy: the caller frees it, so a stack buffer would dangle and a
     * static one would be freed twice. */
    char *out = (char *)malloc(strlen(buf) + 1);
    if (!out) {
        return NULL;
    }
    memcpy(out, buf, strlen(buf) + 1);
    return out;
}

/* The state, and the reason for it. The reason is never empty: a verdict
 * without a reason is a claim, and this module makes claims only about
 * things it checked. */
static void dx_state_and_reason(const char *recorded_hash, const char *current_text,
                                 bool missing, bool unreadable, const char **state,
                                 const char **reason) {
    if (missing) {
        *state = DX_ANCHOR_GONE;
        *reason = "The file or symbol this memory is about no longer exists, so the memory "
                  "describes something that is not there.";
        return;
    }
    if (unreadable || !current_text) {
        *state = DX_ANCHOR_UNANCHORED;
        *reason = "The anchored span could not be located, so nothing can be claimed about "
                  "whether the memory is still true.";
        return;
    }
    if (!recorded_hash || !recorded_hash[0]) {
        *state = DX_ANCHOR_UNANCHORED;
        *reason = "The memory was written without a recorded hash, so it cannot be checked.";
        return;
    }
    char *now = dx_memory_hash(current_text);
    if (!now) {
        *state = DX_ANCHOR_UNANCHORED;
        *reason = "The anchored span could not be hashed, so nothing can be claimed about "
                  "whether the memory is still true.";
        return;
    }
    if (strcmp(now, recorded_hash) == 0) {
        *state = DX_ANCHOR_VALID;
        *reason = "The anchored span is byte-for-byte what it was when the memory was written.";
    } else {
        *state = DX_ANCHOR_CHANGED;
        *reason = "The anchored span has changed since the memory was written, so the memory "
                  "describes the code as it was, not as it is.";
    }
    free(now);
}

const char *dx_memory_anchor_state(const char *recorded_hash, const char *current_text,
                                    bool missing, bool unreadable) {
    const char *state = NULL;
    const char *reason = NULL;
    dx_state_and_reason(recorded_hash, current_text, missing, unreadable, &state, &reason);
    (void)reason;
    return state;
}

/* ---------------------------------------------------------------- recall */

/* A string member of a JSON object, or NULL. */
static const char *dx_mem_str(yyjson_val *obj, const char *key) {
    yyjson_val *v = yyjson_obj_get(obj, key);
    return yyjson_is_str(v) ? yyjson_get_str(v) : NULL;
}

/* A boolean member of a JSON object, or the fallback. */
static bool dx_mem_bool(yyjson_val *obj, const char *key, bool fallback) {
    yyjson_val *v = yyjson_obj_get(obj, key);
    if (!yyjson_is_bool(v)) {
        return fallback;
    }
    return yyjson_get_bool(v);
}

char *dx_memory_recall(const char *memories_json) {
    yyjson_doc *doc = memories_json ? yyjson_read(memories_json, strlen(memories_json), 0) : NULL;
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
    yyjson_mut_val *out_list = yyjson_mut_arr(mdoc);

    int valid = 0;
    int changed = 0;
    int gone = 0;
    int unanchored = 0;

    size_t idx;
    size_t max;
    yyjson_val *item;
    yyjson_arr_foreach(root, idx, max, item) {
        if (!yyjson_is_obj(item)) {
            continue;
        }
        yyjson_val *anchor = yyjson_obj_get(item, "anchor");
        const char *state = NULL;
        const char *reason = NULL;

        if (yyjson_is_obj(anchor)) {
            const char *current = dx_mem_str(anchor, "current");
            const char *hash = dx_mem_str(anchor, "hash");
            bool missing = dx_mem_bool(anchor, "missing", false);
            bool unreadable = dx_mem_bool(anchor, "unreadable", false);
            dx_state_and_reason(hash, current, missing, unreadable, &state, &reason);
        } else {
            state = DX_ANCHOR_UNANCHORED;
            reason = "This memory was never anchored to a span of code, so nothing can be claimed "
                     "about whether it is still true.";
        }

        if (strcmp(state, DX_ANCHOR_VALID) == 0) {
            valid++;
        } else if (strcmp(state, DX_ANCHOR_CHANGED) == 0) {
            changed++;
        } else if (strcmp(state, DX_ANCHOR_GONE) == 0) {
            gone++;
        } else {
            unanchored++;
        }

        /* The original fields are kept: a memory is not replaced by its
         * state, it is reported with it. */
        yyjson_mut_val *entry = yyjson_mut_obj(mdoc);
        size_t kidx;
        size_t kmax;
        yyjson_val *key;
        yyjson_val *val;
        yyjson_obj_foreach(item, kidx, kmax, key, val) {
            yyjson_mut_val *copy = yyjson_val_mut_copy(mdoc, val);
            if (copy) {
                (void)yyjson_mut_obj_add_val(mdoc, entry, yyjson_get_str(key), copy);
            }
        }
        (void)yyjson_mut_obj_add_str(mdoc, entry, "state", state);
        (void)yyjson_mut_obj_add_str(mdoc, entry, "reason", reason);
        (void)yyjson_mut_arr_add_val(out_list, entry);
    }

    (void)yyjson_mut_obj_add_val(mdoc, mroot, "memories", out_list);

    yyjson_mut_val *summary = yyjson_mut_obj(mdoc);
    (void)yyjson_mut_obj_add_int(mdoc, summary, "valid", (int64_t)valid);
    (void)yyjson_mut_obj_add_int(mdoc, summary, "changed", (int64_t)changed);
    (void)yyjson_mut_obj_add_int(mdoc, summary, "gone", (int64_t)gone);
    (void)yyjson_mut_obj_add_int(mdoc, summary, "unanchored", (int64_t)unanchored);
    (void)yyjson_mut_obj_add_val(mdoc, mroot, "summary", summary);

    char *out = yyjson_mut_write(mdoc, 0, NULL);
    yyjson_mut_doc_free(mdoc);
    yyjson_doc_free(doc);
    return out;
}
