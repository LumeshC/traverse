#ifndef REALGRAPH_H
#define REALGRAPH_H

#include "graph_operations.h"

#include <string>
#include <utility>
#include <vector>

// Directed arcs after preprocessing. originalIds[i] is the id from the file
// for compact vertex i. arcs are unique, sorted, and stored with compact ids.
// Compaction is required: the algorithms index arrays by vertex id, and raw
// ids in a real edge list can be sparse.
struct RelationshipData {
  std::vector<int> originalIds;
  std::vector<std::pair<int, int>> arcs;
  long long dataLines = 0;
  long long selfLoopsDropped = 0;
  long long duplicateArcsDropped = 0;
  int declaredNodes = -1;
  int declaredEdges = -1;
};

// Read a SNAP-style directed edge list. Comment lines (# ...) are skipped.
// Self-loops and duplicate arcs are dropped. Every remaining endpoint becomes
// a vertex, then ids are compressed to 0 .. n-1.
RelationshipData load_relationship_file(const std::string& path);

// Criterion 1. Two vertices are adjacent if the file has at least one directed
// arc between them. Direction is discarded.
Graph make_any_arc_graph(const RelationshipData& data);

// Criterion 2. Two vertices are adjacent only when both directed arcs exist.
Graph make_reciprocal_arc_graph(const RelationshipData& data);

// Criterion 3. Two vertices are adjacent when exactly one directed arc exists.
// A reciprocal pair is not an edge under this rule.
Graph make_oneway_arc_graph(const RelationshipData& data);

// Empty string on success, otherwise a description of the mismatch.
std::string realgraph_self_test();

#endif
