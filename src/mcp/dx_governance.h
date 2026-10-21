/* dx_governance.h - The DX governance verdicts.
 *
 * A retrieval tool informs. A governance tool REFUTES. These are the four
 * verdicts that make dx-codebase a DX tool rather than a search index, and
 * none of them exists in the upstream base:
 *
 *   verify_claim            confirmed | refuted | unverifiable, with a citation
 *   certify_public_surface  breaking | non-breaking | potentially-breaking,
 *                           with the consumers named
 *   change_impact_certificate
 *                           did this diff OPEN A NEW PATH into a declared
 *                           boundary? (differential, not absolute)
 *   enforce                 one gate: blocking | frozen | advisory | off
 *
 * The rule the whole module follows: **never certify what cannot be checked.**
 * When the graph does not know, the verdict is "unverifiable", not "safe".
 *
 * The module is the POLICY, and it is deliberately separate from the graph
 * access. What the graph knows is gathered by the caller and handed in as
 * JSON; what a fact MEANS is decided here. That split is what makes the
 * verdicts testable without a store, without tree-sitter and without SQLite -
 * and it is what makes them worth auditing.
 *
 * Every function here takes JSON in and returns a JSON object the caller
 * frees, or NULL on allocation failure.
 */
#ifndef CBM_MCP_DX_GOVERNANCE_H
#define CBM_MCP_DX_GOVERNANCE_H

#include <stdbool.h>
#include <stddef.h>

/* The three verdicts a claim can get. "Unverifiable" is a first-class answer,
 * not a failure: the graph does not know, and saying so is the honest result.
 * The rule the module follows is stated in the base's own agent instructions:
 * treat an unknown as unresolved, never as an all-clear. */
#define DX_VERDICT_CONFIRMED "confirmed"
#define DX_VERDICT_REFUTED "refuted"
#define DX_VERDICT_UNVERIFIABLE "unverifiable"

/* The three verdicts a changed export can get. "Potentially breaking" is the
 * honest middle: the signature changed in a way the graph cannot prove is
 * safe, and the consumers are named so a human can decide. */
#define DX_COMPAT_BREAKING "breaking"
#define DX_COMPAT_NON_BREAKING "non-breaking"
#define DX_COMPAT_POTENTIALLY_BREAKING "potentially-breaking"

/* The four outcomes a finding can have at the gate. "frozen" adopts the
 * existing debt and blocks new findings of the same kind - the way a
 * codebase freezes a lint rule instead of failing the whole build. */
#define DX_ACTION_BLOCKING "blocking"
#define DX_ACTION_FROZEN "frozen"
#define DX_ACTION_ADVISORY "advisory"
#define DX_ACTION_OFF "off"

/* verify_claim.
 *
 * claim    - the claim being asserted, verbatim.
 * resolved - whether the graph could resolve the claim's subject. A claim
 *            about a symbol that is not in the graph is not refuted, it is
 *            unverifiable.
 * citation - where the evidence is, as "path:line" or a path. NULL when
 *            there is none.
 * reason   - why the verdict is what it is, when it is not "confirmed".
 *
 * Returns a JSON object:
 *   {"claim":..., "verdict":"confirmed|refuted|unverifiable",
 *    "citation":..., "reason":..., "receipt":{...}}
 * The receipt is the citation plus the verdict, so a decision can be quoted
 * without re-deriving it. */
char *dx_governance_verify_claim(const char *claim, bool resolved, const char *citation,
                                  const char *reason);

/* certify_public_surface.
 *
 * exports_json is a JSON array of objects:
 *   {"qualified_name": string,
 *    "kind": string,                       (optional)
 *    "compat": "breaking"|"non-breaking"|"potentially-breaking",
 *    "consumers": [{"path":string,"symbol":string,"line":int,
 *                   "via":string}]}       (optional)
 *
 * Returns a JSON object:
 *   {"exports":[{"qualified_name":..., "verdict":..., "consumers":[...],
 *                "consumer_count":int}],
 *    "summary":{"breaking":int,"non-breaking":int,"potentially-breaking":int,
 *               "consumer_count":int}}
 *
 * A breaking export with no named consumer is still reported as breaking: the
 * absence of a known consumer is not evidence that there is none. */
char *dx_governance_certify_surface(const char *exports_json);

/* change_impact_certificate.
 *
 * before_json and after_json are JSON arrays of strings: the paths reachable
 * before and after the change. boundaries_json is a JSON array of objects:
 *   {"name": string, "path_prefix": string}
 *
 * The question is differential: which path did the change OPEN into a
 * declared boundary - reachable after, not before, and inside the boundary.
 *
 * Returns a JSON object:
 *   {"opened":[{"boundary":string,"path":string}],
 *    "boundaries":[{"name":..., "opened":int}],
 *    "verdict":"clean"|"opened"}
 *
 * The paths are compared exactly. A path that merely moved is not an opening;
 * a path that was already reachable is not an opening. */
char *dx_governance_certificate(const char *before_json, const char *after_json,
                                 const char *boundaries_json);

/* enforce.
 *
 * findings_json is a JSON array of objects:
 *   {"id": string, "message": string,
 *    "action": "blocking"|"frozen"|"advisory"|"off",
 *    "kind": string}                       (optional; the "kind" a frozen
 *                                           finding freezes)
 * baseline_json is a JSON array of objects with the same shape, describing
 * the findings the baseline already accepts.
 *
 * Returns a JSON object:
 *   {"held": bool, "blocking":[...], "frozen_new":[...], "advisory":[...],
 *    "off":[...], "frozen_accepted":[...]}
 *
 * The gate holds when nothing blocking is present and no NEW frozen finding
 * appears. A finding whose kind the baseline already freezes is accepted, not
 * a new failure - that is the difference between freezing a rule and failing
 * a build. */
char *dx_governance_enforce(const char *findings_json, const char *baseline_json);

#endif /* CBM_MCP_DX_GOVERNANCE_H */
