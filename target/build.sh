#!/usr/bin/env bash
#
#
# set -o xtrace

target_dir=$(readlink -f "$(dirname "$0")")



if [ "$#" -ne 1 ]; then
    TARGET_VER="0.5.0"
else
    TARGET_VER="$1"
fi
TGT_HASH=`git rev-parse --verify HEAD`
sed -i -e "s/^\#define OSS_TARGET_VER.\+$/#define OSS_TARGET_VER \"$TGT_VER\"/" include/version.h
sed -i -e "s/^\#define OSS_TARGET_HASH.\+$/#define OSS_TARGET_HASH \"$TGT_HASH\"/" include/version.h


CWDNAME=`basename "${target_dir}"`
build_dir="${target_dir}/../df_out"
rpm_build_dir="${build_dir}/rpm-build"
rpm_spec_file="${rpm_build_dir}/SPECS/total.spec"

die()
{
    echo "$*"
    exit 1
}

makePackageDirectories() {
    local rpmBuildDir=$1

    mkdir -p "${rpmBuildDir}"/SRPMS
    mkdir -p "${rpmBuildDir}"/SPECS
    mkdir -p "${rpmBuildDir}"/RPMS/x86_64
    mkdir -p "${rpmBuildDir}"/BUILD/nkv-target
    mkdir -p "${rpmBuildDir}"/BUILDROOT
    mkdir -p "${rpmBuildDir}"/BUILD/nkv-target/usr/dss/
    mkdir -p "${rpmBuildDir}"/BUILD/nkv-target/etc/rsyslog.d
}

generateSpecFile()
{
    local rpmSpecFile=$1
    local targetVer=$2

    [[ -e "${rpmSpecFile}" ]] && rm -f "${rpmSpecFile}"

    cat > "$rpmSpecFile" <<LAB_SPEC

####### NKV Target Package ###########
Name:		nkv-target
Version:        ${targetVer}
Release:        1%{?dist}
Summary:        DSS NKV Target Release
License:        GPLv3+
Vendor:         MSL-SSD

Prefix: /usr/dss/nkv-target

%description
An DSS Target NVMe-oF KV Storage Service.

# Common install for all below subpackages
%install
rm -rf %{buildroot}
cp -a %{name} %{buildroot}
exit 0

# File section for nkv-target
%files
%defattr(-,root,root,-)
%dir /usr/dss/nkv-target
/usr/dss/nkv-target/bin/nvmf_tgt
/usr/dss/nkv-target/bin/dss_target.py
/usr/dss/nkv-target/bin/ustat
/usr/dss/nkv-target/scripts/setup.sh
/usr/dss/nkv-target/scripts/common.sh

/etc/rsyslog.d/dfly.conf
/usr/dss/nkv-target/include/spdk/pci_ids.h
/usr/dss/nkv-target/lib/libdssd.a
/usr/dss/nkv-target/lib/liboss.a

%post

chmod +x /usr/dss/nkv-target/scripts/setup.sh
chmod +x /usr/dss/nkv-target/scripts/common.sh

cat > /etc/ld.so.conf.d/kvlibs.conf << LAB_LDCONFIG
/usr/dss/nkv-target/
LAB_LDCONFIG

/usr/sbin/ldconfig

systemctl daemon-reload
systemctl rsyslog restart
echo "rsyslog started (\$?)"

%postun
if [ \$1 -eq 0 ]
then
    systemctl stop nvmf_tgt
    systemctl rsyslog stop

    rm -f /etc/rsyslog.d/dfly.conf
    rm -f /etc/ld.so.conf.d/kvlibs.conf

    /usr/sbin/ldconfig
fi

%systemd_postun_with_restart rsyslog.service
systemctl daemon-reload

echo "Done (\$?)"
LAB_SPEC
}

generateRPM()
{
    local rpmSpecFile=$1
    local rpmTmp=$2

    # /usr/bin/env
    if ! eval "rpmbuild -bb --clean $rpmSpecFile --define '_topdir $rpmTmp' "
    then
        die "ERR: Failed to build RPM"
    fi
    echo "RPM build success"
}


rm -rf $build_dir

cd $target_dir/oss
./apply-patch.sh

mkdir $build_dir
mkdir $rpm_build_dir

cd ${build_dir}

cmake $target_dir -DCMAKE_BUILD_TYPE=Debug
make spdk_tcp


makePackageDirectories ${rpm_build_dir}

cat > ""${rpm_build_dir}"/BUILD/nkv-target"/etc/rsyslog.d/dfly.conf << EOF
if \$programname == 'dfly' or \$syslogtag == '[dfly]:' \\
then -/var/log/dragonfly/dfly.log
&  stop
EOF

# cp "${target_dir}"/scripts/genrpm.sh "${rpm_build_dir}"/SPECS/
cp -rf ${build_dir}/nkv-target "${rpm_build_dir}"/BUILD/nkv-target/usr/dss/

generateSpecFile ${rpm_spec_file} ${TARGET_VER}
generateRPM ${rpm_spec_file} ${rpm_build_dir}

#if ! sh "${rpm_build_dir}"/SPECS/genrpm.sh "${rpm_build_dir}" "$TARGET_VER"
#then
#    die "ERR: nkv-target RPM creation failed"
#fi

cp "${rpm_build_dir}"/RPMS/x86_64/*.rpm "${build_dir}"/
