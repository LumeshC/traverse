#ifndef GRAPH_OPERATIONS_H
#define GRAPH_OPERATIONS_H

#include <cstddef>
#include <map>
#include <vector>

// Undirected simple graph.
//
// Every graph here is undirected. The synthetic families
// (cycle, complete, heap, equivalence classes) and the Wikipedia talk graph
// are symmetric relations, and "connected components" means the ordinary
// undirected components, not strongly connected components.
//
// Storage is an adjacency list: one vector of neighbor ids per vertex.
// An undirected edge {u, v} is stored twice, once at each endpoint.
// Vertices are non-negative integers and need not be a contiguous range
// (the truncated heap keeps the labels m .. n-1).
//
// After edges are inserted, finalize() sorts each adjacency list and drops
// duplicate neighbors. The algorithms call it themselves, so a caller cannot
// observe a multiedge. Duplicate edges would otherwise look like cycles.
//
// Why these containers:
//   vector adjacency  - O(degree) scans, contiguous, no per-edge node
//   vector<char> marks and parent arrays indexed by vertex id - O(1)
//   priority_queue    - binary heap for Dijkstra, O((V+E) log V)
//   map               - one shortest path per reachable vertex
// A std::set of neighbors would add an extra log factor on every scan, and
// a recursive DFS would blow the call stack on the million-vertex instances.
// The searches below are iterative.

class Graph {
 public:
  static Graph contiguous(int n);

  void ensureVertex(int v);
  void addUndirectedEdge(int u, int v);
  void finalize() const;

  const std::vector<int>& vertices() const;
  const std::vector<int>& neighbors(int v) const;
  int vertexCount() const;
  int indexLimit() const;
  bool hasVertex(int v) const;

 private:
  mutable std::vector<std::vector<int>> adj_;
  mutable std::vector<int> vertices_;
  std::vector<char> present_;
  mutable bool finalized_ = true;
};

// Depth-first search. Each inner vector is one component.
// Order of components, and of vertices inside a component, is not specified.
std::vector<std::vector<int>> connected_components(const Graph& graph);

// Depth-first search. Empty if the graph is acyclic.
// Otherwise a closed walk of at least three distinct vertices: the first and
// last entries are the same vertex. Any one cycle is fine.
std::vector<int> one_cycle(const Graph& graph);

// Dijkstra with every edge weight equal to 1.
// For each vertex v reachable from source, paths[v] is one shortest path
// from v back to source, including both endpoints.
// The path from source to itself is the single vertex [source].
// Unreachable vertices are omitted.
std::map<int, std::vector<int>> shortest_paths(const Graph& graph, int source);

long long count_undirected_edges(const Graph& graph);

// Bytes occupied by the vertex table, the adjacency-list headers, and the
// neighbor ids. Allocator overhead and the algorithm scratch space are not
// included; peak resident set is measured separately with getrusage.
std::size_t graph_storage_bytes(const Graph& graph);

#endif
