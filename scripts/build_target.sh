#!/usr/bin/env bash
#
#
set -e

script_dir=$(readlink -f "$(dirname "$0")")
top_dir=${script_dir}/..

build_dir="${top_dir}/df_out"


pushd "${top_dir}"
  # Read build environment vars
  pushd "${script_dir}"
    # shellcheck disable=SC1091
    source ./build_env
  popd

  pushd target
    echo "Compiling the target source ...."

    ./build.sh "${TARGET_VER}" "${build_dir}" || die "ERR: Building target failed!"

    echo "Compiling the target source ....[DONE]"
  popd

  [[ -d "${build_dir}" ]] || die "ERR: Build directory does not exist: ${build_dir}"
  [[ -d "${build_dir}/rpm-build" ]] || die "ERR: RMP build directory does not exist: ${build_dir}/rpm-build"

  release_dir="${top_dir}/ansible/release"
  [[ -e ${release_dir} ]] || die "ERR: The Release directory does not exist: ${release_dir}"

  cp "${build_dir}"/rpm-build/RPMS/x86_64/*.rpm ${release_dir}/

  # TODO - This block needs to be removed
  # pushd "${build_dir}"
     # [[ -d NKV-Release ]] || mkdir -p NKV-Release
     # cp rpm-build/RPMS/x86_64/*.rpm NKV-Release
     #
     # cp rpm-build/RPMS/x86_64/*.rpm NKV-Release
     # tar -cf NKV-Release.tar NKV-Release
     # cp NKV-Release.tar "${top_dir}"/ansible/release/
  # popd
popd

