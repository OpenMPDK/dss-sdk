#! /usr/bin/bash
set -e

script_dir=$(readlink -f "$(dirname "$0")")
top_dir=${script_dir}/../

# Read build environment vars
pushd "${script_dir}"
# shellcheck disable=SC1091
. ./build_env
popd

echo "Compiling the target source ...."
pushd "${top_dir}"/target
./build.sh 
popd
echo "Compiling the target source ....[DONE]"

# Build Target
build_dir=${top_dir}/build_dir

rm -rf "${build_dir}"
mkdir -p "${build_dir}"/SRPMS
mkdir -p "${build_dir}"/SPECS
mkdir -p "${build_dir}"/RPMS/x86_64
mkdir -p "${build_dir}"/BUILD/NKV
mkdir -p "${build_dir}"/BUILDROOT

# Populate the files to the build path
nkv_dir=${build_dir}/BUILD/NKV
mkdir -p "${nkv_dir}"/usr/bin

# copy ufm/nkv_agent folder over to usr
cp -rf "${top_dir}"/ufm/agents/nkv_agent "${nkv_dir}"/usr

mkdir -p "${nkv_dir}"/etc/systemd/system
cp -rf "${top_dir}"/ufm/agents/nkv_agent/service_scripts/*.service "${nkv_dir}"/etc/systemd/system/

mkdir -p "${nkv_dir}"/etc/rsyslog.d
cat > "${nkv_dir}"/etc/rsyslog.d/dfly.conf << EOF
if \$programname == 'dfly' or \$syslogtag == '[dfly]:' \\
then -/var/log/dragonfly/dfly.log
&  stop
EOF

mkdir -p "${nkv_dir}"/usr/dragonfly/scripts
cp "${top_dir}"/target/oss/spdk_tcp/scripts/common.sh "${nkv_dir}"/usr/dragonfly/scripts/
cp "${top_dir}"/target/oss/spdk_tcp/scripts/setup.sh "${nkv_dir}"/usr/dragonfly/scripts/
cp "${top_dir}"/target/scripts/nkv_tgt_conf.py "${nkv_dir}"/usr/dragonfly/scripts/
cp "${top_dir}"/df_out/oss/spdk_tcp/app/nvmf_tgt/nvmf_tgt "${nkv_dir}"/usr/dragonfly
cp "${top_dir}"/df_out/dssd/cmd/ustat/ustat "${nkv_dir}"/usr/dragonfly

mkdir -p "${nkv_dir}"/var/lib/etcd

cp "${script_dir}"/genrpm.sh "${build_dir}"/SPECS/

if ! sh "${build_dir}"/SPECS/genrpm.sh "${build_dir}" "$TARGET_VER" "$BASE_VER" "$AGENT_VER" "$MONITOR_VER"
then
	echo "RPM creation failed"
	exit
fi

mkdir "${build_dir}"/NKV-Release
cp "${build_dir}"/RPMS/x86_64/*.rpm "${build_dir}"/NKV-Release
pushd "${build_dir}"
tar -cf NKV-Release.tar NKV-Release
popd

# Copy NKV-Release.tar to Ansible release dir
cp "${build_dir}"/NKV-Release.tar "${top_dir}"/ansible/release/
