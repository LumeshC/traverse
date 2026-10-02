#ifndef GRAPH_SIMULATOR_H
#define GRAPH_SIMULATOR_H

#include "graph_operations.h"

// Generators for the six synthetic families. Vertices keep each family's
// natural integer labels. Every graph is undirected and simple.

Graph make_cycle(int n);
Graph make_complete(int n);
Graph make_empty(int n);
Graph make_heap(int n);
Graph make_truncated_heap(int m, int n);
Graph make_equivalence_mod_k(int n, int k);

#endif
