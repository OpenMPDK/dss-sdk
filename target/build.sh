#!/usr/bin/env bash
# shellcheck disable=SC1090
#
# set -o xtrace

# Build/install GCC 5.1.0 RPM from gcc-builder: https://github.com/BobSteagall/gcc-builder
GCCSETENV=/usr/local/bin/setenv-for-gcc510.sh
GCCRESTORE=/usr/local/bin/restore-default-paths-gcc510.sh

# Load GCC 5.1.0 paths
if test -f "$GCCSETENV"; then
    source $GCCSETENV
fi

target_dir=$(readlink -f "$(dirname "$0")")
build_dir="${target_dir}/../df_out"
rpm_build_dir="${build_dir}/rpm-build"
rpm_spec_file="${rpm_build_dir}/SPECS/total.spec"


die()
{
    echo "$*"
    exit 1
}

updateVersionInHeaderFile()
{
    local targetVersion=$1
    local gitHash=$2

    [[ -z "${targetVersion}" ]] && die "ERR: Version string is empty!"
    [[ -z "${gitHash}" ]] && die "ERR: Invalid git hash "

    sed -i -e "s/^\#define OSS_TARGET_VER.\+$/#define OSS_TARGET_VER \"${targetVersion}\"/" include/version.h
    sed -i -e "s/^\#define OSS_TARGET_GIT_VER.\+$/#define OSS_TARGET_GIT_VER \"${gitHash}\"/" include/version.h
}

makePackageDirectories()
{
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
    local packageName=$2
    local gitVer=$3

	#replace hyphen with underscore for file name
	gitVer=${gitVer//[-]/_}

    [[ -z "${gitVer}" ]] && die "ERR: Git Version string is empty!"
    [[ -e "${rpmSpecFile}" ]] && rm -f "${rpmSpecFile}"


    cat > "$rpmSpecFile" <<LAB_SPEC

####### NKV Target Package ###########
Name: ${packageName}
Version: ${gitVer}
Release: 1%{?dist}
Summary: DSS NKV Target Release
License: GPLv3+
Vendor: MSL-SSD

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
/usr/dss/nkv-target/bin/mkfs_blobfs
/usr/dss/nkv-target/bin/dss_target.py
/usr/dss/nkv-target/bin/ustat
/usr/dss/nkv-target/scripts/setup.sh
/usr/dss/nkv-target/scripts/common.sh
/usr/dss/nkv-target/scripts/rpc.py
/usr/dss/nkv-target/scripts/dss_rpc.py
/usr/dss/nkv-target/scripts/rpc/

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

    if ! eval "rpmbuild -bb --clean $rpmSpecFile --define '_topdir $rpmTmp' "
    then
        return 1
    fi
    echo "RPM build success"

    return 0
}

createDragonflyConfigFile()
{
    local filename=$1

    cat > "${filename}" << LAB_DFLY_CONF
if \$programname == 'dfly' or \$syslogtag == '[dfly]:' \\
then -/var/log/dragonfly/dfly.log
&  stop
LAB_DFLY_CONF
}

parse_options()
{
	for i in "$@"
	do
	case $i in
		--rocksdb)
		BUILD_ROCKSDB=true
		shift # past argument=value
		;;
		-v=*|--version=*)
		TARGET_VER="${i#*=}"
		shift # past argument=value
		;;
		*)
			  # unknown option
		;;
	esac
	done
}
####################### main #######################################

BUILD_ROCKSDB=false
TARGET_VER="0.5.0"

parse_options $@
echo "Build rockdb: $BUILD_ROCKSDB"
echo "Target Version: $TARGET_VER"

[[ -d "${build_dir}" ]] && rm -rf "${build_dir}"

mkdir -p "${build_dir}"
mkdir -p "${rpm_build_dir}"

packageName="nkv-target"
# packageRevision=1
# TODO(ER) - Add switch to Job number
jenkingJobnumber=0
targetVersion=${TARGET_VER}
gitVersion="$(git describe --abbrev=4 --always --tags)"


pushd "${target_dir}"/oss
    ./apply-patch.sh
popd

pushd "${build_dir}"
    pushd "${target_dir}"
        updateVersionInHeaderFile "${targetVersion}" "${gitVersion}"
    popd

    cmake "${target_dir}" -DCMAKE_BUILD_TYPE=Debug
	if $BUILD_ROCKSDB;then
		make rocksdb
	else
		make spdk_tcp
	fi

    makePackageDirectories "${rpm_build_dir}"

    createDragonflyConfigFile "${rpm_build_dir}"/BUILD/nkv-target/etc/rsyslog.d/dfly.conf

    cp -rf "${build_dir}"/nkv-target "${rpm_build_dir}"/BUILD/nkv-target/usr/dss/

    generateSpecFile "${rpm_spec_file}" "${packageName}" "${gitVersion}"
    generateRPM "${rpm_spec_file}" "${rpm_build_dir}" || die "ERR: Failed to build RPM"

    cp "${rpm_build_dir}"/RPMS/x86_64/*.rpm "${build_dir}"/

popd

# Restore default GCC paths
if test -f "$GCCRESTORE"; then
    source $GCCRESTORE
fi
