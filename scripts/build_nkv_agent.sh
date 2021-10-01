#!/usr/bin/env bash
set -e

script_dir=$(readlink -f "$(dirname "$0")")
top_dir=${script_dir}/..

# Read build environment vars
pushd "${script_dir}"

# shellcheck disable=SC1091
source ./build_env

# Build NKV Agnet
echo "Building  NKV Agent"

pushd "${top_dir}/ufm/agents/nkv_agent/"
./makenkvagentpackage.sh -pr -J "${AGENT_VER}"

popd
