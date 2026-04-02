#!/bin/sh
set -eu

count="${1:-5000}"
tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT

a="$tmpdir/source"
b="$tmpdir/target"
mkdir -p "$a/src" "$b/src"

i=1
while [ "$i" -le "$count" ]; do
    printf 'int v = %s;\n' "$i" > "$a/src/file_$i.c"
    cp "$a/src/file_$i.c" "$b/src/file_$i.c"
    i=$((i + 1))
done

printf 'int changed = 1;\nint same = 2;\n' > "$a/src/hot.c"
printf 'int changed = 3;\nint same = 2;\n' > "$b/src/hot.c"
printf '0123456789abcdef\n' > "$a/src/size_only.c"
printf '0123456789abcdefXYZ\n' > "$b/src/size_only.c"
printf 'legacy\n' > "$a/src/removed.c"
printf 'new\n' > "$b/src/added.c"

echo "Benchmark dataset ready:"
echo "  Base identical files : $count"
echo "  Modified files       : 2"
echo "  Added files          : 1"
echo "  Removed files        : 1"
echo

if command -v /usr/bin/time >/dev/null 2>&1; then
    /usr/bin/time -f 'Elapsed Seconds: %e\nMax RSS KB: %M' ./build/bin/verdiff --lines --skip-unchanged -o "$tmpdir/detailed.txt" "$a" "$b" > "$tmpdir/report.txt"
else
    ./build/bin/verdiff --lines --skip-unchanged -o "$tmpdir/detailed.txt" "$a" "$b" > "$tmpdir/report.txt"
fi

grep -F "Modified Files        : 2" "$tmpdir/report.txt" >/dev/null
grep -F "Added Files           : 1" "$tmpdir/report.txt" >/dev/null
grep -F "Removed Files         : 1" "$tmpdir/report.txt" >/dev/null
grep -F "File: src/hot.c" "$tmpdir/detailed.txt" >/dev/null

echo
echo "Correctness checks passed."
echo "Report excerpt:"
sed -n '1,40p' "$tmpdir/report.txt"
