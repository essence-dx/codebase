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

**1. One tool, three trust tiers, as the response contract.**
Upstream ships 17 tools with Scout (8) and Analysis (13) profiles
(`src/mcp/mcp.c:982-1015`). The removed candidate shipped one tool for a measured
reason. This fork ships **one** MCP tool whose *answer shape* changes by tier:
`scout` returns names and paths, `verify` returns source plus the call path,
`auditor` returns the full answer with coverage and confidence. Best of both:
one tool to pick, three levels of depth to ask for.

**2. The org graph, with a DX-native semantic layer.**
Upstream can link services across repositories
(`README.md:349`, `README.md:417`). It has no idea what a **DX tool**, a **DX rule**
or a **DX register row** is. This fork indexes the whole `G:\Dx` tree as one graph
and adds nodes and edges for the `dx` manifest routes, the `TOOL_REGISTRY` entries,
the `docs/<tool>.md` register, the `AGENTS.md` rule scopes, and the path-dependency
web between product folders. The question it answers is the hub's actual question:
*which repository owns this, which rule governs it, and what breaks if it changes.*

**3. Test impact that works on day one, on every DX language.**
Upstream's Smart CI is honest about its limits: it narrows only C and C++ whose
tests follow upstream's own conventions, and only with a per-test coverage map you
must produce yourself - "In any other repository the engine answers 'run everything'."
That is safe, but it saves nothing. This fork ships language-agnostic test discovery
(Rust `#[test]`, `#[tokio::test]`, vitest, jest, pytest, `go test`, cargo-nextest)
and derives the blast radius from the graph alone, across every product folder, with
no coverage map to build.

**4. Sufficiency receipts: every answer ships its blind spots.**
The removed candidate's own `AGENTS.md` states the whole product in one line: an
agent falls back to `Read`/`Grep` the instant a graph answer is insufficient. That
makes sufficiency the product, and neither candidate exposed it. This fork returns,
with every answer: the confidence, what the graph proved, the blind spots
(unresolved references by reason, documentation link families below their accuracy
bar, files that were skipped) and the single next question to read if the answer is
not enough. That is `AGENTS.md` section 4 claim tiers made executable.

**5. Offline two-stage retrieval: embed, then rerank.**
Upstream bundles `nomic-embed-code` (768d, int8) and scores with 11 signals, but
it has no reranker - a search for `src/` in the upstream tree finds **zero**
occurrences of "rerank", "reranker" or "cross-encoder". The removed candidate had no
embeddings at all, so its retrieval ceiling was lexical. This fork bundles the
embedder **and** a cross-encoder reranker: hosted-grade search precision with no
network, no API key and no model download at query time.

---

## What is in this repository today

| Path | What it is | Owner |
| --- | --- | --- |
| `src/` | The C backend: MCP server, CLI, daemon, store, pipeline, Cypher, watcher, traces | upstream |
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
| Tool profiles (Scout / Analysis) | `src/mcp/mcp.c:982-1015` |
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
