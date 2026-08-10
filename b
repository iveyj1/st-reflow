#!/usr/bin/env bash 
set -euo pipefail
make clean
make
if [[ ${1-} == -i ]]; then 
    sudo make install
fi

