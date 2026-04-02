#!/bin/sh
set -eu

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT

a="$tmpdir/a"
b="$tmpdir/b"
mkdir -p "$a/src" "$b/src"

printf 'same\n' > "$a/src/keep.txt"
printf 'same\n' > "$b/src/keep.txt"
printf 'one\ntwo\nthree\n' > "$a/src/change.txt"
printf 'one\nTWO\nthree\n' > "$b/src/change.txt"
printf 'alpha\nbeta\n' > "$a/src/size-change.txt"
printf 'alpha\nbeta\ngamma\n' > "$b/src/size-change.txt"
printf 'removed\n' > "$a/old.txt"
printf 'added\n' > "$b/new.txt"

output="$(./build/bin/verdiff --lines -o "$tmpdir/report.txt" "$a" "$b")"

printf '%s\n' "$output" | grep -F "VERDIFF COMPARISON REPORT" >/dev/null
printf '%s\n' "$output" | grep -F "Unchanged Files       : 1" >/dev/null
printf '%s\n' "$output" | grep -F "Modified Files        : 2" >/dev/null
printf '%s\n' "$output" | grep -F "Added Files           : 1" >/dev/null
printf '%s\n' "$output" | grep -F "Removed Files         : 1" >/dev/null
printf '%s\n' "$output" | grep -F "Detailed Report File  : $tmpdir/report.txt" >/dev/null
grep -F "| src/change.txt" "$tmpdir/report.txt" >/dev/null
grep -F "Lines: 2" "$tmpdir/report.txt" >/dev/null
grep -F "Size Changed (line scan skipped)" "$tmpdir/report.txt" >/dev/null
grep -F "File: src/change.txt" "$tmpdir/report.txt" >/dev/null
