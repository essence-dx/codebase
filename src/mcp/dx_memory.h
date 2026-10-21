/* dx_memory.h - Memory that knows when it went stale.
 *
 * The upstream base stores decisions (the ADR store) and the trace surface,
 * but nothing it stores can be checked against the code it describes. A
 * decision written three weeks ago about a function that was rewritten
 * yesterday reads exactly the same as one that is still true, and an agent
 * that trusts it builds on a fact that stopped being a fact.
 *
 * This module closes that gap. Every memory the DX layer records carries an
 * ANCHOR: the hash of the exact span of code the memory is about. A recall
 * re-checks the anchor and reports one of four states:
 *
 *   valid       the span is byte-for-byte what it was
 *   changed     the span moved or its content differs
 *   gone        the file or the symbol no longer exists
 *   unanchored  the span could not be located, so nothing can be claimed
 *
 * The rule is the one the base's own agent instructions state: treat an
 * unknown as unresolved, never as an all-clear. "gone" and "unanchored" are
 * different answers and are never merged.
 *
 * The module is PURE: it reads no files and calls no graph. The caller
 * supplies the current content of the anchored span, and the module decides
 * what that means. That split is what makes every state verifiable here,
 * without a store, a graph or a build.
 */
#ifndef CBM_MCP_DX_MEMORY_H
#define CBM_MCP_DX_MEMORY_H

#include <stdbool.h>

/* The four states an anchor can be in. */
#define DX_ANCHOR_VALID "valid"
#define DX_ANCHOR_CHANGED "changed"
#define DX_ANCHOR_GONE "gone"
#define DX_ANCHOR_UNANCHORED "unanchored"

/* A content hash, lowercase hex. The same text always hashes the same.
 * Returns a heap string (caller frees), or NULL on allocation failure. */
char *dx_memory_hash(const char *text);

/* The state of one anchor.
 *
 * recorded_hash  - the hash stored when the memory was written
 * current_text   - the text of the anchored span NOW, or NULL when it could
 *                  not be read
 * missing        - the anchored file or symbol no longer exists
 * unreadable     - the file exists but the span could not be located
 *
 * Returns a static string - one of the four DX_ANCHOR_* values - or NULL when
 * the state could not be determined. Do not free it. */
const char *dx_memory_anchor_state(const char *recorded_hash, const char *current_text,
                                    bool missing, bool unreadable);

/* Recall every memory and report its state.
 *
 * memories_json is a JSON array of objects:
 *   {"id": string, "kind": string, "title": string, "body": string,
 *    "anchor": {"hash": string,
 *                "current": string | null,
 *                "missing": bool, "unreadable": bool}}
 *
 * A memory with no anchor is reported as "unanchored" with a reason saying
 * it was never anchored: a memory nobody can check is not a memory anybody
 * can trust.
 *
 * Returns a heap JSON string (caller frees):
 *   {"memories":[{...the original fields...,"state":...,"reason":...}],
 *    "summary":{"valid":n,"changed":n,"gone":n,"unanchored":n}}
 */
char *dx_memory_recall(const char *memories_json);

#endif /* CBM_MCP_DX_MEMORY_H */
