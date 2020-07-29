#!/usr/bin/env bash
#
#
set -e

script_dir=$(readlink -f "$(dirname "$0")")
top_dir=${script_dir}/..
release_dir="${top_dir}/ansible/release"

die()
{
    echo "$*"
    exit 1
}

# This function should be removed when the ansible not
# longer extracts rpms from a tar file
#
aVeryUglyFunctionThetMusetBeRemoved()
{
    local topDir=$1
    local releaseDir=$2

    [[ -e ${releaseDir} ]] || die "ERR: The Release directory does not exist: ${releaseDir}"

    local build_dir="${topDir}/df_out"

    pushd "${topDir}"
      [[ -d "${build_dir}" ]] || die "ERR: Build directory does not exist: ${build_dir}"
      [[ -d "${build_dir}/rpm-build" ]] || die "ERR: RMP build directory does not exist: ${build_dir}/rpm-build"

      pushd "${build_dir}"
         mkdir -p NKV-Release

         cp "${topDir}"/ansible/release/nkv-target*.rpm NKV-Release
         cp "${topDir}"/ansible/release/nkvagent*.rpm NKV-Release

         tar -cf NKV-Release.tar NKV-Release

         # Copy NKV-Release.tar to Ansible release dir
         cp NKV-Release.tar "${topDir}"/ansible/release/
      popd
    popd
}

pushd "${script_dir}"
    [[ -d ${release_dir} ]] || mkdir -p ${release_dir}

    ./build_target.sh
    ./build_nkv_agent.sh

    aVeryUglyFunctionThetMusetBeRemoved "${top_dir}" "${release_dir}"

    ./build_ufm.sh
popd





