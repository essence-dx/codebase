# TODO.md - Open Work for `essence-dx/codebase`

**Snapshot taken 2026-10-21 against the tree, not restated from memory.** Every
item below carries the evidence that proves it is open and a `Done when` check
another agent can run without asking anyone.

**Scope:** the DX layer on top of `DeusData/codebase-memory-mcp`. The upstream
base is not re-implemented here and is not tracked here; its own backlog lives
upstream. Items that touch the upstream source tree are marked `UPSTREAM-AWARE`
and say so.

---

## How to use this file

- Take items in priority order: **P0 -> P1 -> P2**. `BLOCKED` items are not
  skipped, they are parked with the blocker named and re-checked.
- Cite the ID (`CB-01`, `CB-02`, ...) in the commit subject and in the
  `CHANGELOG.md` line.
- On completion: **remove the item here and add a `CHANGELOG.md` entry in the
  same commit**. Never leave both records.
- Do not file an item without evidence. Do not resolve an item without running
  its check.
- Nothing here authorises a delete. Where an item's remedy is removal, it stays a
  proposal until approved.

---

## Scoreboard

| Priority | Open | What it guards |
| --- | --- | --- |
| P0 | 4 | The tool is installable and reachable as `dx codebase` |
| P1 | 5 | The fork beats both evaluated upstreams |
| P2 | 3 | The fork stays maintainable as upstream moves |
| **Total** | **12** | |

---

## P0 - Make it a reachable DX tool

### CB-01 - Register the tool so `dx` can install it

**Evidence.** `TOOL_REGISTRY` in `cli/src/update_registry.rs:5` lists every DX
tool as `(name, repo, binary, path)`. It has no `codebase` entry, and it must
not have one yet: the registry drives the install contract, and this fork has
published no release, so there is no asset to install.

**Done when.** The first release of this fork exists, and the registry carries

```rust
("codebase", "essence-dx/codebase", "codebase", "bin/codebase"),
```

with `codebase` in the binary name - not `dx-codebase`. The organisation is
already `essence-dx`, so a second `dx` prefix in the slug is redundant. **This
also changes the release-asset convention** the other 20 tools follow
(`dx-<tool>-<target-triple>[.exe]`, proven by `essence-dx/media` v1.0.0). That
convention is a hub-level install contract, so this is a decision for the hub,
not for this repository - see `CB-11`.

**BLOCKED** on the first release (`CB-02`).

### CB-02 - Publish the first release from this fork

**Evidence.** `git tag` in this repository lists upstream's tags only; the fork
pushed no tag of its own. `essence-dx/codebase` has no release, so
`gh release list --repo essence-dx/codebase` is empty.

**Done when.** `gh release list --repo essence-dx/codebase` shows one release
whose assets are named for the six supported target triples, and the upstream
release workflow's SLSA and VirusTotal gates have run on it.

### CB-03 - Add the folder to the `dx` routing manifest

**Evidence.** The `paths` block of `G:\Dx\dx` has no `codebase` entry, so
`dx codebase` cannot route anywhere. The hub's own `AGENTS.md` section 3 names
the manifest as the source of truth for "where does tool X live on disk".

**Done when.** `dx` resolves the `codebase` path and `Test-Path` confirms the
resolved folder is this repository.

### CB-04 - Add the DX `docs/` register page

**Evidence.** `docs/README.md` carries one row per DX product and has no row
for `codebase`. The hub's `AGENTS.md` section 3 names `docs/<tool>.md` as the
canonical answer to "what has folder X actually achieved".

**Done when.** `docs/codebase.md` exists in the hub, `docs/README.md` has the
matching row, and every number in it has a `file:line` that points at this
repository.

---

## P1 - Make the fork beat both evaluated upstreams

### CB-05 - One tool, three trust tiers, as the response contract

**Evidence.** The upstream base ships 17 tools with two profiles - Scout (8
tools) and Analysis (13) - selected in `src/mcp/mcp.c:982-1015`. The removed
candidate shipped one tool because its own `AGENTS.md` records that a wide menu
gets mis-picked. Neither shape is right: a menu gets mis-picked, and a
single-purpose tool cannot answer every question.

**What to build.** One MCP tool whose *response shape* changes by tier -
`scout` (names and paths), `verify` (source plus call path), `auditor` (full
answer, coverage and confidence).

**Done when.** The server advertises exactly one tool, a `tier` argument selects
the response depth, and the three shapes are covered by tests that assert the
scout response fits a stated token budget.

### CB-06 - The org graph with a DX-native semantic layer

**Evidence.** The upstream base can link services across repositories
(`README.md:349`, `README.md:417`) and can follow a path dependency between
folders, but it has no model of a DX tool, a DX rule or a DX register row. The
hub's `AGENTS.md` section 8.1 lists the path dependencies that break a build in
another repository - a real question the graph should answer.

**What to build.** Index the whole `G:\Dx` tree as one graph, and add nodes and
edges for the `dx` manifest routes, the `TOOL_REGISTRY` entries, the
`docs/<tool>.md` register rows, the `AGENTS.md` rule scopes, and the
path-dependency web between product folders.

**Done when.** One query answers "which repository owns this symbol, which rule
governs it, and what else breaks if it changes", and the answer names the
repositories involved.

### CB-07 - Test impact that works on day one, on every DX language

**Evidence.** `docs/SMART_CI.md` is explicit: the test layer narrows only C and
C++ whose tests follow upstream's own conventions, and only with a per-test
coverage map the user must produce. "In any other repository the engine answers
'run everything'." The DX product folders are Rust, TypeScript and Go.

**What to build.** Language-agnostic test discovery - Rust `#[test]` and
`#[tokio::test]`, vitest, jest, pytest, `go test`, cargo-nextest - and a
blast-radius-to-test mapping derived from the graph alone, with no coverage map.

**Done when.** On a real diff in a Rust product folder, the tool names the
affected tests and says "run everything" with a reason when it cannot be sure -
and the "run everything" path is exercised by at least one test.

### CB-08 - Sufficiency receipts: every answer ships its blind spots

**Evidence.** The removed candidate's `AGENTS.md` states that an agent falls
back to `Read`/`Grep` the instant a graph answer is insufficient - that is the
whole product, and neither candidate exposed it. The upstream base already
collects the raw material: `index_status` reports unresolved references by
reason and the `doc_links` block counts the documentation link families that
did not clear their accuracy bar.

**What to build.** Every answer carries `confidence`, `covered`, `blind_spots`
and `next_question`.

**Done when.** A blind spot that exists in the index appears in the answer's
`blind_spots` list, and a test proves an answer with an unresolved reference
does not claim completeness.

### CB-09 - Offline two-stage retrieval: embed, then rerank

**Evidence.** The upstream base bundles `nomic-embed-code` (768d, int8) and
scores with 11 signals (`src/semantic/semantic.h:1-45`) but has **no**
reranker - `grep -ri rerank src/` returns nothing. The removed candidate had no
embeddings at all, so its retrieval ceiling was lexical.

**What to build.** A bundled cross-encoder reranker on top of the existing
embeddings: no network, no API key, no model download at query time.

**Done when.** A measured retrieval benchmark shows the reranked ranking beats
the embedding-only ranking on the same queries, with the command and the
machine named.

---

## P2 - Keep the fork maintainable

### CB-10 - Fix the upstream sync cadence

**Evidence.** Upstream landed 74 commits on this repository's `main` in one
day, 2026-10-20 to 2026-10-21. The local checkout was 74 commits behind until
it was fast-forwarded on 2026-10-21. Upstream's README was restructured in the
same window, which invalidated a dozen line-number citations in this
repository's own `TECH.md`.

**Done when.** A scheduled or documented cadence exists, and `TECH.md` cites
upstream line numbers against a named commit rather than against "the README".

### CB-11 - Get the hub's release-asset naming decision

**Evidence.** Every other DX tool publishes `dx-<tool>-<target-triple>[.exe]`
(`essence-dx/media` v1.0.0 is the proof in `DX.md:38`). This fork's tool is
named `codebase`, not `dx-codebase`, because the organisation is already
`essence-dx`. That breaks the pattern the hub's own installer depends on.

**Done when.** The hub states in writing whether `codebase` is the exception, or
the tool is renamed.

### CB-12 - Decide what happens to CBM Lens in a DX product

**Evidence.** CBM Lens is upstream's human interface, in open beta at v0.0.1
(`README.md:21`), with a browser-local model that downloads weights from
Hugging Face on first use. It is a 3D architecture map and Galaxy graph
(`README.md:29`) - upstream's own choice, not a DX requirement, and it is the
largest part of the release artifact.

**Done when.** A decision is recorded in `CHANGELOG.md` stating whether the
fork ships Lens, ships it disabled, or removes it, with the artifact-size
consequence named.

---

MIT OR Apache-2.0
updated: 2026-10-21 00:00 UTC
