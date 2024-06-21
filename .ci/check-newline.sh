#!/usr/bin/env bash

set -e -u -o pipefail

ret=0
show=0
# Reference: https://medium.com/@alexey.inkin/how-to-force-newline-at-end-of-files-and-why-you-should-do-it-fdf76d1d090e
while IFS= read -rd '' f; do
    if file --mime-encoding "$f" | grep -qv binary; then
        tail -c1 < "$f" | read -r _ || show=1
        if [ $show -eq 1 ]; then
            echo "Warning: No newline at end of file $f"
            ret=1
            show=0
        fi
    fi
done < <(git ls-files -z examples)

exit $ret
