#!/bin/sh
# Fresh process per row so peak RSS is not carried over from a larger run.
set -eu
cd "$(dirname "$0")"
make simulated_test
OUT=benchmark.csv
echo "kind,n,extra,vertices,edges,graph_bytes,build_cpu,cc_cpu,cc_count,cycle_cpu,cycle_len,sp_cpu,sp_reached,sp_max_hops,path_bytes,rss_after_build,peak_rss" > "$OUT"

record() {
  ./simulated_test bench "$@" | tee /dev/stderr | awk '
    /^RESULT / {
      line = $0
      sub(/^RESULT /, "", line)
      n = split(line, a, " ")
      row = ""
      for (i = 1; i <= n; i++) {
        split(a[i], kv, "=")
        row = (row == "" ? kv[2] : row "," kv[2])
      }
      print row
    }
  ' >> "$OUT"
}

for n in 1000 10000 100000 1000000; do
  record cycle "$n" cy
done
for n in 1000 5000 10000 20000; do
  record cycle "$n" s
done

for n in 50 100 200 400 800; do
  record complete "$n" cys
done

for n in 1000 10000 100000 1000000; do
  record empty "$n" cys
done

for n in 1000 10000 100000 1000000; do
  record heap "$n" cys
done

record truncated 1000 10 cys
record truncated 10000 100 cys
record truncated 100000 1000 cys
record truncated 1000000 10000 cys

record modk 600 3 cys
record modk 2000 5 cys
record modk 8000 20 cys
record modk 20000 50 cys

echo "wrote $OUT"
