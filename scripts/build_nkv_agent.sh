#!/usr/bin/env bash
#
#
set -e

script_dir=$(readlink -f "$(dirname "$0")")
top_dir=${script_dir}/..

# Read build environment vars
pushd "${script_dir}"

# shellcheck disable=SC1091
source ./build_env

release_dir="${top_dir}/ansible/release"

[[ ! -e ${release_dir} ]] && die "ERR: The Release directory does not exist: ${release_dir}"

# Clean existing RPM/DEB packages
echo "Removing existing RPM/DEB packages"
rm -f "${release_dir}/nkvagent*.deb"
rm -f "${release_dir}/nkvagent*.rpm"

# Build NKV Agnet
echo "Building  NKV Agent"

pushd "${top_dir}/ufm/agents/nkv_agent/"
./makenkvagentpackage.sh -pr -J "${AGENT_VER}"

cp ./*.rpm "${release_dir}/"
cp ./*.deb "${release_dir}/"
popd

