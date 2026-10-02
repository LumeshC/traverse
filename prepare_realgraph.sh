#!/bin/sh
# Download the SNAP Wikipedia talk network. It is a directed graph of Wikipedia
# users: an arc means one user edited the other's talk page. SNAP reports
# 2,394,385 nodes and 5,021,410 arcs.
# https://snap.stanford.edu/data/wiki-Talk.html
set -eu
cd "$(dirname "$0")"
mkdir -p data/wiki
txt=data/wiki/wiki-Talk.txt
gz=data/wiki/wiki-Talk.txt.gz
url=https://snap.stanford.edu/data/wiki-Talk.txt.gz
if [ ! -s "$txt" ]; then
  curl -L --fail --retry 3 -o "$gz" "$url"
  gzip -dc "$gz" > "$txt.partial"
  mv "$txt.partial" "$txt"
fi
nodes=$(awk '/^# Nodes:/ { print $3; exit }' "$txt")
if [ -z "$nodes" ] || [ "$nodes" -lt 1000000 ]; then
  echo "expected a header with at least 1000000 nodes, got '${nodes}'" >&2
  exit 1
fi
echo "wiki-Talk is ready: $txt (header nodes=$nodes)"
