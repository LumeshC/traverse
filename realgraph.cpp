#include "realgraph.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

enum class ArcRule { kAny, kReciprocal, kOneWay };

int compact_of(const std::vector<int>& originalIds, int original) {
  const auto it = std::lower_bound(originalIds.begin(), originalIds.end(), original);
  if (it == originalIds.end() || *it != original) {
    throw std::logic_error("arc endpoint missing from the id table");
  }
  return static_cast<int>(it - originalIds.begin());
}

bool parse_labeled_int(const std::string& line, const char* label, int& out) {
  const auto pos = line.find(label);
  if (pos == std::string::npos) {
    return false;
  }
  std::size_t i = pos + std::char_traits<char>::length(label);
  while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i]))) {
    ++i;
  }
  if (i >= line.size() || !std::isdigit(static_cast<unsigned char>(line[i]))) {
    return false;
  }
  long value = 0;
  for (; i < line.size() && std::isdigit(static_cast<unsigned char>(line[i])); ++i) {
    value = value * 10 + (line[i] - '0');
    if (value > 2000000000L) {
      return false;
    }
  }
  out = static_cast<int>(value);
  return true;
}

bool parse_arc(const std::string& line, int& u, int& v) {
  const char* cursor = line.c_str();
  char* end = nullptr;
  const long first = std::strtol(cursor, &end, 10);
  if (end == cursor || first < 0 || first > 2000000000L) {
    return false;
  }
  cursor = end;
  while (*cursor == ' ' || *cursor == '\t') {
    ++cursor;
  }
  const long second = std::strtol(cursor, &end, 10);
  if (end == cursor || second < 0 || second > 2000000000L) {
    return false;
  }
  u = static_cast<int>(first);
  v = static_cast<int>(second);
  return true;
}

RelationshipData compress(std::vector<std::pair<int, int>> raw, long long dataLines,
                          long long selfLoops, int declaredNodes, int declaredEdges) {
  std::sort(raw.begin(), raw.end());
  const auto withDuplicates = raw.size();
  raw.erase(std::unique(raw.begin(), raw.end()), raw.end());

  std::vector<int> ids;
  ids.reserve(raw.size() * 2);
  for (const auto& arc : raw) {
    ids.push_back(arc.first);
    ids.push_back(arc.second);
  }
  std::sort(ids.begin(), ids.end());
  ids.erase(std::unique(ids.begin(), ids.end()), ids.end());

  RelationshipData data;
  data.originalIds = std::move(ids);
  data.dataLines = dataLines;
  data.selfLoopsDropped = selfLoops;
  data.duplicateArcsDropped = static_cast<long long>(withDuplicates - raw.size());
  data.declaredNodes = declaredNodes;
  data.declaredEdges = declaredEdges;
  data.arcs.reserve(raw.size());
  for (const auto& arc : raw) {
    data.arcs.emplace_back(compact_of(data.originalIds, arc.first),
                           compact_of(data.originalIds, arc.second));
  }
  std::sort(data.arcs.begin(), data.arcs.end());
  return data;
}

RelationshipData ingest(std::istream& in) {
  std::vector<std::pair<int, int>> raw;
  std::string line;
  long long dataLines = 0;
  long long selfLoops = 0;
  long long lineNumber = 0;
  int declaredNodes = -1;
  int declaredEdges = -1;

  while (std::getline(in, line)) {
    ++lineNumber;
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF &&
        static_cast<unsigned char>(line[1]) == 0xBB &&
        static_cast<unsigned char>(line[2]) == 0xBF) {
      line.erase(0, 3);
    }
    if (line.empty()) {
      continue;
    }
    if (line[0] == '#') {
      int nodes = 0;
      int edges = 0;
      if (declaredNodes < 0 && parse_labeled_int(line, "Nodes:", nodes) &&
          parse_labeled_int(line, "Edges:", edges)) {
        declaredNodes = nodes;
        declaredEdges = edges;
      }
      continue;
    }
    int u = 0;
    int v = 0;
    if (!parse_arc(line, u, v)) {
      throw std::runtime_error("bad edge on line " + std::to_string(lineNumber));
    }
    ++dataLines;
    if (dataLines % 1000000 == 0) {
      std::cerr << "read " << dataLines << " arcs\n";
    }
    if (u == v) {
      ++selfLoops;
      continue;
    }
    raw.emplace_back(u, v);
  }
  if (!in.eof() && in.fail()) {
    throw std::runtime_error("failed while reading the edge list");
  }
  return compress(std::move(raw), dataLines, selfLoops, declaredNodes, declaredEdges);
}

bool has_arc(const RelationshipData& data, int u, int v) {
  return std::binary_search(data.arcs.begin(), data.arcs.end(), std::make_pair(u, v));
}

std::vector<std::pair<int, int>> undirected_edges(const RelationshipData& data, ArcRule rule) {
  std::vector<std::pair<int, int>> edges;
  edges.reserve(data.arcs.size());
  auto push = [&](int u, int v) {
    if (u > v) {
      std::swap(u, v);
    }
    edges.emplace_back(u, v);
  };

  for (const auto& arc : data.arcs) {
    const int u = arc.first;
    const int v = arc.second;
    const bool rev = has_arc(data, v, u);
    if (u > v) {
      // The opposite arc, when it exists, owns this unordered pair.
      if (rev || rule == ArcRule::kReciprocal) {
        continue;
      }
      push(u, v);
      continue;
    }
    if (rule == ArcRule::kAny || (rule == ArcRule::kReciprocal && rev) ||
        (rule == ArcRule::kOneWay && !rev)) {
      push(u, v);
    }
  }
  std::sort(edges.begin(), edges.end());
  edges.erase(std::unique(edges.begin(), edges.end()), edges.end());
  return edges;
}

Graph graph_from_edges(const RelationshipData& data, ArcRule rule) {
  const auto edges = undirected_edges(data, rule);
  Graph graph = Graph::contiguous(static_cast<int>(data.originalIds.size()));
  for (const auto& edge : edges) {
    graph.addUndirectedEdge(edge.first, edge.second);
  }
  graph.finalize();
  return graph;
}

bool adjacent(const Graph& graph, int u, int v) {
  const auto& nbrs = graph.neighbors(u);
  return std::binary_search(nbrs.begin(), nbrs.end(), v);
}

std::string check_triangle_cycle(const RelationshipData& data, const Graph& graph) {
  const auto cycle = one_cycle(graph);
  if (cycle.size() < 4 || cycle.front() != cycle.back()) {
    return "expected the 4-5-6 cycle";
  }
  bool seen[10] = {};
  int distinct = 0;
  for (std::size_t i = 0; i + 1 < cycle.size(); ++i) {
    const int original = data.originalIds[static_cast<std::size_t>(cycle[i])];
    if (original < 4 || original > 6) {
      return "cycle left the 4-5-6 triangle";
    }
    if (!adjacent(graph, cycle[i], cycle[i + 1])) {
      return "cycle uses a missing edge";
    }
    if (!seen[original]) {
      seen[original] = true;
      ++distinct;
    }
  }
  if (distinct != 3) {
    return "cycle is not the whole triangle";
  }
  return "";
}

}  // namespace

RelationshipData load_relationship_file(const std::string& path) {
  std::ifstream in(path);
  if (!in) {
    throw std::runtime_error("cannot open " + path);
  }
  return ingest(in);
}

Graph make_any_arc_graph(const RelationshipData& data) {
  // Criterion 1. At least one directed arc. Direction is discarded.
  return graph_from_edges(data, ArcRule::kAny);
}

Graph make_reciprocal_arc_graph(const RelationshipData& data) {
  // Criterion 2. Both directed arcs must exist.
  return graph_from_edges(data, ArcRule::kReciprocal);
}

Graph make_oneway_arc_graph(const RelationshipData& data) {
  // Criterion 3. Exactly one directed arc. Reciprocal pairs are not edges.
  return graph_from_edges(data, ArcRule::kOneWay);
}

std::string realgraph_self_test() {
  const char* text =
      "1 2\n"
      "2 1\n"
      "2 3\n"
      "4 5\n"
      "5 6\n"
      "6 4\n"
      "7 8\n"
      "8 7\n"
      "9 1\n";
  std::istringstream in(text);
  const RelationshipData data = ingest(in);
  if (data.originalIds.size() != 9 || data.arcs.size() != 9) {
    return "self-test did not keep the nine directed arcs";
  }

  const Graph any = make_any_arc_graph(data);
  const Graph reciprocal = make_reciprocal_arc_graph(data);
  const Graph oneway = make_oneway_arc_graph(data);
  if (any.vertexCount() != 9 || reciprocal.vertexCount() != 9 || oneway.vertexCount() != 9) {
    return "a stricter rule dropped vertices";
  }
  const long long anyEdges = count_undirected_edges(any);
  const long long reciprocalEdges = count_undirected_edges(reciprocal);
  const long long onewayEdges = count_undirected_edges(oneway);
  if (anyEdges != 7 || reciprocalEdges != 2 || onewayEdges != 5 ||
      anyEdges != reciprocalEdges + onewayEdges) {
    return "adjacency rules did not partition the undirected pairs";
  }

  auto component_count = [](const Graph& graph) {
    return static_cast<int>(connected_components(graph).size());
  };
  if (component_count(any) != 3 || component_count(reciprocal) != 7 ||
      component_count(oneway) != 5) {
    return "unexpected component counts";
  }
  if (!check_triangle_cycle(data, any).empty() || !check_triangle_cycle(data, oneway).empty()) {
    return "missing triangle under the any-arc or one-way rule";
  }
  if (!one_cycle(reciprocal).empty()) {
    return "reciprocal graph should be a matching, with no cycle";
  }

  const int source = compact_of(data.originalIds, 1);
  const int node3 = compact_of(data.originalIds, 3);
  const int node4 = compact_of(data.originalIds, 4);
  const auto paths = shortest_paths(any, source);
  const auto found = paths.find(node3);
  if (found == paths.end() || found->second.size() != 3 || found->second.back() != source) {
    return "path from 3 back to 1 should be three vertices";
  }
  if (paths.count(node4) != 0) {
    return "user 4 is not reachable from user 1 when direction is ignored";
  }
  return "";
}
