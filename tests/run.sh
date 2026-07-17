#!/usr/bin/env bash
#
# skata end-to-end test suite.
#
# Every case in cases/ is a plain C program. There is nothing to declare and
# nothing to keep up to date: clang is the oracle. Each case is built twice --
# once with clang, once with skata -- and the two programs must agree on both
# stdout and exit code.
#
# A case with no main() is a compile-only test: skata just has to emit
# assembly that clang accepts.
#
# usage: tests/run.sh [-v] [pattern]
#   -v        also list passing tests
#   pattern   only run cases whose name contains it
#
# Artifacts for every case are left in tests/build/ (.s, .err, .out) so a
# failure can be inspected directly.
#
# Note: skata prints diagnostics to stdout, not stderr, so they land in the
# .s file. That is why failure text is read from both streams below.

set -uo pipefail

cd "$(dirname "$0")/.."
SKATA=bin/skata
BUILD=tests/build
CASES=tests/cases

VERBOSE=0
[ "${1:-}" = "-v" ] && { VERBOSE=1; shift; }
FILTER="${1:-}"

if [ -t 1 ]; then
    R=$'\e[31m'; G=$'\e[32m'; D=$'\e[2m'; BOLD=$'\e[1m'; N=$'\e[0m'
else
    R=; G=; D=; BOLD=; N=
fi

TIMEOUT=$(command -v timeout || command -v gtimeout || true)
[ -z "$TIMEOUT" ] && echo "note: timeout(1) not found; a miscompiled loop will hang the suite." >&2
to() { if [ -n "$TIMEOUT" ]; then "$TIMEOUT" "$@"; else shift; "$@"; fi; }

strip_ansi() { sed $'s/\033\[[0-9;]*m//g'; }
indent() { sed 's/^/        /'; }

make -s >/dev/null || { echo "${R}build failed${N}"; exit 1; }
mkdir -p "$BUILD"

pass=0; fail=0
failed=()

for src in "$CASES"/*.c; do
    name=$(basename "$src" .c)
    [ -n "$FILTER" ] && [[ "$name" != *"$FILTER"* ]] && continue
    o="$BUILD/$name"

    # ---------- oracle: build and run with clang ----------
    if clang -w "$src" -o "$o.ref" 2>/dev/null; then
        mode=run
        to 10 "./$o.ref" > "$o.ref.stdout" 2>/dev/null; refcode=$?
    else
        mode=compile          # no main(): compile-only case
    fi

    # ---------- subject: build and run with skata ----------
    why=""; detail=""
    if ! to 20 "$SKATA" "$src" > "$o.s" 2> "$o.err"; then
        # diagnostics may be on either stream; ASan uses stderr, skata uses stdout
        if grep -qs 'AddressSanitizer' "$o.err"; then
            why="skata crashed"
            detail=$(grep -hm1 'ERROR: AddressSanitizer' "$o.err" \
                     | sed 's/.*ERROR: //' | indent)
        else
            why="skata rejected the program"
            detail=$(cat "$o.err" "$o.s" 2>/dev/null | strip_ansi \
                     | grep -v '^[[:space:]]*$' | head -3 | indent)
        fi
    elif [ ! -s "$o.s" ]; then
        why="skata emitted no assembly"
    elif [ "$mode" = compile ]; then
        # no main(): assemble only, never link
        if ! clang -w -c "$o.s" -o "$o.o" 2> "$o.aserr"; then
            why="clang rejected skata's assembly"
            detail=$(strip_ansi < "$o.aserr" | grep -m1 -A2 'error:' | indent)
        fi
    elif ! clang -w "$o.s" -o "$o.bin" 2> "$o.aserr"; then
        why="clang rejected skata's assembly"
        detail=$(strip_ansi < "$o.aserr" | grep -m1 -A2 'error:' | indent)
    else
        to 10 "./$o.bin" > "$o.stdout" 2>/dev/null; gotcode=$?
        if [ "$gotcode" = 124 ] && [ "$refcode" != 124 ]; then
            why="skata's program hangs ${D}(clang's exits ${refcode})${N}"
        elif [ "$gotcode" != "$refcode" ]; then
            why="exit code: ${BOLD}skata gave $gotcode${N}, ${BOLD}clang gave $refcode${N}"
        elif ! cmp -s "$o.stdout" "$o.ref.stdout"; then
            why="stdout differs from clang's ${D}(-clang +skata)${N}"
            detail=$(diff -u "$o.ref.stdout" "$o.stdout" | tail -n +3 | head -8 | indent)
        fi
    fi

    # ---------- report ----------
    if [ -z "$why" ]; then
        pass=$((pass+1))
        [ $VERBOSE = 1 ] && printf "${G}pass${N}  %s\n" "$name"
    else
        fail=$((fail+1)); failed+=("$name")
        printf "${R}FAIL${N}  ${BOLD}%s${N}\n      %s\n" "$name" "$why"
        [ -n "$detail" ] && printf "${D}%s${N}\n" "$detail"
    fi
done

echo
if [ $fail -eq 0 ]; then
    printf "${G}all %d passed${N}\n" "$pass"
else
    printf "${G}%d passed${N}, ${R}%d failed${N}\n" "$pass" "$fail"
    printf "${D}artifacts: %s/%s.s  |  rerun one: tests/run.sh %s${N}\n" \
        "$BUILD" "${failed[0]}" "${failed[0]}"
fi

[ $fail -eq 0 ]
