#include "graph_simulator.h"

#include <stdexcept>
#include <vector>

Graph make_cycle(int n) {
  if (n < 0) {
    throw std::invalid_argument("cycle size must be non-negative");
  }
  // Vertices 0 .. n-1. Edge between u and v when u - v is ±1 or ±(n-1),
  // which is exactly the cycle that joins i to (i + 1) mod n.
  Graph g = Graph::contiguous(n);
  if (n >= 2) {
    for (int i = 0; i < n; ++i) {
      g.addUndirectedEdge(i, (i + 1) % n);
    }
  }
  g.finalize();
  return g;
}

Graph make_complete(int n) {
  if (n < 0) {
    throw std::invalid_argument("complete graph size must be non-negative");
  }
  Graph g = Graph::contiguous(n);
  for (int u = 0; u < n; ++u) {
    for (int v = u + 1; v < n; ++v) {
      g.addUndirectedEdge(u, v);
    }
  }
  g.finalize();
  return g;
}

Graph make_empty(int n) {
  if (n < 0) {
    throw std::invalid_argument("empty graph size must be non-negative");
  }
  return Graph::contiguous(n);
}

Graph make_heap(int n) {
  if (n < 0) {
    throw std::invalid_argument("heap size must be non-negative");
  }
  // Parent of v is (v - 1) / 2. Children of v are 2v+1 and 2v+2.
  // Linking every node to its parent produces each tree edge once.
  Graph g = Graph::contiguous(n);
  for (int v = 1; v < n; ++v) {
    g.addUndirectedEdge(v, (v - 1) / 2);
  }
  g.finalize();
  return g;
}

Graph make_truncated_heap(int m, int n) {
  if (m < 0 || n < 0 || m >= n) {
    throw std::invalid_argument("truncated heap needs 0 <= m < n");
  }
  // Vertices are the labels m .. n-1, not a shifted copy of 0 .. n-m-1.
  // An edge exists only when both the node and its heap-parent sit in range.
  Graph g;
  for (int v = m; v < n; ++v) {
    g.ensureVertex(v);
  }
  for (int v = m; v < n; ++v) {
    if (v == 0) {
      continue;
    }
    const int parent = (v - 1) / 2;
    if (parent >= m && parent < n) {
      g.addUndirectedEdge(v, parent);
    }
  }
  g.finalize();
  return g;
}

Graph make_equivalence_mod_k(int n, int k) {
  if (n < 0 || k < 1 || k > n) {
    throw std::invalid_argument("equivalence mod k needs 1 <= k <= n");
  }
  // u ~ v when k divides u - v. Each residue class is a clique.
  std::vector<std::vector<int>> classes(static_cast<std::size_t>(k));
  for (int v = 0; v < n; ++v) {
    classes[static_cast<std::size_t>(v % k)].push_back(v);
  }
  Graph g = Graph::contiguous(n);
  for (const auto& members : classes) {
    for (std::size_t i = 0; i < members.size(); ++i) {
      for (std::size_t j = i + 1; j < members.size(); ++j) {
        g.addUndirectedEdge(members[i], members[j]);
      }
    }
  }
  g.finalize();
  return g;
}
