# TECH.md - `essence-dx/codebase`: what this repository is and why

**What this repository is.** A fork of `DeusData/codebase-memory-mcp` - the native C
code knowledge graph for AI coding agents - kept as a **DX product** under the
`essence-dx` GitHub organisation. The upstream base is preserved verbatim and
tracked as the `upstream` remote; our fork is `origin`.

**What it is not.** It is not a rewrite. It is not a DX-authored graph engine. Every
line of the indexer, the store, the Cypher engine, the 162 tree-sitter grammars and
CBM Lens is upstream's work under the MIT licence. What DX adds is on top, and
`TODO.md` is the ledger for that.

**How to cite this file's evidence.** Paths are relative to this repository's root.
Line numbers into `README.md` are pinned to upstream commit `3457b21b` (2026-10-21);
that file is churned hard by upstream - 74 commits landed in a single day - so
re-derive a line number before quoting it.

---

## Why this fork exists, and not the other one

Two code-graph MCP servers were evaluated side by side in `G:\Dx\codebase` on
2026-10-21. The comparison is recorded here because it is the reason this
repository exists and because the losing candidate was deleted afterwards.

| | `codebase-memory-mcp` (chosen) | `codegraph` (removed) |
| --- | --- | --- |
| Upstream | `DeusData/codebase-memory-mcp` | `colbymchenry/codegraph` |
| Stack | Pure **C**, no language runtime | TypeScript + a Rust napi kernel |
| Languages | 162 vendored grammars | 34 |
| MCP tools | 17 | 8, of which 1 is exposed by default |
| Query language | A read-only openCypher subset | FTS5 plus a custom parser |
| Type resolution | "Hybrid LSP" - 11 language rows | Name matching + 35 framework resolvers + synthesis |
| Extra subsystems | Semantic embeddings, ADR store, docs-in-graph, Smart CI, CBM Lens. Trace ingestion is a **contract, not a feature** - see below | None of those |
| Licence | MIT | MIT |

**The decisive differences.**

1. **It has the memory layers the others do not.** Measured against eight layers -
   persistent index, structural graph, semantic retrieval, decision memory, runtime
   memory, cross-repo memory, visual memory, documentation memory - this fork's
   base has **five**. The removed candidate had **two**: a persistent index and a
   structural graph. It had no embeddings (its own source says "deterministic, no
   embeddings", `colbymchenry/codegraph` at `8beab37`, `src/mcp/tools.ts:3926`), no
   ADR storage (zero occurrences of "ADR" or "architecture decision" in its `src/`),
   no trace memory and no cross-repo model.

   **The runtime-trace layer does not count.** Upstream's own contract document
   states that `ingest_traces` "returns `status="accepted"`, `traces_received`,
   and an unimplemented note; this is not an accepted durable mutation, and the
   handler does not open or mutate a store"
   (`docs/RUNTIME_TRACE_MODEL.md:50-53`). The design is thorough - versioned
   identities, commutative aggregation, a composite publication head - but the
   implementation is a stub. A layer that cannot store anything is not a layer.

   **A second, sharper finding.** Upstream's measurement guide sets a rule the
   rest of the field breaks: `docs/MEASURING_SAVINGS.md:277` - *"Never turn a
   result from one repository, question set, model, or machine into a universal
   savings claim."* The same guide forbids substituting the tool's own query
   counter for agent tool calls, forbids inferring a missing measurement
   window, and forbids pseudocounts.

   The base now mostly obeys its own rule: `README.md:473` leads with the
   31-repository preprint's "10x lower token use and 2.1x fewer tool calls" and
   demotes the older five-query figure to "an earlier comparison" with a pinned
   commit. That is the right shape.

   What the base does **not** do is measure against anyone else's harness. The
   10x is a self-reported number from the project's own paper, and the
   independent measurements disagree with it in both directions. So this fork
   inherits the rule and not the number, and every performance claim it makes
   must name the repository, the question set, the model and the machine - or
   not be made.
2. **The graph reaches past the code.** Since v0.12.0 (2026-10-15) the graph carries
   documentation, architecture decisions, diagrams and API specs, and it links
   infrastructure - Kubernetes, Helm, Terraform, Ansible, Compose - to the program a
   container runs. The removed candidate stopped at the source file.
3. **One binary, no runtime.** The other needed Node plus a Rust kernel, and its
   browser viewer was unreleased and gated behind an environment variable.

**What the removed candidate did better, and what we keep.** It shipped **one** MCP
tool on purpose, because its own measurements showed a wide menu gets mis-picked
(`colbymchenry/codegraph` at `8beab37`, `AGENTS.md`). That is a good idea and it is
**TODO item `CB-05`** here.

---

## The five features that make this fork worth having

These are the reasons this is a DX product rather than a vendored reference. Each
one is a `TODO.md` item with a done-when check. None of them exists in the upstream
base, and none of them exists in the removed candidate.

**The plan is [`dx/PLAN.md`](dx/PLAN.md).** The order of work, the measurement
commitments and the milestone list are there. The first feature's contract is
[`dx/F1-SURFACE.md`](dx/F1-SURFACE.md).

**0. The finding that sets the order.** The base's graph is good; the base's
*surface* is what loses. A measured autopsy of all three candidates
(`ttsc`, 2026-06-30) found `codebase-memory-mcp` spending **66% more tokens
than running with no MCP server at all** on one grid, `codegraph` 4% more and
`serena` 130% more, while the compiler-backed `@ttsc/graph` cost **11% of
baseline**. Their words: *"The graph was right there; the surface buried it."*
An independent benchmark agrees from the other side - a **thin wrapper** around
the base was cheaper than the raw server in every setting. So the first
milestone is the interface, not the index.

**1. One tool, typed, with CoT-compliance.**
One MCP tool whose schema carries the reasoning as **required arguments**:
`question`, `draft{reason,type}`, `review`, `request{type,...}`. The rule that
makes it work: *"free prose can hide a skipped step; a typed submission
cannot."* `escape` is a first-class answer for a question that is not a graph
question, and the response carries an `audit` of what was checked and a `next`
step. **Landed 2026-10-21** - see `CHANGELOG.md`.

**2. The answer is the index, never the bodies, under a hard budget.**
Returning whole source bodies is the token bomb: the tool that does it spent
4% more than doing nothing. The answer should be relationships - names, edges,
signatures, spans - so its size does not grow with the repository, and the
budget should be a **gate** that fails with a non-zero exit code, not a warning.
The base already has the mechanism; it is simply not the default.

**3. The org graph, with a DX-native semantic layer.**
Upstream can link services across repositories (`README.md:349`,
`README.md:417`). It has no idea what a **DX tool**, a **DX rule** or a **DX
register row** is. This fork indexes the whole `G:\Dx` tree as one graph and
adds nodes and edges for the `dx` manifest routes, the `TOOL_REGISTRY` entries,
the `docs/<tool>.md` register, the `AGENTS.md` rule scopes, and the
path-dependency web between product folders - plus a conflict graph over every
in-flight branch, pull request and agent task. The question it answers is the
hub's actual question: *which repository owns this, which rule governs it, and
what breaks if it changes.*

**4. Governance as verdicts, and test impact that works on day one.**
Two halves of the same idea: the tool must be able to *refute* as well as
answer. `verify_claim` returns `confirmed` / `refuted` / `unverifiable` with a
citation receipt; `certify_public_surface` names the consumers a change breaks;
the certificate asks whether a diff **opened a new path** into a declared
boundary. Meanwhile upstream's Smart CI is honest about its limits - it narrows
only C and C++ whose tests follow upstream's conventions, and only with a
per-test coverage map you must build yourself, so "in any other repository the
engine answers 'run everything'." This fork derives the blast radius from the
graph alone, for Rust, TypeScript and Go, across every product folder, with no
coverage map - and delivers it as a 20-line skill, because in the one measured
study of that shape, a 107-line skill cut to 20 lines quadrupled an agent's
resolution rate while procedural instructions without the targeted context made
regressions *worse*.

**5. Anchored memory, and a tool that measures itself before you trust it.**
Every memory carries the hash of what it claims, so a recall reports `valid` /
`changed` / `gone` instead of a confident stale fact - the base's ADR store
cannot do that today. And `dx-codebase prove` measures the tool on a repository
the caller names, offline, with no API key, **including the case where it
loses**: a competitor publishes its own +43% loss on a small repo and says
*"Don't guess from our repos - run `openlore prove` on yours."* A tool that
refuses adoption without a measurement is DX's culture, not a bolt-on.

---

## What is in this repository today

| Path | What it is | Owner |
| --- | --- | --- |
| `src/` | The C backend: MCP server, CLI, daemon, store, pipeline, Cypher, watcher, traces | upstream |
| `src/mcp/dx_surface.c` | The DX single-tool surface: the envelope, the dispatch table, the response wrapping | **DX** |
| `dx/` | The plan and the feature contracts | **DX** |
| `internal/cbm/` | The 162 vendored tree-sitter grammars and the AST extraction engine | upstream |
| `graph-ui/` | CodeAtlasWeb - the React/Monaco/Three.js frontend behind CBM Lens | upstream |
| `pkg/` | Installers for npm, PyPI, Go, Homebrew, Scoop, AUR, Chocolatey, winget | upstream |
| `docs/` | Benchmark report, configuration, Smart CI, runtime trace model | upstream |
| `README.md` | The project's own README | upstream |
| `TECH.md` | This file - the DX decision record | DX |
| `TODO.md` | The DX work ledger | DX |
| `CHANGELOG.md` | What DX changed, dated | DX |

**Remotes.**

| Remote | Points at | Purpose |
| --- | --- | --- |
| `origin` | `https://github.com/essence-dx/codebase.git` | This fork. We push here. |
| `upstream` | `https://github.com/DeusData/codebase-memory-mcp.git` | The upstream base. We pull from here. |

`essence-dx/codebase` is a **real GitHub fork** of `DeusData/codebase-memory-mcp`
(public, default branch `main`), so GitHub's own "Sync fork" button and the
upstream link both work. Because the organisation is already `essence-dx`, the
repository slug carries no second `dx` prefix: the repo is `essence-dx/codebase`,
not `essence-dx/dx-codebase`.

---

## Licence and attribution

The upstream base is MIT (`LICENSE`, Copyright (c) 2025 DeusData). A fork may keep
the licence and the notice, and must not misrepresent origin. This repository
therefore carries the upstream `LICENSE`, `THIRD_PARTY.md` and `DCO` unchanged, and
every DX-added file says in its header that it is a DX addition on top of
`DeusData/codebase-memory-mcp`.

The removal of the second candidate is recorded in `CHANGELOG.md` with its size and
the command that regenerates it, per the hub's rule that nothing is deleted without
a written record.

---

## Evidence index

| Claim | Where to check |
| --- | --- |
| 17 MCP tools | `README.md:7`, `README.md:23` |
| 45 client integrations | `README.md:10`, `README.md:23` |
| 162 vendored grammars | `README.md:8`, `README.md:1069`, `internal/cbm/vendored/grammars/` (162 directories) |
| Pure C, no language runtime | `README.md:11`, `README.md:19` |
| Hybrid LSP, 11 language rows | `README.md:1044-1072` |
| openCypher subset | `README.md:946-956` |
| Node and edge vocabulary | `README.md:930-944` |
| Documentation in the graph | `README.md:323` |
| Infrastructure in the graph | `README.md:334` |
| Cross-repo intelligence | `README.md:349`, `README.md:417` |
| Smart CI and its C/C++ limit | `README.md:355`, `docs/SMART_CI.md` |
| Team-shared graph artifact | `README.md:419-440` |
| Source layout | `README.md:1086-1104` |
| Tool profiles (Scout / Analysis) | `src/mcp/mcp.c:1004-1043` |
| Semantic embeddings, 11 signals | `src/semantic/semantic.h:1-45` |
| No reranker in the upstream base | `grep -ri rerank src/` returns nothing |
| Storage root `~/.cache/codebase-memory-mcp` | `README.md:988` |
| Lens ships in the binary | `README.md:21`, `README.md:29` |
| `ingest_traces` is a stub, not a store | `docs/RUNTIME_TRACE_MODEL.md:50-53` |
| The base's own rule against universal savings claims | `docs/MEASURING_SAVINGS.md:277` |
| The base's current self-reported figure (10x, 31 repos) | `README.md:473` |
| The removed candidate's 1-tool doctrine | `colbymchenry/codegraph` at `8beab37`, `AGENTS.md` |
| The removed candidate's "no embeddings" | `colbymchenry/codegraph` at `8beab37`, `src/mcp/tools.ts:3926` |

---

MIT OR Apache-2.0
updated: 2026-10-21 00:00 UTC
