#include "graph_operations.h"
#include "graph_simulator.h"
#include "instrumentation.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

void fail(const std::string& message) {
  std::cerr << "CHECK FAILED: " << message << "\n";
  std::exit(1);
}

bool adjacent(const Graph& graph, int u, int v) {
  const auto& nbrs = graph.neighbors(u);
  return std::binary_search(nbrs.begin(), nbrs.end(), v);
}

void expect_valid_cycle(const Graph& graph, const std::vector<int>& cycle) {
  if (cycle.size() < 4) {
    fail("cycle has fewer than three distinct vertices");
  }
  if (cycle.front() != cycle.back()) {
    fail("cycle does not start and end on the same vertex");
  }
  std::set<int> seen;
  for (std::size_t i = 0; i + 1 < cycle.size(); ++i) {
    if (!seen.insert(cycle[i]).second) {
      fail("cycle repeats a vertex before it closes");
    }
    if (!adjacent(graph, cycle[i], cycle[i + 1])) {
      fail("cycle uses a missing edge");
    }
  }
}

std::vector<int> bfs_distance(const Graph& graph, int source) {
  std::vector<int> dist(static_cast<std::size_t>(graph.indexLimit()), -1);
  std::queue<int> q;
  dist[static_cast<std::size_t>(source)] = 0;
  q.push(source);
  while (!q.empty()) {
    const int u = q.front();
    q.pop();
    for (int v : graph.neighbors(u)) {
      if (dist[static_cast<std::size_t>(v)] < 0) {
        dist[static_cast<std::size_t>(v)] = dist[static_cast<std::size_t>(u)] + 1;
        q.push(v);
      }
    }
  }
  return dist;
}

void expect_paths_match_bfs(const Graph& graph, int source) {
  const auto paths = shortest_paths(graph, source);
  const auto dist = bfs_distance(graph, source);
  if (paths.count(source) != 1 || paths.at(source) != std::vector<int>{source}) {
    fail("path from the source to itself is not [source]");
  }
  for (int v : graph.vertices()) {
    const int d = dist[static_cast<std::size_t>(v)];
    if (d < 0) {
      if (paths.count(v) != 0) {
        fail("unreachable vertex appeared in the shortest-path map");
      }
      continue;
    }
    const auto it = paths.find(v);
    if (it == paths.end()) {
      fail("reachable vertex missing from the shortest-path map");
    }
    const auto& path = it->second;
    if (static_cast<int>(path.size()) != d + 1) {
      fail("shortest path length does not match BFS");
    }
    if (path.front() != v || path.back() != source) {
      fail("shortest path does not run from v back to the source");
    }
    for (std::size_t i = 0; i + 1 < path.size(); ++i) {
      if (!adjacent(graph, path[i], path[i + 1])) {
        fail("shortest path uses a missing edge");
      }
    }
  }
}

void expect_partition(const Graph& graph, const std::vector<std::vector<int>>& components,
                      int expected_count) {
  if (static_cast<int>(components.size()) != expected_count) {
    fail("unexpected number of connected components");
  }
  std::set<int> seen;
  for (const auto& component : components) {
    if (component.empty()) {
      fail("empty component");
    }
    for (int v : component) {
      if (!seen.insert(v).second) {
        fail("vertex appears in two components");
      }
    }
  }
  if (static_cast<int>(seen.size()) != graph.vertexCount()) {
    fail("components do not cover every vertex");
  }
}

void check_cycle_graph() {
  const int n = 30;
  const Graph g = make_cycle(n);
  if (count_undirected_edges(g) != n) {
    fail("n-cycle edge count");
  }
  const auto components = connected_components(g);
  expect_partition(g, components, 1);
  const auto cycle = one_cycle(g);
  expect_valid_cycle(g, cycle);
  if (static_cast<int>(cycle.size()) != n + 1) {
    fail("n-cycle did not return the unique cycle of length n");
  }
  expect_paths_match_bfs(g, 0);
  const auto paths = shortest_paths(g, 0);
  int max_hops = 0;
  for (const auto& entry : paths) {
    max_hops = std::max(max_hops, static_cast<int>(entry.second.size()) - 1);
  }
  if (max_hops > n / 2) {
    fail("n-cycle shortest path longer than n/2");
  }
}

void check_complete_graph() {
  const int n = 25;
  const Graph g = make_complete(n);
  if (count_undirected_edges(g) != static_cast<long long>(n) * (n - 1) / 2) {
    fail("complete graph edge count");
  }
  expect_partition(g, connected_components(g), 1);
  expect_valid_cycle(g, one_cycle(g));
  expect_paths_match_bfs(g, 0);
  const auto paths = shortest_paths(g, 3);
  for (int v = 0; v < n; ++v) {
    const auto& path = paths.at(v);
    if (v == 3) {
      if (path.size() != 1) {
        fail("complete graph source path");
      }
    } else if (path.size() != 2 || path[0] != v || path[1] != 3) {
      fail("complete graph paths are not unit length");
    }
  }
}

void check_empty_graph() {
  const int n = 18;
  const Graph g = make_empty(n);
  if (count_undirected_edges(g) != 0) {
    fail("empty graph has an edge");
  }
  expect_partition(g, connected_components(g), n);
  if (!one_cycle(g).empty()) {
    fail("empty graph reported a cycle");
  }
  const auto paths = shortest_paths(g, 4);
  if (paths.size() != 1 || paths.at(4) != std::vector<int>{4}) {
    fail("empty graph shortest paths");
  }
  expect_paths_match_bfs(g, 4);
}

void check_heap_graph() {
  const int n = 64;
  const Graph g = make_heap(n);
  if (count_undirected_edges(g) != n - 1) {
    fail("heap edge count");
  }
  expect_partition(g, connected_components(g), 1);
  if (!one_cycle(g).empty()) {
    fail("heap reported a cycle");
  }
  expect_paths_match_bfs(g, 0);
  const auto paths = shortest_paths(g, 0);
  for (int v = 0; v < n; ++v) {
    std::vector<int> expected;
    for (int current = v; current != 0; current = (current - 1) / 2) {
      expected.push_back(current);
    }
    expected.push_back(0);
    if (paths.at(v) != expected) {
      fail("heap path does not follow parents to the root");
    }
  }
}

void check_truncated_heap() {
  const int m = 5;
  const int n = 80;
  const Graph g = make_truncated_heap(m, n);
  const int vertices = n - m;
  const int components_expected = m + 1;  // holds when n >= 2m + 1
  const long long edges_expected = static_cast<long long>(n) - 1 - 2LL * m;
  if (g.vertexCount() != vertices) {
    fail("truncated heap vertex set");
  }
  if (!g.hasVertex(m) || g.hasVertex(m - 1)) {
    fail("truncated heap labels");
  }
  if (count_undirected_edges(g) != edges_expected) {
    fail("truncated heap edge count");
  }
  expect_partition(g, connected_components(g), components_expected);
  if (!one_cycle(g).empty()) {
    fail("truncated heap reported a cycle");
  }
  expect_paths_match_bfs(g, m);
  // A forest on V vertices with C components has V - C edges.
  if (edges_expected != static_cast<long long>(vertices) - components_expected) {
    fail("truncated heap formula is inconsistent");
  }
}

void check_mod_k() {
  const int n = 40;
  const int k = 6;
  const Graph g = make_equivalence_mod_k(n, k);
  const auto components = connected_components(g);
  expect_partition(g, components, k);
  for (const auto& component : components) {
    const int residue = component.front() % k;
    for (std::size_t i = 0; i < component.size(); ++i) {
      if (component[i] % k != residue) {
        fail("mod-k component mixed residues");
      }
      for (std::size_t j = i + 1; j < component.size(); ++j) {
        if (!adjacent(g, component[i], component[j])) {
          fail("mod-k component is not complete");
        }
      }
    }
  }
  expect_valid_cycle(g, one_cycle(g));
  expect_paths_match_bfs(g, 0);
  const auto paths = shortest_paths(g, 0);
  if (paths.count(1) != 0) {
    fail("mod-k path crossed components");
  }
  if (k < n && (paths.count(k) != 1 || paths.at(k).size() != 2)) {
    fail("mod-k neighbors are not at distance 1");
  }
}

void run_checks() {
  check_cycle_graph();
  check_complete_graph();
  check_empty_graph();
  check_heap_graph();
  check_truncated_heap();
  check_mod_k();
  std::cout << "All correctness checks passed.\n";
}

std::string format_int_list(const std::vector<int>& values) {
  std::ostringstream out;
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i != 0) {
      out << ' ';
    }
    out << values[i];
  }
  return out.str();
}

void print_report(const std::string& title, const Graph& graph, int source) {
  std::cout << "=== " << title << " ===\n";
  std::cout << "vertices=" << graph.vertexCount()
            << " edges=" << count_undirected_edges(graph) << "\n";
  const auto components = connected_components(graph);
  std::cout << "components=" << components.size() << "\n";
  for (std::size_t i = 0; i < components.size(); ++i) {
    std::vector<int> members = components[i];
    std::sort(members.begin(), members.end());
    std::cout << "  component " << i << " (" << members.size() << "): "
              << format_int_list(members) << "\n";
  }
  const auto cycle = one_cycle(graph);
  if (cycle.empty()) {
    std::cout << "cycle: none\n";
  } else {
    std::cout << "cycle (" << (cycle.size() - 1) << "): " << format_int_list(cycle) << "\n";
  }
  const auto paths = shortest_paths(graph, source);
  std::cout << "shortest paths from " << source << " (" << paths.size() << " reachable)\n";
  for (const auto& entry : paths) {
    std::cout << "  " << entry.first << ": " << format_int_list(entry.second) << "\n";
  }
  std::cout << "\n";
}

void run_samples() {
  print_report("6-cycle", make_cycle(6), 0);
  print_report("complete K5", make_complete(5), 0);
  print_report("empty n=4", make_empty(4), 0);
  print_report("heap n=7", make_heap(7), 0);
  print_report("truncated heap m=1 n=7", make_truncated_heap(1, 7), 1);
  print_report("equivalence mod 3 on n=9", make_equivalence_mod_k(9, 3), 0);
}

bool wants(const std::string& algos, char bit) {
  return algos.find(bit) != std::string::npos;
}

struct Timed {
  double cpu_seconds = 0;
  long rss_after_first = 0;
};

// Average CPU over up to five runs, stopping once 0.2s has accumulated.
// Resident set is sampled immediately after the first run, before a repeat
// can hold two results at once and raise the high-water mark.
template <typename Fn>
Timed time_first(Fn&& fn) {
  const double start = current_usage().cpu_seconds;
  fn();
  const double after_first = current_usage().cpu_seconds;
  const long rss = current_usage().peak_rss_bytes;
  int reps = 1;
  double elapsed = after_first - start;
  while (reps < 5 && elapsed < 0.20) {
    const double mark = current_usage().cpu_seconds;
    fn();
    elapsed += current_usage().cpu_seconds - mark;
    ++reps;
  }
  return Timed{elapsed / static_cast<double>(reps), rss};
}

void run_bench(const std::string& kind, int n, int extra, const std::string& algos) {
  Graph graph;
  const double build_start = current_usage().cpu_seconds;
  if (kind == "cycle") {
    graph = make_cycle(n);
  } else if (kind == "complete") {
    graph = make_complete(n);
  } else if (kind == "empty") {
    graph = make_empty(n);
  } else if (kind == "heap") {
    graph = make_heap(n);
  } else if (kind == "truncated") {
    graph = make_truncated_heap(extra, n);
  } else if (kind == "modk") {
    graph = make_equivalence_mod_k(n, extra);
  } else {
    fail("unknown graph kind " + kind);
  }
  const double build_cpu = current_usage().cpu_seconds - build_start;
  const long rss_after_build = current_usage().peak_rss_bytes;
  const long long edges = count_undirected_edges(graph);
  const std::size_t stored = graph_storage_bytes(graph);
  const int source = graph.vertices().empty() ? 0 : graph.vertices().front();

  double cc_cpu = -1;
  int cc_count = -1;
  long rss_peak = rss_after_build;
  std::vector<std::vector<int>> components;
  if (wants(algos, 'c')) {
    const Timed timed = time_first([&] { components = connected_components(graph); });
    cc_cpu = timed.cpu_seconds;
    rss_peak = std::max(rss_peak, timed.rss_after_first);
    cc_count = static_cast<int>(components.size());
  }

  double cycle_cpu = -1;
  int cycle_len = -1;
  std::vector<int> cycle;
  if (wants(algos, 'y')) {
    const Timed timed = time_first([&] { cycle = one_cycle(graph); });
    cycle_cpu = timed.cpu_seconds;
    rss_peak = std::max(rss_peak, timed.rss_after_first);
    cycle_len = static_cast<int>(cycle.size());
  }

  double sp_cpu = -1;
  int sp_reached = -1;
  int sp_max_hops = -1;
  std::size_t path_bytes = 0;
  std::map<int, std::vector<int>> paths;
  if (wants(algos, 's')) {
    const Timed timed = time_first([&] { paths = shortest_paths(graph, source); });
    sp_cpu = timed.cpu_seconds;
    rss_peak = std::max(rss_peak, timed.rss_after_first);
    sp_reached = static_cast<int>(paths.size());
    sp_max_hops = 0;
    for (const auto& entry : paths) {
      sp_max_hops = std::max(sp_max_hops, static_cast<int>(entry.second.size()) - 1);
      path_bytes += entry.second.size() * sizeof(int);
    }
  }

  if (kind == "cycle" && n >= 3) {
    if (cc_count >= 0 && cc_count != 1) {
      fail("bench cycle components");
    }
    if (cycle_len >= 0 && cycle_len != n + 1) {
      fail("bench cycle length");
    }
    if (sp_reached >= 0 && (sp_reached != n || sp_max_hops > n / 2)) {
      fail("bench cycle distances");
    }
  } else if (kind == "complete") {
    if (cc_count >= 0 && cc_count != 1) {
      fail("bench complete components");
    }
    if (cycle_len >= 0 && n >= 3 && cycle_len < 4) {
      fail("bench complete cycle");
    }
    if (sp_reached >= 0 && (sp_reached != n || sp_max_hops > 1)) {
      fail("bench complete distances");
    }
  } else if (kind == "empty") {
    if (cc_count >= 0 && cc_count != n) {
      fail("bench empty components");
    }
    if (cycle_len >= 0 && cycle_len != 0) {
      fail("bench empty cycle");
    }
    if (sp_reached >= 0 && (sp_reached != 1 || sp_max_hops != 0)) {
      fail("bench empty distances");
    }
  } else if (kind == "heap") {
    if (edges != static_cast<long long>(n) - 1) {
      fail("bench heap edges");
    }
    if (cc_count >= 0 && cc_count != 1) {
      fail("bench heap components");
    }
    if (cycle_len >= 0 && cycle_len != 0) {
      fail("bench heap cycle");
    }
    if (sp_reached >= 0 && sp_reached != n) {
      fail("bench heap reachability");
    }
  } else if (kind == "truncated") {
    const int m = extra;
    if (n < 2 * m + 1) {
      fail("bench truncated heap is outside the formula's range");
    }
    if (edges != static_cast<long long>(n) - 1 - 2LL * m) {
      fail("bench truncated edges");
    }
    if (cc_count >= 0 && cc_count != m + 1) {
      fail("bench truncated components");
    }
    if (cycle_len >= 0 && cycle_len != 0) {
      fail("bench truncated cycle");
    }
  } else if (kind == "modk") {
    if (cc_count >= 0 && cc_count != extra) {
      fail("bench mod-k components");
    }
    if (cycle_len >= 0 && n / extra >= 3 && cycle_len < 4) {
      fail("bench mod-k cycle");
    }
  }

  if (cc_count >= 0 && sp_reached >= 0) {
    int source_component = -1;
    for (const auto& component : components) {
      if (std::find(component.begin(), component.end(), source) != component.end()) {
        source_component = static_cast<int>(component.size());
      }
    }
    if (source_component != sp_reached) {
      fail("shortest-path map does not match the source component");
    }
  }

  std::cout << std::fixed;
  std::cout.precision(6);
  std::cout << "RESULT"
            << " kind=" << kind
            << " n=" << n
            << " extra=" << extra
            << " vertices=" << graph.vertexCount()
            << " edges=" << edges
            << " graph_bytes=" << stored
            << " build_cpu=" << build_cpu
            << " cc_cpu=" << cc_cpu
            << " cc_count=" << cc_count
            << " cycle_cpu=" << cycle_cpu
            << " cycle_len=" << cycle_len
            << " sp_cpu=" << sp_cpu
            << " sp_reached=" << sp_reached
            << " sp_max_hops=" << sp_max_hops
            << " path_bytes=" << path_bytes
            << " rss_after_build=" << rss_after_build
            << " peak_rss=" << rss_peak
            << "\n";
}

void print_usage() {
  std::cerr << "Usage:\n"
            << "  simulated_test check\n"
            << "  simulated_test samples\n"
            << "  simulated_test bench <cycle|complete|empty|heap> <n> [cys]\n"
            << "  simulated_test bench truncated <n> <m> [cys]\n"
            << "  simulated_test bench modk <n> <k> [cys]\n"
            << "c = components, y = cycle, s = shortest paths. Default is cys.\n";
}

}  // namespace

int main(int argc, char** argv) {
  std::ios::sync_with_stdio(false);
  const std::string mode = argc >= 2 ? argv[1] : "demo";
  try {
    if (mode == "check") {
      run_checks();
      return 0;
    }
    if (mode == "samples") {
      run_samples();
      return 0;
    }
    if (mode == "demo") {
      run_checks();
      run_samples();
      return 0;
    }
    if (mode == "bench") {
      if (argc < 4) {
        print_usage();
        return 1;
      }
      const std::string kind = argv[2];
      const int n = std::stoi(argv[3]);
      if (kind == "truncated" || kind == "modk") {
        if (argc < 5) {
          print_usage();
          return 1;
        }
        const int extra = std::stoi(argv[4]);
        const std::string algos = argc >= 6 ? argv[5] : "cys";
        run_bench(kind, n, extra, algos);
      } else {
        const std::string algos = argc >= 5 ? argv[4] : "cys";
        run_bench(kind, n, 0, algos);
      }
      return 0;
    }
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << "\n";
    return 1;
  }
  print_usage();
  return 1;
}
