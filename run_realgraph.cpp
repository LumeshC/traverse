#include "instrumentation.h"
#include "realgraph.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

struct Options {
  std::string path;
  std::string criterion = "all";
  bool self_test_only = false;
  bool allow_small = false;
};

void print_usage() {
  std::cerr
      << "Usage:\n"
      << "  run_realgraph --self-test\n"
      << "  run_realgraph <edge-file> [--criterion any|reciprocal|oneway|all]\n"
      << "\n"
      << "The edge file is a directed arc list. Three undirected graphs are built:\n"
      << "  any         at least one directed arc\n"
      << "  reciprocal  both directed arcs (primary: a two-way tie)\n"
      << "  oneway      exactly one directed arc\n"
      << "The Wikipedia graph is expected to have at least 1000000 vertices.\n";
}

Options parse_options(int argc, char** argv) {
  Options options;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--self-test") {
      options.self_test_only = true;
    } else if (arg == "--allow-small") {
      options.allow_small = true;
    } else if (arg == "--criterion" && i + 1 < argc) {
      options.criterion = argv[++i];
    } else if (arg == "--help") {
      print_usage();
      std::exit(0);
    } else if (!arg.empty() && arg[0] == '-') {
      throw std::runtime_error("unknown option " + arg);
    } else if (options.path.empty()) {
      options.path = arg;
    } else {
      throw std::runtime_error("unexpected argument " + arg);
    }
  }
  return options;
}

int original_id(const RelationshipData& data, int compact) {
  return data.originalIds[static_cast<std::size_t>(compact)];
}

std::vector<int> original_ids(const RelationshipData& data, const std::vector<int>& compact) {
  std::vector<int> ids;
  ids.reserve(compact.size());
  for (int vertex : compact) {
    ids.push_back(original_id(data, vertex));
  }
  return ids;
}

void write_sizes(const std::filesystem::path& path, const std::vector<int>& sizes) {
  std::ofstream out(path);
  if (!out) {
    throw std::runtime_error("cannot write " + path.string());
  }
  out << "component_sizes " << sizes.size() << "\n";
  for (int size : sizes) {
    out << size << "\n";
  }
}

void print_ids(const std::vector<int>& ids) {
  if (ids.size() <= 24) {
    for (std::size_t i = 0; i < ids.size(); ++i) {
      if (i != 0) {
        std::cout << ' ';
      }
      std::cout << ids[i];
    }
    return;
  }
  for (int i = 0; i < 12; ++i) {
    if (i != 0) {
      std::cout << ' ';
    }
    std::cout << ids[static_cast<std::size_t>(i)];
  }
  std::cout << " ... " << ids.back();
  std::cout << " (" << ids.size() << " vertices, closed walk)";
}

struct Clock {
  double cpu = 0;
  double wall = 0;
};

Clock now() {
  Clock clock;
  clock.cpu = current_usage().cpu_seconds;
  clock.wall = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
  return clock;
}

double cpu_since(const Clock& start) { return now().cpu - start.cpu; }

struct RunStats {
  long long edges = 0;
  std::size_t storageBytes = 0;
  int components = 0;
  int largest = 0;
  int isolates = 0;
  int nontrivial = 0;
  int cycleEdges = 0;
  int reachable = 0;
  int maxHops = 0;
  int sourceOriginal = -1;
  long long pathBytes = 0;
  double componentsCpu = 0;
  double cycleCpu = 0;
  double pathsCpu = 0;
  std::vector<int> topSizes;
  std::vector<int> cycleIds;
  std::vector<int> farPath;
  std::vector<int> sizes;
};

RunStats evaluate(const RelationshipData& data, const Graph& graph) {
  RunStats stats;
  stats.edges = count_undirected_edges(graph);
  stats.storageBytes = graph_storage_bytes(graph);

  Clock start = now();
  const auto components = connected_components(graph);
  stats.componentsCpu = cpu_since(start);
  stats.components = static_cast<int>(components.size());
  stats.sizes.reserve(components.size());
  int source = -1;
  for (const auto& component : components) {
    const int size = static_cast<int>(component.size());
    stats.sizes.push_back(size);
    if (size == 1) {
      ++stats.isolates;
    } else {
      ++stats.nontrivial;
    }
    if (size > stats.largest) {
      stats.largest = size;
      source = component.front();
    }
  }
  auto ranked = stats.sizes;
  const std::size_t shown = std::min<std::size_t>(8, ranked.size());
  std::partial_sort(ranked.begin(), ranked.begin() + static_cast<std::ptrdiff_t>(shown), ranked.end(),
                    std::greater<int>());
  stats.topSizes.assign(ranked.begin(), ranked.begin() + static_cast<std::ptrdiff_t>(shown));

  start = now();
  const auto cycle = one_cycle(graph);
  stats.cycleCpu = cpu_since(start);
  if (!cycle.empty()) {
    stats.cycleEdges = static_cast<int>(cycle.size()) - 1;
    stats.cycleIds = original_ids(data, cycle);
  }

  if (source >= 0) {
    stats.sourceOriginal = original_id(data, source);
    start = now();
    const auto paths = shortest_paths(graph, source);
    stats.pathsCpu = cpu_since(start);
    stats.reachable = static_cast<int>(paths.size());
    const std::vector<int>* farthest = nullptr;
    for (const auto& entry : paths) {
      stats.pathBytes += static_cast<long long>(entry.second.size()) * static_cast<long long>(sizeof(int));
      const int hops = static_cast<int>(entry.second.size()) - 1;
      if (farthest == nullptr || hops > stats.maxHops) {
        stats.maxHops = hops;
        farthest = &entry.second;
      }
    }
    if (farthest != nullptr) {
      stats.farPath = original_ids(data, *farthest);
    }
  }
  return stats;
}

void print_stats(const std::string& title, const RunStats& stats) {
  std::cout << "\n--- " << title << " ---\n";
  std::cout << "undirected edges: " << stats.edges << "\n";
  std::cout << "graph storage bytes: " << stats.storageBytes << "\n";
  std::cout << "connected components: " << stats.components << "\n";
  std::cout << "largest component: " << stats.largest << "\n";
  std::cout << "nontrivial components: " << stats.nontrivial << "\n";
  std::cout << "isolated vertices: " << stats.isolates << "\n";
  std::cout << "largest component sizes:";
  for (int size : stats.topSizes) {
    std::cout << ' ' << size;
  }
  std::cout << "\n";
  if (stats.cycleEdges == 0) {
    std::cout << "one cycle: none\n";
  } else {
    std::cout << "one cycle length: " << stats.cycleEdges << " original ids: ";
    print_ids(stats.cycleIds);
    std::cout << "\n";
  }
  std::cout << "shortest paths from original id " << stats.sourceOriginal
            << " (a vertex in the largest component): reachable=" << stats.reachable
            << " max_hops=" << stats.maxHops << " path_bytes=" << stats.pathBytes << "\n";
  if (!stats.farPath.empty()) {
    std::cout << "one longest shortest path (original ids, far vertex back to source): ";
    print_ids(stats.farPath);
    std::cout << "\n";
  }
  std::cout.setf(std::ios::fixed);
  std::cout.precision(4);
  std::cout << "cpu seconds: components=" << stats.componentsCpu << " cycle=" << stats.cycleCpu
            << " shortest_paths=" << stats.pathsCpu << "\n";
  std::cout.flush();
}

const char* criterion_title(const std::string& criterion) {
  if (criterion == "any") {
    return "criterion 1: at least one directed arc";
  }
  if (criterion == "reciprocal") {
    return "criterion 2 (primary): both directed arcs";
  }
  if (criterion == "oneway") {
    return "criterion 3: exactly one directed arc";
  }
  return nullptr;
}

Graph build_criterion(const std::string& criterion, const RelationshipData& data) {
  if (criterion == "any") {
    return make_any_arc_graph(data);
  }
  if (criterion == "reciprocal") {
    return make_reciprocal_arc_graph(data);
  }
  if (criterion == "oneway") {
    return make_oneway_arc_graph(data);
  }
  throw std::runtime_error("unknown criterion " + criterion);
}

void run_one(const std::string& criterion, const RelationshipData& data, double loadCpu) {
  const char* title = criterion_title(criterion);
  if (title == nullptr) {
    throw std::runtime_error("unknown criterion " + criterion);
  }
  const Clock start = now();
  Graph graph = build_criterion(criterion, data);
  const double buildCpu = cpu_since(start);
  RunStats stats = evaluate(data, graph);
  std::filesystem::create_directories("output");
  write_sizes(std::filesystem::path("output") / ("wiki_" + criterion + "_components.txt"), stats.sizes);
  std::cout.setf(std::ios::fixed);
  std::cout.precision(4);
  std::cout << "\nload cpu seconds: " << loadCpu << "\n";
  std::cout << "build cpu seconds: " << buildCpu << "\n";
  print_stats(title, stats);
}

}  // namespace

int main(int argc, char** argv) {
  std::ios::sync_with_stdio(false);
  std::cin.tie(nullptr);

  // Primary adjacency: two vertices stay connected only when the relation
  // runs in both directions. One directed arc is a weak contact. The driver
  // still builds and measures all three rules.
  try {
    const Options options = parse_options(argc, argv);
    const std::string selfTestError = realgraph_self_test();
    if (!selfTestError.empty()) {
      std::cerr << "realgraph self-test failed: " << selfTestError << "\n";
      return 1;
    }
    std::cout << "realgraph self-test passed\n";
    if (options.self_test_only) {
      return 0;
    }
    if (options.path.empty()) {
      print_usage();
      return 1;
    }

    const Clock loadStart = now();
    const RelationshipData data = load_relationship_file(options.path);
    const double loadCpu = cpu_since(loadStart);
    const int nodes = static_cast<int>(data.originalIds.size());
    if (!options.allow_small && nodes < 1000000) {
      throw std::runtime_error(
          "Wikipedia graph has " + std::to_string(nodes) +
          " vertices; expected at least 1000000");
    }

    std::cout << "edge file: " << options.path << "\n";
    std::cout << "declared nodes: " << data.declaredNodes << "\n";
    std::cout << "declared edges: " << data.declaredEdges << "\n";
    std::cout << "data lines: " << data.dataLines << "\n";
    std::cout << "self-loops dropped: " << data.selfLoopsDropped << "\n";
    std::cout << "duplicate arcs dropped: " << data.duplicateArcsDropped << "\n";
    std::cout << "unique directed arcs: " << data.arcs.size() << "\n";
    std::cout << "vertices: " << nodes << "\n";
    if (!data.originalIds.empty()) {
      std::cout << "original id range: " << data.originalIds.front() << " .. "
                << data.originalIds.back() << "\n";
    }
    std::cout.flush();

    if (options.criterion == "all") {
      run_one("any", data, loadCpu);
      run_one("reciprocal", data, loadCpu);
      run_one("oneway", data, loadCpu);
    } else {
      run_one(options.criterion, data, loadCpu);
    }

    const Usage usage = current_usage();
    std::cout << "\npeak rss bytes: " << usage.peak_rss_bytes << "\n";
    std::cout.setf(std::ios::fixed);
    std::cout.precision(4);
    std::cout << "process cpu seconds: " << usage.cpu_seconds << "\n";
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << "\n";
    return 1;
  }
  return 0;
}
