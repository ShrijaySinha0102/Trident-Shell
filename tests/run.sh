#!/usr/bin/env bash
# Runs every tests/cases/*.sh through nsh and bash, diffs stdout and compares exit codes.
# Expected-difference cases use hand-written NAME.expected.out or NAME.expected.exit files.

cd "$(dirname "$0")/.." || exit 1
pass=0; fail=0

NSH_CMD="${NSH_WRAPPER:-} ./nsh"

for f in tests/cases/*.sh; do
    [ -e "$f" ] || continue
    name=$(basename "$f" .sh)
    
    # Run bash
    bash_out=$(bash --norc < "$f" 2>/dev/null)
    bash_exit=$?
    
    # Run nsh
    # we need to redirect stderr to null to not mess up stdout diff
    got=$($NSH_CMD < "$f" 2>/dev/null)
    nsh_exit=$?
    
    # Check overrides
    if [ -f "tests/cases/${name}.expected.out" ]; then
        exp=$(cat "tests/cases/${name}.expected.out")
    else
        exp="$bash_out"
    fi
    
    if [ -f "tests/cases/${name}.expected.exit" ]; then
        exp_exit=$(cat "tests/cases/${name}.expected.exit")
    else
        exp_exit=$bash_exit
    fi
    
    failed=0
    if [ "$exp" != "$got" ]; then
        echo "FAIL: $f (stdout mismatch)"
        echo "  Expected: '$exp'"
        echo "  Got:      '$got'"
        failed=1
    fi
    
    if [ "$exp_exit" != "$nsh_exit" ]; then
        echo "FAIL: $f (exit code mismatch: expected $exp_exit, got $nsh_exit)"
        failed=1
    fi
    
    if [ $failed -eq 0 ]; then
        pass=$((pass+1))
    else
        fail=$((fail+1))
    fi
done

echo "passed=$pass failed=$fail"
[ "$fail" -eq 0 ]
