#include "graph_operations.h"

#include <algorithm>
#include <limits>
#include <queue>
#include <stdexcept>
#include <utility>

Graph Graph::contiguous(int n) {
  if (n < 0) {
    throw std::invalid_argument("vertex count must be non-negative");
  }
  Graph g;
  g.adj_.assign(static_cast<std::size_t>(n), {});
  g.present_.assign(static_cast<std::size_t>(n), 1);
  g.vertices_.resize(static_cast<std::size_t>(n));
  for (int i = 0; i < n; ++i) {
    g.vertices_[static_cast<std::size_t>(i)] = i;
  }
  g.finalized_ = true;
  return g;
}

void Graph::ensureVertex(int v) {
  if (v < 0) {
    throw std::invalid_argument("vertex id must be non-negative");
  }
  if (static_cast<std::size_t>(v) >= adj_.size()) {
    adj_.resize(static_cast<std::size_t>(v) + 1);
    present_.resize(static_cast<std::size_t>(v) + 1, 0);
  }
  if (!present_[static_cast<std::size_t>(v)]) {
    present_[static_cast<std::size_t>(v)] = 1;
    vertices_.push_back(v);
    finalized_ = false;
  }
}

void Graph::addUndirectedEdge(int u, int v) {
  if (u == v) {
    return;
  }
  ensureVertex(u);
  ensureVertex(v);
  adj_[static_cast<std::size_t>(u)].push_back(v);
  adj_[static_cast<std::size_t>(v)].push_back(u);
  finalized_ = false;
}

void Graph::finalize() const {
  if (finalized_) {
    return;
  }
  std::sort(vertices_.begin(), vertices_.end());
  vertices_.erase(std::unique(vertices_.begin(), vertices_.end()), vertices_.end());
  for (int v : vertices_) {
    auto& nbrs = adj_[static_cast<std::size_t>(v)];
    std::sort(nbrs.begin(), nbrs.end());
    nbrs.erase(std::unique(nbrs.begin(), nbrs.end()), nbrs.end());
  }
  finalized_ = true;
}

const std::vector<int>& Graph::vertices() const {
  finalize();
  return vertices_;
}

const std::vector<int>& Graph::neighbors(int v) const {
  static const std::vector<int> kEmpty;
  finalize();
  if (v < 0 || static_cast<std::size_t>(v) >= adj_.size()) {
    return kEmpty;
  }
  return adj_[static_cast<std::size_t>(v)];
}

int Graph::vertexCount() const {
  return static_cast<int>(vertices().size());
}

int Graph::indexLimit() const {
  finalize();
  return static_cast<int>(adj_.size());
}

bool Graph::hasVertex(int v) const {
  return v >= 0 && static_cast<std::size_t>(v) < present_.size() &&
         present_[static_cast<std::size_t>(v)];
}

long long count_undirected_edges(const Graph& graph) {
  long long degree_sum = 0;
  for (int v : graph.vertices()) {
    degree_sum += static_cast<long long>(graph.neighbors(v).size());
  }
  return degree_sum / 2;
}

std::size_t graph_storage_bytes(const Graph& graph) {
  graph.finalize();
  std::size_t bytes = graph.vertices().size() * sizeof(int);
  bytes += static_cast<std::size_t>(graph.indexLimit()) * sizeof(std::vector<int>);
  bytes += static_cast<std::size_t>(graph.indexLimit()) * sizeof(char);
  for (int v : graph.vertices()) {
    bytes += graph.neighbors(v).size() * sizeof(int);
  }
  return bytes;
}

std::vector<std::vector<int>> connected_components(const Graph& graph) {
  graph.finalize();
  const int limit = graph.indexLimit();
  std::vector<char> seen(static_cast<std::size_t>(limit), 0);
  std::vector<std::vector<int>> components;
  std::vector<int> stack;
  stack.reserve(graph.vertexCount());

  for (int start : graph.vertices()) {
    if (seen[static_cast<std::size_t>(start)]) {
      continue;
    }
    std::vector<int> component;
    stack.clear();
    stack.push_back(start);
    seen[static_cast<std::size_t>(start)] = 1;
    while (!stack.empty()) {
      const int u = stack.back();
      stack.pop_back();
      component.push_back(u);
      for (int v : graph.neighbors(u)) {
        if (!seen[static_cast<std::size_t>(v)]) {
          seen[static_cast<std::size_t>(v)] = 1;
          stack.push_back(v);
        }
      }
    }
    components.push_back(std::move(component));
  }
  return components;
}

namespace {

std::vector<int> trace_cycle(int from, int back_to, const std::vector<int>& parent) {
  std::vector<int> cycle;
  cycle.push_back(from);
  int current = from;
  const int guard_limit = static_cast<int>(parent.size()) + 1;
  for (int steps = 0; current != back_to; ++steps) {
    current = parent[static_cast<std::size_t>(current)];
    if (current < 0 || steps > guard_limit) {
      return {};
    }
    cycle.push_back(current);
  }
  cycle.push_back(from);
  return cycle;
}

}  // namespace

std::vector<int> one_cycle(const Graph& graph) {
  graph.finalize();
  const int limit = graph.indexLimit();
  std::vector<char> color(static_cast<std::size_t>(limit), 0);
  std::vector<int> parent(static_cast<std::size_t>(limit), -1);

  struct Frame {
    int u;
    int next;
  };
  std::vector<Frame> stack;

  for (int start : graph.vertices()) {
    if (color[static_cast<std::size_t>(start)] != 0) {
      continue;
    }
    color[static_cast<std::size_t>(start)] = 1;
    stack.push_back({start, 0});
    while (!stack.empty()) {
      const int u = stack.back().u;
      const int index = stack.back().next;
      const std::vector<int>& nbrs = graph.neighbors(u);
      if (index >= static_cast<int>(nbrs.size())) {
        color[static_cast<std::size_t>(u)] = 2;
        stack.pop_back();
        continue;
      }
      const int v = nbrs[static_cast<std::size_t>(index)];
      stack.back().next = index + 1;
      if (v == u || v == parent[static_cast<std::size_t>(u)]) {
        continue;
      }
      // A neighbor still on the active DFS stack is an ancestor. The tree
      // path from u up to v, plus this back edge, is a simple cycle.
      if (color[static_cast<std::size_t>(v)] == 1) {
        return trace_cycle(u, v, parent);
      }
      if (color[static_cast<std::size_t>(v)] == 0) {
        parent[static_cast<std::size_t>(v)] = u;
        color[static_cast<std::size_t>(v)] = 1;
        stack.push_back({v, 0});
      }
    }
  }
  return {};
}

std::map<int, std::vector<int>> shortest_paths(const Graph& graph, int source) {
  graph.finalize();
  if (!graph.hasVertex(source)) {
    throw std::invalid_argument("shortest_paths source is not a vertex of the graph");
  }

  const int limit = graph.indexLimit();
  const int inf = std::numeric_limits<int>::max();
  std::vector<int> dist(static_cast<std::size_t>(limit), inf);
  std::vector<int> parent(static_cast<std::size_t>(limit), -1);
  dist[static_cast<std::size_t>(source)] = 0;

  // Lazy binary heap: STL has no decrease-key, so a vertex may sit in the
  // heap more than once. The first time it is popped, its distance is final
  // because every edge weight is positive.
  using Node = std::pair<int, int>;
  std::priority_queue<Node, std::vector<Node>, std::greater<Node>> heap;
  heap.push(Node{0, source});

  while (!heap.empty()) {
    const int d = heap.top().first;
    const int u = heap.top().second;
    heap.pop();
    if (d != dist[static_cast<std::size_t>(u)]) {
      continue;
    }
    for (int v : graph.neighbors(u)) {
      const int next = d + 1;
      if (next < dist[static_cast<std::size_t>(v)]) {
        dist[static_cast<std::size_t>(v)] = next;
        parent[static_cast<std::size_t>(v)] = u;
        heap.push(Node{next, v});
      }
    }
  }

  std::map<int, std::vector<int>> paths;
  for (int v : graph.vertices()) {
    if (dist[static_cast<std::size_t>(v)] == inf) {
      continue;
    }
    std::vector<int> path;
    for (int current = v; current != -1; current = parent[static_cast<std::size_t>(current)]) {
      path.push_back(current);
    }
    paths.emplace(v, std::move(path));
  }
  return paths;
}
