#!/bin/sh
set -eu

repo_root="/home/itsdcsec/chandru_system_backup/practice/c_learning"
tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT

a="$tmpdir/source"
b="$tmpdir/target"

mkdir -p "$a" "$b"

tar -C "$repo_root" \
    --exclude='.git' \
    --exclude='verdiff/build' \
    --exclude='carbon/target' \
    --exclude='lexer/target' \
    -cf - . | tar -C "$a" -xf -

cp -a "$a/." "$b/"

printf '\n/* benchmark mutation */\n' >> "$b/Array/task.c"
printf '\n/* benchmark mutation */\n' >> "$b/Array/task.h"
mkdir -p "$b/bench_added"
printf 'temporary benchmark file\n' > "$b/bench_added/new_file.txt"
rm -f "$b/word_count/word_count.c"

files_a="$(find "$a" -type f | wc -l)"
files_b="$(find "$b" -type f | wc -l)"

echo "Real-tree benchmark dataset:"
echo "  Source files : $files_a"
echo "  Target files : $files_b"
echo "  Expected changes:"
echo "    Modified : 2"
echo "    Added    : 1"
echo "    Removed  : 1"
echo

echo "Running verdiff..."
/usr/bin/time -f 'verdiff elapsed=%e rss_kb=%M' \
    ./build/bin/verdiff --lines --skip-unchanged "$a" "$b" > "$tmpdir/verdiff.txt" 2> "$tmpdir/verdiff.time"

echo "Running git diff --no-index..."
set +e
/usr/bin/time -f 'gitdiff elapsed=%e rss_kb=%M' \
    git diff --no-index --no-color "$a" "$b" > "$tmpdir/gitdiff.txt" 2> "$tmpdir/gitdiff.time"
git_rc=$?
set -e
if [ "$git_rc" -ne 0 ] && [ "$git_rc" -ne 1 ]; then
    cat "$tmpdir/gitdiff.time"
    exit "$git_rc"
fi

echo "Running diff -rq..."
set +e
/usr/bin/time -f 'diff-rq elapsed=%e rss_kb=%M' \
    diff -rq "$a" "$b" > "$tmpdir/diff_rq.txt" 2> "$tmpdir/diff_rq.time"
diff_rc=$?
set -e
if [ "$diff_rc" -ne 0 ] && [ "$diff_rc" -ne 1 ]; then
    cat "$tmpdir/diff_rq.time"
    exit "$diff_rc"
fi

grep -F "Modified Files        : 2" "$tmpdir/verdiff.txt" >/dev/null
grep -F "Added Files           : 1" "$tmpdir/verdiff.txt" >/dev/null
grep -F "Removed Files         : 1" "$tmpdir/verdiff.txt" >/dev/null
grep -F "Array/task.c" "$tmpdir/verdiff.txt" >/dev/null
grep -F "Size Changed (line scan skipped)" "$tmpdir/verdiff.txt" >/dev/null
grep -F "Only in $b: bench_added" "$tmpdir/diff_rq.txt" >/dev/null
grep -F "Only in $a/word_count: word_count.c" "$tmpdir/diff_rq.txt" >/dev/null
grep -F "Array/task.c" "$tmpdir/gitdiff.txt" >/dev/null
grep -F "bench_added/new_file.txt" "$tmpdir/gitdiff.txt" >/dev/null
grep -F "word_count/word_count.c" "$tmpdir/gitdiff.txt" >/dev/null

echo
echo "Correctness checks passed."
echo
echo "Timing summary:"
cat "$tmpdir/verdiff.time"
cat "$tmpdir/gitdiff.time"
cat "$tmpdir/diff_rq.time"
echo
echo "Verdiff report summary:"
sed -n '1,28p' "$tmpdir/verdiff.txt"
