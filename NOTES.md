# Traverse

Notes on the data structures, the synthetic families, and the Wikipedia talk network.

The graphs in this project are undirected. An edge is a symmetric relation in every family below (a cycle, a clique, a heap tree, a residue class, and a Wikipedia talk tie after one of the three rules in section 4). Connected components are the ordinary undirected ones. The same three functions are used for every graph.


1. How a graph is stored

A graph is an adjacency list, implemented in graph_operations.cpp. There is no graph library.

  vertices          a vector of the vertex ids that exist
  adj[v]            a vector<int> of the neighbors of v
  present[v]        a byte, so a truncated heap can keep the labels m..n-1
                    without inventing fake vertices 0..m-1

An undirected edge {u, v} is stored once at each endpoint. After the edges are inserted, each neighbor list is sorted and duplicates are removed, so the graph is simple. Self-loops are ignored. The algorithms call that cleanup themselves.

Vertex ids are integers, so the searches mark vertices with arrays indexed by id (color, parent, distance). That is O(1) per mark. A std::set of visited vertices would add a log factor on every step, and a recursive depth-first search would overflow the call stack on the million-vertex runs, so both searches are iterative.

The three functions:

  connected_components(graph)
      Iterative depth-first search. Returns a vector of components.
      Each component is a vector of vertex ids. Time O(V + E), extra
      memory O(V). Order is not significant.

  one_cycle(graph)
      Iterative depth-first search with the usual white / gray / black
      colors. A neighbor that is still gray, and is not the parent, is
      a back edge to an ancestor. The tree path from the current vertex
      up to that ancestor, followed by the back edge, is the cycle.
      The returned list starts and ends on the same vertex and has at
      least three distinct vertices. If the graph is acyclic the list
      is empty. Any one cycle is returned. Time O(V + E) and it stops
      at the first cycle.

  shortest_paths(graph, source)
      Dijkstra with every edge weight equal to 1. The priority queue is
      the STL binary heap. The heap has no decrease-key, so a vertex may
      be pushed more than once and stale entries are skipped. With
      positive weights the first time a vertex is popped its distance is
      final. Time O((V + E) log V).
      The result is a std::map from vertex id to a path. For each vertex
      v that can reach the source, the path lists vertices from v back to
      the source, including both ends. The path from the source to itself
      is the single id [source]. Unreachable vertices are left out of the
      map. Several shortest paths may exist; only one is kept.

The map keeps one path per reachable vertex. It is also the expensive part of the large runs: storing a path of length L for every vertex costs far more than the search itself when L grows with V. Section 3 measures that.

graph_storage_bytes is the neighbor ids, the vertex-id table, and the vector headers. It does not include malloc overhead. Peak resident set is the process high-water mark from getrusage (ru_maxrss, which is bytes on macOS) and does include overhead, the code, and the algorithm's output.

CPU time is user + system time from getrusage. A short call is repeated until about 0.2 seconds have accumulated, at most five times, and the reported time is the average. Resident set is sampled after the first call, before a repeat can hold two results at once.


2. Simulated graphs

All six families are in graph_simulator.cpp. Vertices use each family's natural labels. Every check below was also asserted by simulated_test, not only printed.

  n-cycle            one component, n edges, one cycle of length n,
                     shortest-path hops at most n/2
  complete K_n       one component, n(n-1)/2 edges, hops at most 1,
                     many cycles (the search returns a triangle)
  empty              n components, no edges, no cycle,
                     the only path is the source itself
  heap               one tree, n-1 edges, no cycle,
                     the path from v to the root follows parents (v-1)/2
  truncated heap     vertices m..n-1, no cycle.
                     When n >= 2m+1 there are m+1 components and
                     n-1-2m edges, which is exactly
                     (number of vertices) - (number of components)
  equivalence mod k  k cliques, one per residue. Hops inside a clique
                     are 1. There is no path between different residues.

Sample output from `./simulated_test samples`:

=== 6-cycle ===
vertices=6 edges=6
components=1
  component 0 (6): 0 1 2 3 4 5
cycle (6): 5 4 3 2 1 0 5
shortest paths from 0 (6 reachable)
  0: 0
  1: 1 0
  2: 2 1 0
  3: 3 2 1 0
  4: 4 5 0
  5: 5 0

The longest hop count is 3, which is n/2. Vertex 4 goes the short way around, through 5, not the long way through 1, 2, and 3.

=== complete K5 ===
vertices=5 edges=10
components=1
cycle (3): 2 1 0 2
shortest paths from 0: [0], and [v 0] for every other v

=== empty n=4 ===
components=4, each a single vertex
cycle: none
shortest paths from 0: only [0]

=== heap n=7 ===
vertices=7 edges=6
components=1
cycle: none
  0: 0
  1: 1 0
  2: 2 0
  3: 3 1 0
  4: 4 1 0
  5: 5 2 0
  6: 6 2 0

=== truncated heap m=1 n=7 ===
vertices=6 edges=4
components=2
  {1, 3, 4} and {2, 5, 6}
cycle: none
From source 1 the paths are [1], [3 1], [4 1]. Vertex 2 is a different component, so it is absent from the map. m+1 = 2 components and n-1-2m = 4 edges.

=== equivalence mod 3 on n=9 ===
components=3
  {0, 3, 6}, {1, 4, 7}, {2, 5, 8}
cycle (3): 6 3 0 6
From 0 the paths are [0], [3 0], [6 0]. Vertex 1 is not reachable.


3. Running time and memory on the simulated graphs

Compiler: Apple clang 21, -O2, -std=c++17. Machine: 16 GB RAM.
Each row is its own process. The full machine-readable log is benchmark.csv.
Times are seconds. graph_bytes and peak_rss are bytes.
A -1 means that algorithm was not run in that process, so its output could not inflate the resident set.

n-cycle. Components and the cycle search are a separate process from shortest paths, because the path map is the thing that grows.

  n        build     components   cycle      peak_rss (search only)
  1000     0.000043  0.000005     0.000010   1.8e6
  10000    0.000315  0.000044     0.000089   2.7e6
  100000   0.003098  0.000394     0.000878   9.2e6
  1000000  0.033650  0.004031     0.008704   7.3e7

Component time goes up by about 10x when n goes up by 10x. That is the O(V + E) scan with E = n. The cycle search is the same shape and a bit slower, because it also records parents. At n = 10^6 the search's peak resident set is 72 MB against 37 MB of adjacency data.

Shortest paths on the same cycle. The path map stores every path, and on a cycle the sum of the path lengths is n^2/4 + n vertices. path_bytes is that count times 4, and it matches exactly (400,080,000 bytes at n = 20000).

  n       time      max hops   path_bytes    peak_rss
  1000    0.000417  500        1.004e6       3.3e6
  5000    0.007636  2500       2.502e7       4.0e7
  10000   0.031003  5000       1.000e8       1.5e8
  20000   0.124159  10000      4.001e8       5.4e8

Doubling n multiplies the time and the path storage by 4. Dijkstra itself is nearly linear here (E = n); the quadratic term is the map of paths. Peak resident set is about 1.3 times the raw path bytes. The gap is the per-path heap allocation inside std::map, not the search. n = 10^6 was not run for this function: the path map would be about n^2/4 integers, on the order of a terabyte.

Complete graph. Build is the quadratic step. Every distance is 0 or 1, so the path map stays tiny and the cycle that comes back is a triangle (list length 4).

  n     edges    build     components  cycle      shortest   peak_rss
  50    1225     0.000025  0.000001    0.000001   0.000007   1.8e6
  100   4950     0.000061  0.000004    0.000000   0.000013   1.9e6
  200   19900    0.000205  0.000013    0.000001   0.000031   2.2e6
  400   79800    0.000718  0.000052    0.000001   0.000081   3.3e6
  800   319600   0.002637  0.000185    0.000001   0.000231   7.7e6

Doubling n multiplies the edge count by 4 and the build time by about 3.6. Component time tracks the edge count. The cycle search does not: it returns as soon as it sees the first triangle, so it stays near the noise floor while the graph gets denser.

Empty graph. There is nothing to search, but connected_components still allocates one vector per vertex.

  n        components time   component count   graph_bytes   peak_rss
  1000     0.000025          1000              2.9e4         1.9e6
  10000    0.000163          10000             2.9e5         3.4e6
  100000   0.001766          100000            2.9e6         1.6e7
  1000000  0.015805          1000000           2.9e7         1.4e8

At n = 10^6 the adjacency data is 29 MB and the process peaks at 141 MB. The extra is a million separate component vectors. Shortest paths stay at one entry, the source, and the cycle list stays empty. The cycle and path times at this size are 0.0022 s and 0.0003 s.

Heap. One tree, no cycle, every vertex reachable from the root. The deepest path has floor(log2(n - 1)) hops: 9, 13, 16, 19 at the four sizes below.

  n        build     components  cycle     shortest   path_bytes   peak_rss
  1000     0.000036  0.000006    0.000006  0.000123   3.6e4        2.0e6
  10000    0.000317  0.000045    0.000061  0.001542   4.9e5        3.9e6
  100000   0.003363  0.000398    0.000634  0.019236   6.3e6        2.4e7
  1000000  0.033114  0.004272    0.006131  0.220843   7.6e7        2.6e8

Component time again tracks n. Shortest-path time grows faster than n because each path is about log n long and each one is its own allocation, but it is nowhere near the cycle's quadratic. At n = 10^6 the path payload is 76 MB and the process peaks at 257 MB.

Truncated heap. extra is m. The formulas checked out on every row: components = m + 1, edges = n - 1 - 2m, cycle length 0.

  n        m       vertices  edges   components  build     comp. time  peak_rss
  1000     10      990       979     11          0.000050  0.000007    2.0e6
  10000    100     9900      9799    101         0.000416  0.000050    2.8e6
  100000   1000    99000     97999   1001        0.003899  0.000476    1.0e7
  1000000  10000   990000    979999  10001       0.040944  0.004953    8.9e7

Shortest paths were started at vertex m, which is a root (its heap parent is less than m). In these four shapes that root's subtree has 127 vertices and depth 6, so the map has 127 paths and max hops 6. The other roots are separate components and correctly do not appear. Paths that exist are short, because a heap path follows parent links and those links are shallow. Component time still grows linearly with n.

Equivalence mod k. extra is k. Each class is a clique of size n/k, so the hop count is 1 and the cycle that comes back is a triangle.

  n      k    edges      build     components time   component count   peak_rss
  600    3    59700      0.000594  0.000040           3                 2.7e6
  2000   5    399000     0.003703  0.000324           5                 6.8e6
  8000   20   1596000    0.014581  0.001203           20                1.9e7
  20000  50   3990000    0.037996  0.002901           50                4.5e7

Build time follows the number of edges. From the third row to the fourth, edges grow by 2.5 and build time grows by 2.6. The path map stays small because every reached vertex is a neighbor of the source.

What the simulated runs say, taken together:

  Sparse scans (cycle, heap, truncated heap) are linear in V + E.
  Building a dense graph (complete, mod k) is linear in the number of edges, which is quadratic in the clique size.
  The cycle search on a dense graph returns immediately; on a pure cycle it walks the whole graph.
  The cost that actually hurts is materializing every shortest path when those paths are long. On the cycle that output is Theta(n^2) and both time and resident set follow it. On the heap the paths are Theta(log n) and a million vertices still fit in a few hundred megabytes.
  An empty graph is cheap to store and expensive only if the caller asks for a million separate component vectors.


4. Wikipedia talk network

Source. Stanford SNAP, wiki-Talk:

  https://snap.stanford.edu/data/wiki-Talk.html
  https://snap.stanford.edu/data/wiki-Talk.txt.gz

The graph is the communication network of Wikipedia. A vertex is a user. A directed arc from i to j means i wrote on j's talk page. SNAP cites J. Leskovec, D. Huttenlocher, and J. Kleinberg, "Signed Networks in Social Media," CHI 2010. The page lists 2,394,385 nodes and 5,021,410 arcs, and a largest weakly connected component of 2,388,953 nodes. That is the real graph used here.

The file is directed. This project keeps its graphs undirected, so the loader does not run the algorithms on the arcs themselves. realgraph.cpp turns the arc list into an undirected graph under three rules, which are named in comments above the three functions:

  Criterion 1. Adjacent if at least one directed arc exists. Direction is discarded.
  Criterion 2. Adjacent only if both directed arcs exist. Each user wrote on the other's talk page.
  Criterion 3. Adjacent if exactly one directed arc exists. A reciprocal pair is not an edge.

Criterion 2 is the primary one. A single talk-page edit is a weak contact. A reply in both directions is a conversation.

Preprocessing, in prepare_realgraph.sh and load_relationship_file:

  Download wiki-Talk.txt.gz and decompress it.
  Skip lines that start with #. The stats line in the header is "Nodes: 2394385 Edges: 5021410".
  Read each remaining line as two integer ids.
  Drop self-loops and duplicate arcs. This file had none of either: 5,021,410 data lines, 5,021,410 unique arcs.
  Collect the distinct ids and compress them to 0 .. n-1. The searches index arrays by vertex id, so a sparse id space would otherwise allocate out to the largest raw id. In this file the ids are already 0 through 2,394,384, so the labels do not change.
  Keep all 2,394,385 users as vertices under every rule. A stricter rule isolates people; it does not delete them.

The three undirected edge sets partition the pairs. Let R be the number of reciprocal pairs and O the number of one-way pairs. Then the directed arc count is 2R + O, and the criterion-1 edge count is R + O. The run produced

  R = 361,845
  O = 4,297,720
  R + O = 4,659,565
  2R + O = 5,021,410

which matches the arc count in the header. Criterion 1's largest component has 2,388,953 vertices, the same number SNAP publishes for the largest weak component. Those two checks are why I trust the loader.

Each rule was a separate process (run_realgraph.sh), so the peak resident set belongs to that rule. Times are CPU seconds from getrusage, same definition as section 3. The build flags are the same ones: -std=c++17 -O2.

  rule          edges      storage     components   largest     build     cc       cycle    paths    peak rss
  1 any arc     4,659,565  106.7 MB    2,555        2,388,953   0.631 s   0.049 s  0.0007 s 0.529 s  592,003,072
  2 reciprocal    361,845   72.3 MB    2,301,204       92,117   0.307 s   0.033 s  0.0006 s 0.019 s  409,092,096
  3 one-way     4,297,720  103.8 MB       20,339    2,370,807   0.614 s   0.046 s  0.0005 s 0.575 s  589,316,096

Reading the file cost about 0.62 s of CPU in each process. Process CPU, including the read, the build, and the three searches, was 1.93 s, 1.06 s, and 1.95 s.

Sample output for criterion 1 (the full text is output/wiki_any.txt):

  vertices: 2394385
  unique directed arcs: 5021410
  undirected edges: 4659565
  connected components: 2555
  largest component: 2388953
  one cycle length: 3 original ids: 5 2 1 5
  shortest paths from original id 0: reachable=2388953 max_hops=7
  one longest shortest path: 401260 401259 387656 23359 5622 302 2956 0

Criterion 2 returns a 4-cycle, original ids 22 5 4 3 22. Its longest recorded shortest path, from the far user back to user 2, is 12063 85052 85053 160756 8524 5 130 2. Criterion 3 returns the same triangle as criterion 1, 5 2 1 5, and a 7-hop path ending at user 0. Component sizes for all three rules are written under output/wiki_*_components.txt.

What changes with the rule. Criterion 1 is one block of 2,388,953 users and 2,554 small pieces holding the other 5,432. Nobody is isolated, because every id in the file touches an arc. Criterion 3 deletes the reciprocal edges and the block barely moves: 2,370,807 users, plus 17,485 isolates. Criterion 2 keeps only the reciprocal edges. The block falls to 92,117. The other 2,300,246 users are isolated, and the remaining mutual conversations are 957 components whose largest has 8 vertices.

So the weak connectivity of this network is carried by unanswered talk-page edits. Requiring a reply does not peel off a thin layer of isolates. It removes most of the edges (4.66 million down to 0.36 million) and leaves a core of about 92 thousand users. SNAP's largest strongly connected component is 111,881 vertices. That is a different question: a directed path each way, possibly through other people. This project does not compute strong components. The 92,117 figure is the undirected component of direct mutual edits, and it is smaller than the strong component, which is what you would expect if some directed cycles have no reciprocal edge.

The searches stay cheap here for the reason section 3 predicted. The graph is sparse, about two arcs per user, and the paths are short (7 hops from the sources above). path_bytes on criterion 1 is 45,078,760. Divided by four, that is about 4.7 integers per reached user: a handful of hops, not a quadratic map. Peak resident set is 592 MB against 107 MB of adjacency data. The gap is the arc table, one vector per shortest path, and allocator overhead, not a blow-up in the path lengths. SNAP lists the diameter of the directed graph as 9. The 7 hops above are from one source, not that diameter.


5. How to reproduce

  make check
  ./simulated_test samples
  ./run_experiments.sh
  ./run_realgraph.sh

make check does not need the wiki-Talk download. It runs the synthetic-graph checks and a hand-built directed arc list that distinguishes the three talk rules. run_realgraph.sh downloads wiki-Talk and runs the three adjacency rules.
