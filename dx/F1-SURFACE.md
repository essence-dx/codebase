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
| `details` | one symbol's source and neighbourhood | `get_code_snippet` |
| `outline` | a file's declaration outline | `get_file_outline` |
| `architecture` | repo-level overview | `get_architecture` |
| `schema` | the graph schema | `get_graph_schema` |
| `coverage` | is this scope indexed and fresh | `check_index_coverage` |
| `changes` | what a diff reaches | `detect_changes` |
| `impact` | blast radius of a symbol | `search_graph` + `trace_path` |
| `escape` | **not a graph question** | none - returns a clean bail-out |

**Output** (a tool result with text content, plus):

| Field | Meaning |
| --- | --- |
| `audit` | What the server checked this answer against. Never a claim the server cannot back. |
| `next` | `answer` (you have enough), `inspect` (one more named request), or `escape`. |

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
**verbatim** as the underlying tool's arguments, with two additions the base
tools already understand:

- `project` - if the caller supplies it, it is passed through untouched.
- `include_source` - the F2 policy. Until F2 lands, the default is the base's
  current behaviour; the field exists so F2 can change the default without
  changing this contract.

**`escape` is special.** It never touches the graph. It returns the standard
bail-out text: the question is not a graph question, use the caller's own tools.
`next` is set to `escape`.

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
| 11 | `project` passthrough | present in the rebuilt args JSON |
| 12 | `escape` in the type list | `escape` resolves to the bail-out, not to a base tool |
| 13 | Every known type maps | each maps to the expected base tool name |
| 14 | Response wrapping | `audit` and `next` present; `next` is one of the three values |
| 15 | Budget field | `include_source` is not injected when absent |

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
