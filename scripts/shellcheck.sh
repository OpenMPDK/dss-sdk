#!/usr/bin/env bash
# Run shellcheck and generate checkstyle format reports for each file
# For use in CICD pipeline
set -e

# Set path variables
SCRIPT_DIR=$(readlink -f "$(dirname "$0")")
TOP_DIR="${SCRIPT_DIR}/.."

pushd "${TOP_DIR}"
    # Get list of shell scripts in repo
    mapfile -t SHELLSCRIPTS < <(git ls-files '*.sh')

    i=0
    for SCRIPT in "${SHELLSCRIPTS[@]}"
    do
        set +e
        shellcheck "${SCRIPT}" --format=checkstyle > "sc${i}.xml"
        set -e
        ((i=i+1))
    done
popd
