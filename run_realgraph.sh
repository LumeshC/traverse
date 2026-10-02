#!/bin/sh
# One process per adjacency rule, so peak RSS belongs to that rule.
set -eu
cd "$(dirname "$0")"
make run_realgraph
sh prepare_realgraph.sh
mkdir -p output
file=data/wiki/wiki-Talk.txt
for criterion in any reciprocal oneway; do
  echo "running $criterion"
  ./run_realgraph "$file" --criterion "$criterion" | tee "output/wiki_${criterion}.txt"
done
echo "wrote output/wiki_any.txt output/wiki_reciprocal.txt output/wiki_oneway.txt"
