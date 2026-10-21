# F1-SURFACE.md - The `dx_codebase` tool: contract and design

**Milestone M1 of [`PLAN.md`](PLAN.md).** This is the contract the C
implementation must satisfy. Where this file and the code disagree, the code is
the bug.

**Why this exists.** The base ships 17 MCP tools behind a Cypher query language,
and an agent wired to the server without the install-time skill file never
reaches the relation graph - it greps instead. `dx_codebase` replaces the menu
with **one** tool whose schema carries the reasoning as required arguments.

---

## 1. The tool

Name: `dx_codebase`

**Input** (JSON object, all four top-level fields required):

| Field | Type | Meaning |
| --- | --- | --- |
| `question` | string | The code question, in the caller's own words. Never a symbol list. |
| `draft` | object | The smallest plan, and why. `{ "reason": string, "type": <request type> }` |
| `review` | string | Self-correction. May name a different type than `draft.type`. |
| `request` | object | The committed request. `{ "type": <request type>, ...type-specific fields }` |

**Request types** (the `type` discriminator):

| `type` | Purpose | Underlying base tool |
| --- | --- | --- |
| `search` | find symbols | `search_graph` |
| `trace` | callers, callees, call path | `trace_path` |
| `impact` | blast radius of a symbol | `trace_path` (the direction is one of that tool's own parameters) |
| `details` | one symbol's source and neighbourhood | `get_code_snippet` |
| `outline` | a file's declaration outline | `get_file_outline` |
| `architecture` | repo-level overview | `get_architecture` |
| `schema` | the graph schema | `get_graph_schema` |
| `coverage` | is this scope indexed and fresh | `check_index_coverage` |
| `changes` | what a diff reaches | `detect_changes` |
| `escape` | not a graph question - hand it back to your own tools | none - returns a clean bail-out |
| `verify` | does the graph back this claim | none - answered by the DX layer |
| `certificate` | did this change open a new path into a declared boundary | none - answered by the DX layer |
| `enforce` | the commit gate | none - answered by the DX layer |

**The governance requests** are not base tools. They turn graph facts into
verdicts, and the verdict policy lives in `src/mcp/dx_governance.c` - separate
from the graph access, so the verdicts are testable without a store. The rule
the whole module follows: **never certify what cannot be checked.**

| Request | Fields | Answers |
| --- | --- | --- |
| `verify` | `claim`, `subject` | `confirmed` / `refuted` / `unverifiable`, with a citation receipt. The subject is resolved through the base's own snippet lookup in outline mode - a verdict needs a location, not the code. A subject the graph does not have is **unverifiable, never refuted**: the graph not knowing is not evidence the claim is false. |
| `certificate` | `before`, `after` (path arrays), `boundaries` | `clean` / `opened`, with the opened paths named. The question is **differential**: reachable after, not before, and inside a declared boundary. A path that merely moved is not an opening. |
| `enforce` | `findings`, `baseline` | `held` or not, with the blocking, new-frozen, accepted-frozen, advisory and off findings separated. A frozen kind the baseline already accepts is **adopted debt, not a failure** - that is the difference between freezing a rule and failing a build. An unrecognised action is advisory, never blocking: an unknown policy must not fail a build. |

`certify_public_surface` - the per-export breaking-change verdict - is **not yet
wired**. Its policy is implemented and tested in `dx_governance.c`; the graph
side needs the diff's changed exports and their consumers, which is the next
slice. It is recorded in `TODO.md` as `CB-10`.

**The answer policy** (two optional request fields, and the DX surface's own):

| Field | Type | Default | Meaning |
| --- | --- | --- | --- |
| `source` | `"outline"` \| `"full"` | `outline` | Whether the answer may carry source bodies. `outline` answers with relationships and a `file:line` pointer, so the answer does not grow with the repository. `full` asks for the source text and costs what the source costs. |
| `max_tokens` | integer, 1..1000000 | the base tool's own | The token budget for the answer. An answer that does not fit is **refused** with the excess named, never silently truncated. |

Both are DX fields, so neither is forwarded to the base tool verbatim: the DX
surface translates them into the base's own vocabulary - `source_mode` and
`max_output_tokens` - and a caller who sets the base's parameter directly keeps
their value, because an explicit parameter beats a default.

`source: "full"` is **refused** on every request type except `details`,
because `source_mode` is only understood by `get_code_snippet`. On any other
type it would be a silent no-op, and a silent no-op is what this surface
exists to prevent.

**The fields a request needs.** The DX surface is the **only** schema the
agent sees - there is no second tool to read the base tool's schema from - so
the DX layer carries the base tool's own requirements itself:

| Type | Needs |
| --- | --- |
| `search`, `architecture`, `schema`, `coverage`, `changes` | nothing beyond `type` |
| `trace`, `impact` | `function_name` |
| `details` | `qualified_name` |
| `outline` | `file_path` |

A request missing one of those is **refused with the field named**, not
forwarded for the base to reject in its own words. The same list is in the
tool schema under `x-dx-required`, so the agent can read it rather than
discover it.

`project` is deliberately **not** in that list. Every base tool requires one
and the base has no implicit fallback (`resolve_store_internal`: *"project is
required - no implicit fallback"*), but the session already knows which project
it is on, so the DX layer fills it in from the session. A request that names
its own project keeps it.

**Output** (a tool result with text content, plus):

| Field | Meaning |
| --- | --- |
| `audit` | What the server checked this answer against. Never a claim the server cannot back. |
| `shape` | What the answer carried and what it cost: `source` (the policy in force), `tokens` (the estimate), `budget`, `over_budget`, `excess`, `bodies_included`. |
| `next` | `answer` (you have enough), `inspect` (one more named request), or `escape`. |

An answer the base refused or failed is **not** an answer: its `next` is
`inspect` and its detail says to read the error. "The answer is complete" on
a failed answer is a lie the agent would act on.

The `audit` states what it can back. The review is free text, so the layer
cannot report what the review *said*; when the committed type differs from
the drafted one it says the committed type **is** the one that was answered,
and not that the review named it.

**Token accounting.** One rate, used by both files: four UTF-8 bytes per token.
The base states the same contract in `src/mcp/mcp.c`
(`MCP_OUTPUT_BYTES_PER_TOKEN_ESTIMATE`), and the two must agree - a budget that
means two different things in two files is not a budget.

---

## 2. Validation rules

The envelope is checked **before** any handler runs, and a failure is a
**schema-shaped error**, never a success with a shrug.

1. `question` must be a non-empty string.
2. `draft` must be an object; `draft.reason` a non-empty string; `draft.type`
   must be a known request type.
3. `review` must be a non-empty string.
4. `request` must be an object; `request.type` must be a known request type.
5. **The envelope may not be skipped.** A call with any of the four fields
   missing is rejected. This is the whole point: prose can be ignored, a
   required field cannot.
6. `draft.type` and `request.type` may differ. That is the review step working.
   The response's `audit` records which type was committed.

**Error shape.** A rejected envelope returns a tool result with `isError: true`
and a message naming the field and the rule. It does **not** fall back to a
default request type.

---

## 3. Dispatch

`request.type` maps to one base tool. The type-specific fields are forwarded
**verbatim** as the underlying tool's arguments, with two exceptions: the DX
fields (`type`, `source`, `max_tokens`) are consumed by the DX surface and
translated into the base's own vocabulary - `source_mode` and
`max_output_tokens` - and a caller who set the base's parameter directly keeps
their value, because an explicit parameter beats a default.

**`escape` is special.** It never touches the graph. It returns the standard
bail-out text: the question is not a graph question, use the caller's own tools.
`next` is set to `escape`.

**The budget is a gate.** The DX surface measures the composed answer at four
bytes per token and, when it does not fit the caller's budget, **refuses** it:
the result becomes an error carrying the excess, the token count and the budget.
A shortened answer would read as a complete one, and the agent cannot tell the
difference - so the answer is refused, and the `next` field says what to do
about it. The refusal uses the base's own error shape, so a DX refusal is
indistinguishable from a base error and the agent knows how to treat it.

---

## 4. The `dx` tool profile

A new profile, `dx`, exposes **exactly one** tool: `dx_codebase`.

The base already has `ALL`, `ANALYSIS` (13 tools) and `SCOUT` (8 tools)
(`src/mcp/mcp.c:982-1015`). `dx` is a fourth, and it is the one DX installs by
default. The other three stay available and unchanged for anyone who wants them.

Profile selection is per-session, like the others, so a single install can serve
both a DX session and a plain session.

---

## 5. Test matrix

`tests/test_dx_surface.c`, using the house `tests/test_framework.h`.

| # | Case | Expectation |
| --- | --- | --- |
| 1 | Envelope with all four fields | parses; `request.type` recorded |
| 2 | Missing `question` | rejected, message names `question` |
| 3 | Missing `draft` | rejected, message names `draft` |
| 4 | Missing `review` | rejected, message names `review` |
| 5 | Missing `request` | rejected, message names `request` |
| 6 | `draft.type` unknown | rejected, message names `draft.type` |
| 7 | `request.type` unknown | rejected, message names `request.type` |
| 8 | `request.type` = `escape` | parses; no graph dispatch; bail-out text |
| 9 | `draft.type` != `request.type` | parses; committed type is `request.type`; audit records both |
| 10 | Type-specific fields forwarded | the rebuilt args JSON carries them verbatim |
| 11 | `project` | the caller's own is kept; otherwise the session's is filled in |
| 12 | `escape` in the type list | `escape` resolves to the bail-out, not to a base tool |
| 13 | Every known type maps | each maps to the expected base tool name |
| 14 | Response wrapping | `audit`, `shape` and `next` present; `next` is one of the three values |
| 15 | The source policy defaults | `outline` when the caller says nothing |
| 16 | `source: "full"` | accepted; the base is not forced to `outline` |
| 17 | `source` unknown | rejected, message names `request.source` |
| 18 | The source policy reaches the base | `source_mode: "outline"` on a `details` request; `source` itself is not forwarded |
| 19 | The budget reaches the base | `max_tokens` becomes `max_output_tokens`; `max_tokens` is not forwarded |
| 20 | An explicit `max_output_tokens` | wins over the DX budget |
| 21 | The token estimate | four bytes per token, rounded up |
| 22 | An answer over the budget | **refused**, not truncated; the excess is named |
| 23 | An answer inside the budget | passes through as a fresh buffer, never the caller's pointer |
| 24 | No budget | no gate |
| 25 | `source: "full"` on a type that cannot honour it | rejected, message names `request.source` |
| 26 | A malformed `max_tokens` | rejected, message names `request.max_tokens` - never silently ignored |
| 27 | A request missing the field its type needs | rejected, message names the field |
| 28 | A request with no `project` | the session's project is filled in |
| 29 | The base refused or failed | `next` is `inspect`; `isError` is set; the DX refusal sets it too |
| 25 | The shape | reports the source policy, the tokens, the budget, the excess and the bodies |
| 26 | Source bodies counted | one for a result carrying `source`, zero without |
| 27 | The schema advertises the policy | the `source` enum, its default and `max_tokens` bounds are all present |
| 28 | The schema and the table agree | the `type` enum equals the dispatch table, both in `request` and in `draft` |
| 29 | The schema requires the envelope | all four fields are in `required` |

**The module is compiled and run standalone.** It depends on `yyjson` and the C
standard library only - no store, no tree-sitter, no SQLite - so the contract
is verifiable without the full build:

```
gcc -std=c11 -Wall -Wextra -Werror -Isrc -Ivendored -Itests \
    -o build/dx_surface_test \
    tests/test_dx_surface.c src/mcp/dx_surface.c vendored/yyjson/yyjson.c tests/test_main.c
```

---

## 6. Files

| File | Role |
| --- | --- |
| `src/mcp/dx_surface.h` | The module's public interface. Small: parse, dispatch-target, wrap. |
| `src/mcp/dx_surface.c` | The implementation. No dependency on the rest of the server. |
| `tests/test_dx_surface.c` | The contract tests above. |
| `src/mcp/mcp.c` | Wiring: the tool definition, the handler, the `dx` profile. |
| `src/mcp/mcp_internal.h` | The `dx` profile enum value. |

---

MIT OR Apache-2.0
updated: 2026-10-21 00:00 UTC
