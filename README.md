# Traverse

A C++17 project for undirected graphs stored as adjacency lists. The same three algorithms run on every graph: connected components, one cycle, and unit-weight shortest paths.

The synthetic side builds six families (cycle, complete, empty, heap, truncated heap, and residue classes) and checks their structure. The Wikipedia side loads the SNAP wiki-Talk network, about 2.4 million users, and turns the directed talk-page edits into an undirected graph under three rules: at least one arc, both arcs, or exactly one arc.

## Look

Open [`demo/index.html`](demo/index.html). The top of the page is the Wikipedia result: the same 2.4 million users under the three edge rules, and the collapse to 92,117 people when an edge requires a reply. Under that, the six families play connected components, one cycle, and shortest paths on screen.

```sh
make demo
```

## Build

```sh
make
make check
```

`make check` runs the synthetic-graph checks and a small directed-arc self-test. No download is required for that.

## Synthetic graphs

```sh
./simulated_test samples
./run_experiments.sh
```

`run_experiments.sh` starts a fresh process for each size so the peak resident set belongs to that run. Rows land in `benchmark.csv`.

## Wikipedia talk network

```sh
./run_realgraph.sh
```

`prepare_realgraph.sh` downloads the SNAP wiki-Talk edge list. `realgraph.cpp` turns that directed arc list into an undirected graph under the three adjacency rules. `run_realgraph` runs connected components, one cycle, and shortest paths. Each rule is a separate process so its peak resident set is its own. Summaries are written under `output/`.

Downloaded edge lists stay in `data/` and are not part of the source tree.

## Files

| File | Role |
| --- | --- |
| `graph_operations.cpp` | Adjacency-list graph, connected components, one cycle, Dijkstra shortest paths |
| `graph_simulator.cpp` | The six synthetic families |
| `simulated_test.cpp` | Checks, sample output, and timed runs |
| `realgraph.cpp` | Wikipedia loader and the three talk-network adjacency rules |
| `run_realgraph.cpp` | Driver: components, one cycle, shortest paths |
| `NOTES.md` | Data structures, timings, and observations |
| `demo/index.html` | The Wikipedia split and the three searches, drawn |
