#!/bin/bash
CURRENT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

LIB_PATH="$CURRENT_DIR"

if [ -d "$CURRENT_DIR" ]; then
    while IFS= read -r dir; do
        if [[ ":$LIB_PATH:" != *":$dir:"* ]]; then
            LIB_PATH="$LIB_PATH:$dir"
        fi
    done < <(find "$CURRENT_DIR" -name "*.so*" -exec dirname {} \; | sort -u)
fi

export LD_LIBRARY_PATH="$LIB_PATH${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

echo "LD_LIBRARY_PATH set to: $LD_LIBRARY_PATH"
