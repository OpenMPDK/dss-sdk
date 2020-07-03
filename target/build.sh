#!/bin/bash

set -o xtrace

target_dir=$(readlink -f "$(dirname "$0")")

if [ "$#" -ne 1 ]; then
    TARGET_VER="0.5.0"
else
    TARGET_VER="$1"
fi
TGT_HASH=`git rev-parse --verify HEAD`
sed -i -e "s/^\#define OSS_TARGET_VER.\+$/#define OSS_TARGET_VER \"$TGT_VER\"/" include/version.h
sed -i -e "s/^\#define OSS_TARGET_HASH.\+$/#define OSS_TARGET_HASH \"$TGT_HASH\"/" include/version.h

#source ~/.dragonfly

CWDNAME=`basename "${target_dir}"`
build_dir="${target_dir}/../df_out"

rm -rf $build_dir

cd $target_dir/oss
./apply-patch.sh

mkdir $build_dir
mkdir $build_dir/rpm-build
cd ${build_dir}

cmake $target_dir -DCMAKE_BUILD_TYPE=Debug
make spdk_tcp

mkdir -p "${build_dir}"/rpm-build/SRPMS
mkdir -p "${build_dir}"/rpm-build/SPECS
mkdir -p "${build_dir}"/rpm-build/RPMS/x86_64
mkdir -p "${build_dir}"/rpm-build/BUILD/nkv-target
mkdir -p "${build_dir}"/rpm-build/BUILDROOT
mkdir -p "${build_dir}"/rpm-build/BUILD/nkv-target/usr/dss/


#mkdir -p "${build_dir}"/rpm-build/BUILD/nkv-target/etc/systemd/system
#cp -rf "${target_dir}"/scripts/*.service "${build_dir}"/rpm-build/BUILD/nkv-target/etc/systemd/system/

mkdir -p ""${build_dir}"/rpm-build/BUILD/nkv-target"/etc/rsyslog.d
cat > ""${build_dir}"/rpm-build/BUILD/nkv-target"/etc/rsyslog.d/dfly.conf << EOF
if \$programname == 'dfly' or \$syslogtag == '[dfly]:' \\
then -/var/log/dragonfly/dfly.log
&  stop
EOF

cp "${target_dir}"/scripts/genrpm.sh "${build_dir}"/rpm-build/SPECS/
cp -rf ${build_dir}/nkv-target "${build_dir}"/rpm-build/BUILD/nkv-target/usr/dss/

if ! sh "${build_dir}"/rpm-build/SPECS/genrpm.sh "${build_dir}/rpm-build" "$TARGET_VER"
then
	echo "nkv-target RPM creation failed"
	exit
fi

cp "${build_dir}"/rpm-build/RPMS/x86_64/*.rpm "${build_dir}"/
