# CHANGELOG.md - What Changed in `essence-dx/codebase`

Dated, receipted, per release. One event has one record: when an item in
[`TODO.md`](TODO.md) closes, it is deleted there and a line is added here **in
the same commit**. Every entry names the `file:line` or the command output that
earns it.

**Claim tiers** apply to every line below. *repo-managed* means committed and
readable. *executed* means run here with a named command. *runtime / hosted*
means true on another host. *release-qualified* means shipping verified end to
end on a real device. **Nothing in this file is release-qualified** - this fork
has published no release of its own.

**This repository is a fork.** Everything not marked *DX* below is upstream's
work, by `DeusData/codebase-memory-mcp`, under MIT. DX's own changes are marked
*DX* and are the only ones this ledger claims.

---

## Unreleased

### 2026-10-21 - `fix:` a stack buffer overflow in upstream's own test suite

**Found by the toolchain, not by reading.** GCC 16's `-Wstringop-overflow`
rejected the house test build on a line that is a genuine defect:

```
tests/test_daemon_ipc.c:712:9: error: 'ipc_test_copy_path' accessing 1024 bytes
    in a region of size 256 [-Werror=stringop-overflow]
```

**The bug.** The helper declared its destination as a full-size path buffer
and wrote a full-size path buffer into it:

```c
static void ipc_test_copy_path(char out[TEST_PATH_CAP], const char *path) {
    ...
    (void)snprintf(out, TEST_PATH_CAP, "%s", path);
}
```

`TEST_PATH_CAP` is 1024. But five call sites pass a
`CBM_DAEMON_IPC_WINDOWS_NAME_CAP` buffer - **256 bytes**:

```c
char address_a[CBM_DAEMON_IPC_WINDOWS_NAME_CAP] = {0};
...
ipc_test_copy_path(address_a, cbm_daemon_ipc_endpoint_address(endpoint_a));
```

So a path longer than 255 characters wrote up to 1024 bytes into a 256-byte
stack buffer. It is a **stack buffer overflow in the test suite**, and it
survived upstream because upstream's CI compiles with clang, which does not
emit that warning for this shape.

**The fix.** The destination's size is taken at the call site, from the array
itself, and the write is bounded by the buffer that is actually there:

```c
static void ipc_test_copy_path_n(char *out, size_t out_size, const char *path) { ... }

#define ipc_test_copy_path(out, path) ipc_test_copy_path_n((out), sizeof(out), (path))
```

The macro leaves every call site unchanged - there are around eighty of them -
and each one now gets its own buffer's size. If a caller ever passes a
pointer rather than an array, `sizeof` yields the pointer's size, so the copy
truncates instead of overflowing.

**Also fixed here, and it was ours.** The five new DX profile tests were
written *after* the `SUITE(mcp)` block that references them, so the framework
saw an implicit declaration and then a `static` definition:

```
error: implicit declaration of function 'test_dx_profile_parses_from_the_flag'
error: static declaration of 'test_dx_profile_parses_from_the_flag' follows
       non-static declaration
```

The house convention is that every test is **defined** before the suite that
runs it. The definitions now are - 420 definitions, 420 `RUN_TEST` calls, every
one matched, no duplicates.

**And a toolchain fact, so the next attempt does not rediscover it.** The test
build defaults to `-fsanitize=address,undefined`, and this MinGW has **no
sanitizer runtime**: the link fails with

```
gcc: fatal error: cannot read spec file 'libsanitizer.spec': No such file or directory
```

`Makefile.cbm:102` already says what to do - *"Test flags: debug + sanitizers
(override SANITIZE= to disable on Windows)"* - so the house's own Windows path
is `make -f Makefile.cbm SANITIZE= build/c/test-runner`. **Every object
compiled** under the test flags, sanitizer or not, including the ones the
sanitizer flags touch; the failure was at the link and only because of the
runtime that is not installed.

**And a second upstream defect, from the same build.** GCC 16's
`-Wfree-nonheap-object` caught a test that frees a **poison pointer**:

```
tests/test_daemon_application.c:780:5: error: 'free' called on a pointer to an
unallocated object '1' [-Werror=free-nonheap-object]
```

The test arms the address 1 as a sentinel, to prove that a callback did *not*
write an output pointer - and then frees the value. If the request never ran,
which is what happens when the encode step fails, the sentinel is still in
place and `free((void *)1)` is an invalid free that aborts the whole test
binary rather than failing one assertion. The house already has the pattern
elsewhere in this suite (`if (ledger != (cbm_test_impact_ledger_t *)(uintptr_t)1)`),
so the free is now guarded the same way. A sweep of the suite for that shape
finds this one site and no other.

**The pattern is worth naming.** GCC 16's stricter analysis has now found two
genuine defects in upstream's test suite that upstream's own CI cannot see,
because that CI compiles with clang. Neither is in the product; both are in
the tests that guard the product.

**Executed.**

| Check | Result |
| --- | --- |
| `tests/test_daemon_ipc.c` with the test flags | exit 0 - the overflow is gone |
| `tests/test_daemon_application.c` with the test flags | exit 0 - the invalid free is gone |
| `tests/test_mcp.c` with the test flags | exit 0 |
| Every test object under the test flags | compiles |
| The house test runner | linking with `SANITIZE=` |

### 2026-10-21 - `feat:` the request says what it needs, and the session says which project

**Two more flaws, both found by reading the base's own tool schemas out of the
running server rather than from the source.**

**Flaw 6 - the DX schema never said what a request needs.** The DX surface is
the only schema the agent sees - that is the entire point of a one-tool
surface - so when the schema said the request's other fields are "the base
tool's own parameters and are forwarded verbatim", it was describing something
the agent had no way to look up. There is no second tool to read
`search_graph`'s schema from. So the agent had to send a request, be told it
was wrong, and read the error to find out what the tool wanted. That is the
"menu the agent never navigates" problem in a new place: the DX surface
removed the menu and left the guessing.

Read straight out of the built server, the base's own requirements are:

| Type | Base tool | Required by the base |
| --- | --- | --- |
| `search` | `search_graph` | `project` |
| `trace` | `trace_path` | `function_name`, `project` |
| `impact` | `trace_path` | `function_name`, `project` |
| `details` | `get_code_snippet` | `qualified_name`, `project` |
| `outline` | `get_file_outline` | `project`, `file_path` |
| `architecture` | `get_architecture` | `project` |
| `schema` | `get_graph_schema` | `project` |
| `coverage` | `check_index_coverage` | `project` |
| `changes` | `detect_changes` | `project` |

**The fix.** The DX layer now carries the requirement itself and checks the
request against it. A `trace` request with no `function_name` is **refused
with the field named** - *"a 'trace' request needs function_name, and the
request does not have function_name"* - instead of being forwarded to a base
tool that will reject it in its own words. The schema carries the same list
in an `x-dx-required` block, so the agent can read it rather than discover
it.

**Flaw 7 - the agent had to name the project, and it cannot know the name.**
Every base tool requires a `project`, and the base has **no implicit
fallback** - `src/mcp/mcp.c`'s `resolve_store_internal` says so in as many
words:

```c
if (!project) {
    return NULL; /* project is required - no implicit fallback */
}
```

So every DX request had to carry a project name - the one thing an agent
working on "this repository" has no way of knowing. The session already knows
which project it is on (`cbm_mcp_server_session_project`), and the base's own
help text says "the session project's index refreshes automatically". So the
DX layer now **fills the project in from the session** when the request did
not name one, and a request that names its own project keeps it.

**Why this is a DX improvement and not a convenience.** The thesis of the
surface is that the agent asks a *question about the code*, not a question
about a project and a symbol. Making it name the project was leaking the
base's plumbing into the surface.

**One more correction, in the instructions.** The DX profile's server
instructions described `impact` as "what a change affects". That is
`detect_changes`' job, and `impact` dispatches to `trace_path`. It is now
described as what it is - **"what depends on a symbol: who breaks if it
changes"** - and the instructions point at `x-dx-required` and say not to
send `project` unless a different one is meant.

**And two more flaws, found by reading the server's own answer.** The probe
drove the built binary with a complete envelope and got back an error - the
repository is not indexed here - and the answer said:

```
audit: Answered from the indexed graph. The committed request type was 'details' ...
next:  {"kind":"answer","detail":"the answer is complete; read the source it names if you need more"}
text:  {"error":"project not found or not indexed", ...}
```

**"the answer is complete" on a failed answer.** An agent that reads `next` and
stops has stopped on a failure. So:

- When the underlying result carries `isError`, the `next` kind is now
  **`inspect`** and the detail says the answer was an error and to read it.
- And the DX layer's **own refusal** - the over-budget answer that is
  refused rather than truncated - did not set `isError` at all. A refusal
  that reads like a success with an odd body is exactly the failure this
  surface exists to prevent, so it now sets `isError`, like every base error.

**Executed.**

| Check | Result |
| --- | --- |
| The surface contract tests | **54 passed**, exit 0 (was 44) |
| The governance contract tests | 23 passed, exit 0 |
| The memory contract tests | 12 passed, exit 0 |
| **89 DX contract tests in total** | all green |
| `gcc -fanalyzer` over the three DX modules and their three test files | **clean** |
| The base tools' required fields, read from the running server | the table above |
| `make -f Makefile.cbm cbm` | **links**, exit 0 |
| The DX surface, driven over real stdio against the **final** binary | exit 0; **4 responses**; exactly one tool (`dx_codebase`, `alwaysLoad`); the four-field envelope enforced; `audit`, `shape` and `next` on every answer; an errored answer's `next` is `inspect` |

The receipt from the final binary, verbatim:

```
exit code: 0
responses: 4

tools/list
  tool count: 1
  dx_codebase  alwaysLoad=True

tools/call, missing envelope
  isError: True
  text: dx_codebase: 'draft' is required and must be an object with 'reason' and 'type'

tools/call, complete envelope (no index in this environment)
  isError: True
  audit: Answered from the indexed graph. The committed request type was 'details', which the
         draft also proposed. The graph is the only source consulted; where it does not know,
         the answer says so.
  next:  {"kind":"inspect","detail":"the answer was an error, not a result; read it and decide
          what to do next"}
  shape: {"source":"outline","tokens":75,"over_budget":false,"bodies_included":0}
  text:  {"error":"project not found or not indexed","hint":"No projects indexed yet. Call
          index_repository first."}
```

The last line is the base's own honest error, passed through untouched. A tool
that hides a failure behind a cheerful answer is worse than no tool, and the
DX surface says what happened and what to do next.

That error is also the receipt for the session project. The base has two
different failures and the probe shows the *named* one:

```c
return project ? build_project_list_error("project not found or not indexed")
                : build_missing_project_error();
```

The request named no project, and the base still said *"project not found or
not indexed"* rather than *"project is required"*. So the session's project
was filled in by the DX layer on its way to the base - the feature works in
the product, not just in a unit test.

**The surface, measured on the built binary.** The `tools/list` payload the
agent receives, under each profile, at the base's own four-bytes-per-token
rate:

| Profile | Tools | Schema bytes | Tokens |
| --- | --- | --- | --- |
| ALL (upstream's surface) | 18 | 24111 | 6028 |
| DX (this fork's surface) | 1 | 3182 | 796 |

**An 86.8% reduction in the schema the agent receives.** That is a
*surface-size* measurement and nothing more: it is not a session-level token
cost, and it is not the base's +66% figure, which is a whole-session
measurement. The session measurement is `CB-13`, and it has not been run.
What this number does say is where the surface's cost lives: almost all of it
is in the seventeen schemas the DX profile does not send, and the one schema
it does send - 2547 bytes - carries the whole contract.

**New house tests**, where both halves are linked and can be compared:

- `dx_the_required_fields_match_the_base_tools_own_schemas` - for every DX
  type, every field the base tool requires (other than `project`) is in the
  DX layer's list, and `project` is not. If the base changes a tool's
  requirements, this fails.
- `dx_the_schema_names_the_same_required_fields_as_the_layer` - the schema
  the agent reads and the list the layer enforces say the same thing.

### 2026-10-21 - `fix:` the DX profile could not start the server at all

**The worst flaw found in this audit, and the one the unit tests could never
have caught.** The DX surface - the whole reason this fork exists - **did not
work in the product.** Not "worked badly": the server exited immediately.

**The reproduction, with the built binary.**

```
> codebase-memory-mcp.exe --tool-profile=dx
exit code: 1
stderr: codebase-memory-mcp: daemon session context was rejected
responses: 0
```

Zero JSON-RPC responses. The process refused to serve. Every DX feature
built so far - the surface, the answer policy, the governance verdicts, the
anchored memory - was **unreachable**, because the only way to reach it is the
DX profile, and the DX profile could not start.

**Why.** `src/mcp/mcp.h` puts the DX profile at 3:

```c
CBM_MCP_TOOL_PROFILE_ALL = 0,
CBM_MCP_TOOL_PROFILE_ANALYSIS = 1,
CBM_MCP_TOOL_PROFILE_SCOUT = 2,
CBM_MCP_TOOL_PROFILE_DX = 3,
```

But the daemon's context handshake bounded the profile at `SCOUT`:

```c
/* src/daemon/application.c, in both the client and the daemon */
tool_profile > CBM_MCP_TOOL_PROFILE_SCOUT ||
profile_value > (uint8_t)CBM_MCP_TOOL_PROFILE_SCOUT ||
```

So DX - which is 3 - was **outside the valid range**. The client sent it, the
daemon rejected it, and `src/main.c` turned that rejection into
`EXIT_FAILURE` with "daemon session context was rejected" on stderr. The
upstream code was not wrong: it had three profiles, and the bound was the
third. **Adding a fourth profile without updating the bound is the bug**, and
the bound was written against a *named profile* rather than against "the
highest one", so nothing about the change made it look wrong.

**Why the tests did not catch it.** Every DX test calls the functions
directly. The profile is the *process-level* selection, and nothing in the
unit suites ever went through the daemon handshake. **A green suite, 79 tests
passing, and a product that cannot start.** The only thing that found it was
running the built binary with the flag a user would use.

**The fix (DX).** The bound is now written against a name that means what it
says:

```c
/* The highest valid profile value. A new profile added above this line is
 * accepted by the bounds automatically; a new profile added below it is not,
 * and that is the point. */
CBM_MCP_TOOL_PROFILE_LAST = CBM_MCP_TOOL_PROFILE_DX,
```

Both handshake sites now bound against `CBM_MCP_TOOL_PROFILE_LAST`. Five
`_Static_assert`s in `src/mcp/mcp.c` pin the enum to be contiguous from 0 with
DX last, so a profile added later has to be added above that line and `LAST`
updated - and if it is not, the build fails instead of the server refusing to
start. The comment at the enum records the exact incident, because the next
person to add a profile needs to know what happens if they forget.

**A second, quieter flaw in the same area.** The DX profile had **no server
instructions of its own.** `MCP_SERVER_INSTRUCTIONS` (the generic upstream
text), `MCP_ANALYSIS_SERVER_INSTRUCTIONS` and `MCP_SCOUT_SERVER_INSTRUCTIONS`
each exist, and the profile switch covered only the latter two - so an agent
connecting with the DX profile was handed instructions describing a
seventeen-tool menu it could not call, and was never told that the surface
requires four fields, that an over-budget answer is refused rather than
truncated, or where to look when the graph does not know. `MCP_DX_SERVER_
INSTRUCTIONS` now says all of that, and the profile switch selects it.

**Executed.**

| Check | Result |
| --- | --- |
| `--tool-profile=dx` against the built binary, **before** the fix | exit 1, "daemon session context was rejected", **0 responses** |
| `--tool-profile=dx` against the built binary, **after** the fix | exit 0, **4 responses**, exactly one tool |
| `tools/list` under the DX profile | exactly one tool: `dx_codebase`, marked `alwaysLoad` |
| `tools/call` with a missing envelope | refused with the missing fields named |
| The surface contract tests | 44 passed, exit 0 |
| `gcc -fanalyzer` over the three DX modules and their three test files | **clean** |

**New house tests** (they run in the project's own runner, not standalone):

- `dx_profile_parses_from_the_flag` - both spellings work, an unknown value
  fails closed instead of widening the surface.
- `dx_profile_does_not_open_the_http_surface` - a restricted profile must not
  start a second unrestricted surface.
- `dx_profile_advertises_exactly_one_tool` - the thesis in one assertion: the
  DX profile exposes `dx_codebase` and nothing else, and it is marked
  `alwaysLoad` so a client that defers tools behind ToolSearch still shows it.

### 2026-10-21 - `fix:` five brutal flaws, and the build that now links

**The full product build now completes on this machine.** It did not before
this entry, and the reason was not DX's code. One line of upstream's portability
plus one line of mine were standing in the way.

**Flaw 1 - the build blocker (upstream).** `src/foundation/arena.h` declared
`cbm_arena_sprintf` with `__attribute__((format(printf, 2, 3)))`. GCC's `printf`
attribute means *the target's* printf, and on MinGW that is the Microsoft one,
whose format dialect has no `%z` length modifier. The implementation is
`vsnprintf` - C99 - so `%zu` works perfectly at runtime; only the compile-time
check rejected it:

```
src/pipeline/helm_template.c:2202: error: unknown conversion type character 'z' in format
internal/cbm/doc_pdf.c:1337:      error: unknown conversion type character 'z' in format
src/pipeline/pass_k8s.c:4672:     error: too many arguments for format
```

**The fix (DX).** A new `src/foundation/printf_attr.h` provides
`CBM_PRINTF_FMT(fmt_index, first_arg)`, which selects `gnu_printf` - always the
C99-conformant format - and expands to nothing on a compiler without the
attribute. The attribute is a check on the *format string*, not on the runtime
library, so describing these three helpers as C99 is the accurate thing to do.
Applied to all three sites that had the attribute: `arena.h`, `http_server.c`,
`httpd.h`. The third of those was already guarded by a compiler check, which is
why the fix belongs in a shared header rather than in one file.

**Flaw 2 - a module that compiled in the test and did not link in the product
(DX).** `src/mcp/dx_memory.c` called the vendored `XXH64` through an external
reference. The product link does not carry `vendored/xxhash/xxhash.c`, so the
linker refused it:

```
ld.exe: undefined reference to `XXH64'
```

**The standalone test passed the whole time** - because that test linked
`xxhash.c` separately. This is the exact shape of flaw the house test binary
exists to catch, and it is a reminder that a green standalone suite is not a
built product. The fix is the house pattern (`src/semantic/rotsq.c`,
`src/pipeline/pass_semantic_edges.c`): `#define XXH_INLINE_ALL` before the
include, so the implementation is instantiated into the module's own
translation unit and no link dependency is introduced at all. Verified with
`nm`: the object now exports no external `XXH` symbol.

**Flaw 3 - a silent no-op in the answer policy (DX).** `request.source: "full"`
was accepted for every request type, but only `get_code_snippet` understands
`source_mode`. On every other type the request was a no-op - the caller asked
for source bodies and got the default, with nothing saying so. A silent no-op
is exactly what this tool exists to prevent, so `"full"` is now **refused** on
the types that cannot honour it, and the refusal names the problem. `"outline"`
is still accepted everywhere, because it is the default and asking for the
default is not a request that goes unserved.

**Flaw 4 - a malformed budget silently became no budget (DX).**
`request.max_tokens` was read with a helper that returns 0 for anything out of
range. A budget of `"lots"`, `0`, `-5` or `99999999` therefore became "no
budget" - an unbounded answer handed to a caller who believed they had set a
limit. A budget that is present but malformed is now an **error**, with the
range named.

**Flaw 5 - the shape report could lie (DX).** Two ways:

- The base writes `"(source not available)"` where a source body would be,
  rather than omitting the key. Counting key presence therefore reported a
  body for an answer that carried none. A body now counts only when the value
  is a real, non-empty string.
- Only the top level of the payload was inspected, so a tree result - which
  carries a body on each row - was reported as carrying none. The count is now
  a walk of the whole payload.

And one more: if a base result already carried an `audit`, `shape` or `next`
key, the DX wrapper added a second key of the same name. A JSON object with
one key twice is ambiguous and a parser may take either. The DX value now
**replaces** the base's rather than sitting beside it.

**And a sixth, found by the test build rather than the product build.**
`arena.h`'s new `#include "foundation/printf_attr.h"` resolved for the product
build and failed for the **house test build**, which compiles the internal
extraction units as a unity build with `GRAMMAR_CFLAGS_TEST` - and that flag
set has no `-Isrc`. The error named a header that plainly exists, which is
what a relative-include mistake looks like. The house already had the answer
written down: `src/foundation/hash_table.h` and `src/foundation/dyn_array.h`
both include their siblings relatively, each with a comment saying why - *"same
directory: the lsp_all unit has no -Isrc"*. `arena.h` now does the same.
Verified by compiling `lsp_all.c` with the grammar flags and no `-Isrc`.

**Executed.**

| Check | Result |
| --- | --- |
| `make -f Makefile.cbm cbm` on MinGW | **links** (was: 3 format errors) |
| The surface contract tests | **43 passed**, exit 0 (was 34) |
| The governance contract tests | 23 passed, exit 0 |
| The memory contract tests | 12 passed, exit 0, with no external `XXH` symbol |
| `nm` on the memory object | no undefined `XXH` reference |
| The full server build | **SUCCEEDS** - the DX layer is in the product binary |

**What is still not done.** The **house test binary** has not been built and
run, so the DX suites still run standalone rather than inside the project's
own harness. `CB-20` now records that as the remaining step.

### 2026-10-21 - `docs:` the Windows build story, and one reverted prototype

**Two findings, one of which changes what the DX layer can claim.**

**The full build fails on this machine, and the reason is upstream.** The whole
tree compiles - all 162 vendored grammars, the tree-sitter runtime, SQLite,
mimalloc, xxhash, yyjson - and reaches the link stage. It then fails to compile
three call sites:

```
src/pipeline/helm_template.c:2202:72: error: unknown conversion type character 'z' in format
internal/cbm/doc_pdf.c:1337:70: error: unknown conversion type character 'z' in format
src/pipeline/pass_k8s.c:4672:58: error: too many arguments for format
```

`src/foundation/arena.h:82` declares `cbm_arena_sprintf` with
`__attribute__((format(printf, 2, 3)))`. On **MinGW** that attribute resolves to
`ms_printf` semantics, which do not support `%zu`. Upstream's Windows binary is
an **MSVC** build - this box has MinGW and no MSVC (`vswhere` is absent), so the
project's own sanctioned Windows path (`scripts/test-windows.ps1`) cannot run
here.

**What this means for the DX layer, stated plainly.** Every DX module is
compile-verified with the project's own flags and unit-tested standalone, but
**the full server build has never been run in this environment**, and the DX
suites have never run inside the house test binary. `CB-20` records the two
ways out - build on MSVC, or make the format attribute portable so the tree
builds with the toolchain that is actually present.

**One prototype reverted rather than shipped.** The DX-native org graph layer
(`CB-19`) was designed, written and prototyped. Its output carried corrupted
string values, the cause was not found in the time available, and **unverified
code does not go in** - so it was reverted, and the design and the two defects
found are recorded instead:

- a size calculation was wrong (`+8` where the id needed `+10`), so a
  dependency id was silently truncated to `dx:dep:bot->metasear`;
- a folder that was both a route target and a dependency source produced two
  nodes with one id, and a graph with the same id twice is not a graph.

**Executed.**

| Check | Result |
| --- | --- |
| `dx_surface.c` compiles clean; its contract tests pass | exit 0, **34 passed** |
| `dx_governance.c` compiles clean; its contract tests pass | exit 0, **23 passed** |
| `dx_memory.c` compiles clean; its contract tests pass | exit 0, **12 passed** |
| `mcp.c`, `dx_surface.c`, `dx_governance.c`, `dx_memory.c` and all three test files pass `-fsyntax-only` with the project include set | exit 0 |
| The full server build | **FAILS** - the MinGW `ms_printf` attribute rejects `%zu` (`CB-20`) |

### 2026-10-21 - `feat:` anchored memory - a memory that says when it went stale

**Milestone M5, policy half.** The base stores decisions, but nothing it stores
can be checked against the code it describes. A decision written three weeks ago
about a function that was rewritten yesterday reads exactly the same as one that
is still true.

**What was built (DX).**

| File | What it is |
| --- | --- |
| `src/mcp/dx_memory.h` | The four anchor states and the contract. |
| `src/mcp/dx_memory.c` | The policy. **Pure**: it reads no files and calls no graph. |
| `tests/test_dx_memory.c` | 12 contract tests. |

**The four states.**

| State | Means |
| --- | --- |
| `valid` | the anchored span is byte-for-byte what it was |
| `changed` | the span moved or its content differs |
| `gone` | the file or the symbol no longer exists |
| `unanchored` | the span could not be located, so nothing can be claimed |

**"gone" and "unanchored" are different answers and are never merged.** A memory
that was never anchored to a span recalls as `unanchored`, never as `valid`: a
memory nobody can check is not a memory anybody can trust. And every memory comes
back with a **reason** - a verdict without a reason is a claim, and this module
makes claims only about things it checked.

**The module is pure on purpose.** It does not read the file; the caller
supplies the current content of the anchored span and the module decides what
that means. That is what makes every state verifiable without a store, a graph
or a build - and it is why the store side can be written later without touching
the policy.

**No new dependency.** The hash is the vendored `xxhash` (XXH64, lowercase
hex), so the same text always hashes the same and nothing is fetched at
runtime.

**Executed.**

| Check | Result |
| --- | --- |
| `dx_memory.c` compiles clean under the project flags | exit 0 |
| The memory contract tests pass | **12 passed, exit 0** |
| `dx_memory.c` and `test_dx_memory.c` pass `-fsyntax-only` with the project include set | exit 0 |

**Not yet done.** The store side - writing a memory with its anchor hash, and
resolving an anchor to the current span at recall time - needs the base's store
and its Windows-safe file access. `CB-11` records it.

### 2026-10-21 - `feat:` the governance verdicts - the tool can refute, not just answer

**Milestone M3.** A retrieval tool informs. This one **refutes**. Three of the
four verdicts are now reachable through the single DX tool.

**What was built (DX).**

| File | What it is |
| --- | --- |
| `src/mcp/dx_governance.h` | The four verdicts and their contracts. |
| `src/mcp/dx_governance.c` | The verdict **policy**. Depends on yyjson alone - no store, no tree-sitter, no SQLite - so every verdict is verifiable without a graph. |
| `tests/test_dx_governance.c` | 23 contract tests. |
| `src/mcp/mcp.c` | The handlers, and the three request types the surface routes to them. |
| `src/mcp/dx_surface.c` | The new request types: `verify`, `certificate`, `enforce`. |

**The rule the whole module follows: never certify what cannot be checked.**

| Request | Answers |
| --- | --- |
| `verify` | `confirmed` / `refuted` / `unverifiable`, with a citation receipt. The subject is resolved through the base's own snippet lookup in **outline** mode - a verdict needs a location, not the code. |
| `certificate` | `clean` / `opened`: did this change **open a new path** into a declared boundary? Reachable after, not before, and inside the boundary. |
| `enforce` | The commit gate: `held` or not, with blocking, new-frozen, accepted-frozen, advisory and off findings separated. |
| `certify_public_surface` | **Policy done, not wired.** The graph side needs the diff's changed exports and their consumers. |

**The three decisions that carry the weight.**

1. **An unresolved subject is "unverifiable", not "refuted".** The graph not
   knowing is not evidence that a claim is false. A resolved subject with no
   located evidence is also unverifiable - the base's own agent instructions
   say to treat an unknown as unresolved, never as an all-clear.
2. **A path that was already reachable is not an opening.** The certificate is
   differential. A path that merely moved is not a new path, and a boundary
   prefix matches on a **segment**, so `/src/api` covers `/src/api/x.c` and
   not `/src/apix.c`.
3. **A frozen kind the baseline already accepts is adopted debt, not a
   failure.** That is the difference between freezing a rule and failing a
   build. And an unrecognised action is **advisory, never blocking**: an
   unknown policy must not fail a build.

**Nothing is called safe on a shrug.** An export whose compatibility is missing
or unrecognised is `potentially-breaking`, and a breaking export with no known
consumer is still reported as breaking - the absence of a known consumer is not
evidence that there is none.

**Executed.**

| Check | Result |
| --- | --- |
| `dx_governance.c` compiles clean under the project flags | exit 0 |
| The governance contract tests pass | **23 passed, exit 0** |
| `dx_surface.c` compiles clean and its contract tests pass | exit 0, **34 passed** |
| `mcp.c` and `dx_governance.c` pass `-fsyntax-only` with the project include set | exit 0 |
| The full server build | **NOT RUN** - the tree-sitter runtime and the 162 vendored grammars are not built in this environment |

**Two real bugs the tests caught, and one that was not a bug.**

- The first cut of the module used `yyjson_obj_foreach` on **arrays**. The
  macro walks an object's members, so every array came back empty. Twelve
  tests failed. Worse, two tests *passed* for the wrong reason - they expected
  a clean result and got one from an empty iteration. A green suite is not
  proof; the array cases are now the ones that would have caught it.
- A boundary-prefix case failed because the **test** was wrong, not the code:
  it added a *new* path inside the boundary and expected "clean", when a new
  path inside a boundary is exactly an opening. The test was split into two -
  the "nothing new" case and the "only the new path counts" case.
- A naming trap worth recording: `yyjson_arr_foreach` is for arrays and
  `yyjson_obj_foreach` is for objects. The compiler does not catch the
  mistake, because both are macros and both compile.

### 2026-10-21 - `feat:` the answer policy - relationships by default, and a budget that gates

**Milestone M2, landed inside M1.** The shape of the answer is decided at the
surface, so it belongs with the surface.

**What was built (DX).**

| Piece | Where | What it does |
| --- | --- | --- |
| `request.source` | `src/mcp/dx_surface.c` | `"outline"` (the default) or `"full"`. `outline` answers with relationships and a `file:line` pointer, so the answer does not grow with the repository. `full` asks for the source text and costs what the source costs. |
| `request.max_tokens` | `src/mcp/dx_surface.c` | The caller's budget. An answer that does not fit is **refused**, not truncated. |
| The gate | `dx_surface_apply_budget` | Measures the composed answer and refuses it when it does not fit, naming the excess, the token count and the budget. |
| `shape` | `dx_surface_shape_json` | Every answer reports the source policy in force, the token count, the budget, whether it was over, the excess, and how many source bodies it included. |

**The base was further along than this feature assumed, and the record says so.**
Every base tool the DX surface can reach already advertises `max_output_tokens`
(default 3200, minimum 128, maximum 1,000,000), the cap is applied on a
deterministic **UTF-8 byte ceiling of four bytes per token**, it is applied only
at whole semantic-unit boundaries, and when the first unit does not fit it
refuses and says *"raise max_output_tokens"* rather than truncating mid-unit.
`get_code_snippet` already has `source_mode: auto | full | outline` and reports
the mode it used. So the base caps; what it did not do is make the **choice**
explicit or price it. The DX surface therefore:
- defaults the shape to `outline` and makes `full` an explicit opt-in,
- adds a **caller-level** gate on top of the base's tool-level cap, and
- reports what the answer carried and what it cost.

**One rate, two files.** The DX surface estimates tokens at four UTF-8 bytes per
token - the base's own stated contract (`MCP_OUTPUT_BYTES_PER_TOKEN_ESTIMATE` in
`src/mcp/mcp.c`). A budget that means two different things in two files is not
a budget, so the constant is named in one place and documented in both.

**Translation, not duplication.** `source` and `max_tokens` are DX fields, so
neither is forwarded to the base verbatim. The surface translates them into the
base's own vocabulary - `source_mode` and `max_output_tokens` - and a caller
who set the base's parameter directly keeps their value, because an explicit
parameter beats a default.

**A refusal that looks like a refusal.** A DX refusal uses the base's own error
shape: the text in `content`, and `{"error": text}` in `structuredContent`. A
refusal that looked different from a base error would read as a different kind
of failure, and the agent would not know how to treat it.

**Executed.**

| Check | Result |
| --- | --- |
| The module compiles clean under the project flags | exit 0 |
| The contract tests pass | **33 passed, exit 0** |
| `mcp.c` and `dx_surface.c` pass `-fsyntax-only` with the project include set | exit 0 |
| The full server build | **NOT RUN** - the tree-sitter runtime and the 162 vendored grammars are not built in this environment |

**One API bug caught before it shipped.** The first cut of
`dx_surface_apply_budget` returned the caller's own pointer when the answer fit
and a fresh string when it did not. A function whose return is sometimes owned
and sometimes borrowed is a bug waiting to happen, so it now always returns a
fresh buffer and the caller always frees it.

### 2026-10-21 - `feat:` the DX surface - one typed tool over the code graph

**Milestone M1 of [`dx/PLAN.md`](dx/PLAN.md).** The first DX feature is landed:
the 17-tool menu is replaced, for DX sessions, by **one tool whose schema makes
the reasoning a required argument**.

**Why this, and not a new index.** A measured autopsy of the base and its two
competitors (`ttsc`, 2026-06-30) found that on the GPT-5.6 grid with the `zod`
repository, `codebase-memory-mcp` spent **66% more tokens than running with no
MCP server at all** (worst cell +82%), `codegraph` 4% more and `serena` 130%
more, while the compiler-backed `@ttsc/graph` cost **11% of baseline**. Their
conclusion: *"In the runs I measured it called the MCP zero times and fell back
to the shell every time. It gave up and grepped. The graph was right there; the
surface buried it."* An independent benchmark agrees from the other side - a
thin wrapper around the base was **cheaper than the raw server in every
setting** (`nefayran/code-graph-vs-grep`, 2026-10-01, p <= 0.021).

**What was built (DX).**

| File | What it is |
| --- | --- |
| `src/mcp/dx_surface.h` | The module's public interface: parse, dispatch target, wrap. |
| `src/mcp/dx_surface.c` | The implementation. Depends on `yyjson` and the C standard library only - no store, no tree-sitter, no SQLite - so the contract is verifiable without the full build. |
| `tests/test_dx_surface.c` | 19 contract tests, in the house framework. |
| `dx/PLAN.md` | The master plan: five features, the order, the measurement commitments. |
| `dx/F1-SURFACE.md` | This feature's contract: the envelope, the validation rules, the dispatch table, the test matrix. |

The envelope is `question`, `draft{reason,type}`, `review` and
`request{type,...fields}`. All four are **required**: a missing one is
rejected at the call boundary, never answered from a default. `request.type` is
a discriminated union over `search | trace | impact | details | outline |
architecture | schema | coverage | changes | escape`, and `escape` is a
first-class no-op for a question that is not a graph question. The request's own
fields are forwarded **verbatim** to the underlying base tool, so the module
owns no per-tool field mapping and cannot drift from the base tools' schemas.
The response gains two fields the base never had: `audit` (what was checked)
and `next` (answer, inspect, or escape).

**Wiring (DX, in the upstream tree).**

- `src/mcp/mcp.h` - a fourth tool profile, `CBM_MCP_TOOL_PROFILE_DX`, which
  exposes exactly one tool.
- `src/mcp/mcp.c` - the `dx_codebase` tool definition, the
  `handle_dx_codebase` handler, the `dx` branch of the profile filter, the
  `--tool-profile=dx` flag, and the dispatcher split so the DX surface can
  reach the base tools the profile does not expose directly. The request-type
  table is that allowlist: the mutating base tools are not reachable from it.
- `Makefile.cbm` - `src/mcp/dx_surface.c` in `MCP_SRCS`, `tests/test_dx_surface.c`
  in `TEST_MCP_SRCS`.
- `tests/test_main.c` - the suite is declared and run with the rest.

**What was executed, and what was not.**

| Check | Command | Result |
| --- | --- | --- |
| The contract compiles clean under the project's flags | `gcc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-sign-compare` on `src/mcp/dx_surface.c` | exit 0 |
| The contract tests pass | the same compile plus `tests/test_dx_surface.c`, then the binary | **19 passed, exit 0** |
| The wiring compiles clean | `gcc -fsyntax-only` with the project's include set on `src/mcp/mcp.c` and `src/mcp/dx_surface.c` | exit 0 |
| The full server build | `make -f Makefile.cbm test` | **NOT RUN** - the tree-sitter runtime and the 162 vendored grammars are not built in this environment |
| The suite inside the house test binary | `tests/test_main.c` | **NOT RUN** - same reason |

The standalone compile is not a substitute for the full build, and this entry
does not claim one. The module was written to depend on nothing but `yyjson`
precisely so that the contract could be *executed* here; the wiring is
compile-verified and awaits the full build.

**One real bug the tests caught.** The first version of the schema test looked
for `request` at the schema root rather than under `properties`, and then for the
enum one level too shallow. The test failed, the test was wrong, and the test
now walks a dotted path (`dx_path`) so the mistake cannot recur.

### 2026-10-21 - `chore:` the fork is established as `essence-dx/codebase`

**The DX layer is created. The upstream base is untouched.**

**What was done.**

- `G:\Dx\codebase` is now this repository's root. The previous
  `G:\Dx\codebase\codebase-memory-mcp\` level was removed and its contents -
  including `.git` - were moved up one level, so the folder layout is the
  upstream project layout with no wrapper.
- `essence-dx/codebase` was created as a **real GitHub fork** of
  `DeusData/codebase-memory-mcp` (public, default branch `main`).
  `gh repo view essence-dx/codebase --json parent` returns
  `{"name":"codebase-memory-mcp","owner":{"login":"DeusData"}}`.
- Remotes are set the standard fork way: `origin` is
  `https://github.com/essence-dx/codebase.git` and `upstream` is
  `https://github.com/DeusData/codebase-memory-mcp.git`.
- The local checkout was fast-forwarded from `a2227157` to `3457b21b`. The
  clone was 74 commits behind upstream; upstream landed 74 commits between
  2026-10-20 06:34 and 2026-10-21 03:44.
- The repository slug carries no second `dx` prefix: the organisation is
  already `essence-dx`, so the repo is `essence-dx/codebase`, not
  `essence-dx/dx-codebase`.

**DX files added.**

| File | What it is |
| --- | --- |
| `TECH.md` | The decision record: what this fork is, why this upstream and not the other one, the five features that make it a DX product, and the evidence index. |
| `TODO.md` | The DX work ledger. 12 items, each with evidence and a done-when check. |
| `CHANGELOG.md` | This file. |

**The other candidate was removed, and here is the record.**

`G:\Dx\codebase\codegraph` - a clean checkout of `colbymchenry/codegraph` at
`8beab37`, 1550 commits, 204.5 MB - was deleted on 2026-10-21 under explicit
user approval, after the comparison in `TECH.md` decided this fork's base.

| Field | Value |
| --- | --- |
| Target | `G:\Dx\codebase\codegraph` (entire tree) |
| Size | 204.5 MB, 1550 files |
| State at deletion | clean working tree, 0 uncommitted changes |
| Upstream | `https://github.com/colbymchenry/codegraph` |
| Regenerate | `git clone https://github.com/colbymchenry/codegraph` - it restores at `8beab37` |

Nothing unique was lost: the working tree was clean and every commit was
already pushed to the upstream remote. The comparison evidence it carried is
preserved in `TECH.md`, cited against the upstream commit
`colbymchenry/codegraph` at `8beab37`.

**No upstream source was modified.** `git diff --stat upstream/main..main` shows
exactly four files, all of them DX additions - 488 insertions, 0 deletions:

```
 .gitignore   |   6 ++
 CHANGELOG.md |  84 ++++++++++++++++++++++++++++++++++++++++++++++++++++++
 TECH.md      | 183 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 TODO.md      | 215 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
```

The one upstream file touched is `.gitignore`, and only by **adding** six lines:
upstream ignores `CHANGELOG.md` at line 61 because it publishes release notes on
GitHub instead, so this fork negates that rule in a marked DX block. Without the
negation a plain `git add -A` silently drops the ledger entry that the
same-commit rule requires - the exact failure the hub has been caught with. The
upstream `LICENSE`, `THIRD_PARTY.md` and `DCO` are unchanged.

**Not claimed.** The fork has no release, no `dx` route, no `TOOL_REGISTRY`
entry, and no `docs/` register page. Those are `CB-01` through `CB-04` in
`TODO.md`, and `CB-01` is blocked on the first release.

---

MIT OR Apache-2.0
updated: 2026-10-21 00:00 UTC
