#!/usr/bin/env bash
# shellcheck disable=SC1090
#
# set -o xtrace

# Minimum and Maximum GCC versions
GCCMINVER=5.1.0
GCCMAXVER=5.5.0

# GCC Setenv scripts for CentOS
GCCSETENV=/usr/local/bin/setenv-for-gcc510.sh
GCCRESTORE=/usr/local/bin/restore-default-paths-gcc510.sh

die()
{
    echo "$*"
    exit 1
}

vercomp () {
    if [[ "$1" == "$2" ]]
    then
        return 0
    fi
    local IFS=.
    # shellcheck disable=SC2206
    local i ver1=($1) ver2=($2)
    # fill empty fields in ver1 with zeros
    for ((i=${#ver1[@]}; i<${#ver2[@]}; i++))
    do
        ver1[i]=0
    done
    for ((i=0; i<${#ver1[@]}; i++))
    do
        if [[ -z ${ver2[i]} ]]
        then
            # fill empty fields in ver2 with zeros
            ver2[i]=0
        fi
        if ((10#${ver1[i]} > 10#${ver2[i]}))
        then
            return 1
        fi
        if ((10#${ver1[i]} < 10#${ver2[i]}))
        then
            return 2
        fi
    done
    return 0
}

testvercomp () {
    vercomp "$1" "$2"
    case $? in
        0) op='=';;
        1) op='>';;
        2) op='<';;
    esac
    if [[ "$op" != "$3" ]]
    then
        return 1
    else
        return 0
    fi
}

# Load GCC 5.1.0 paths
if test -f "$GCCSETENV"
then
    source $GCCSETENV
fi

# Check gcc version
GCCVER=$(gcc --version | grep -oP '^gcc \([^)]+\) \K[^ ]+')

# Validate GCC version is supported
if testvercomp "$GCCVER" "$GCCMINVER" '<' || testvercomp "$GCCVER" "$GCCMAXVER" '>'
then
    die "ERROR - Found GCC version: $GCCVER. Must be between $GCCMINVER and $GCCMAXVER."
else
    echo "Supported GCC version found: $GCCVER"
fi

target_dir=$(readlink -f "$(dirname "$0")")
build_dir="${target_dir}/../df_out"
rpm_build_dir="${build_dir}/rpm-build"
rpm_spec_file="${rpm_build_dir}/SPECS/total.spec"

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
/usr/dss/nkv-target/lib/libjudyL.so

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
        -b=*|--build-type=*)
        TYPE="${i#*=}"
        if [ "$TYPE" == "debug" ] ; then
            BUILD_TYPE="debug"
        else
            BUILD_TYPE="release"
        fi
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

parse_options "$@"
echo "Build rockdb: $BUILD_ROCKSDB"
echo "Target Version: $TARGET_VER"
echo "Build Type: $BUILD_TYPE"

[[ -d "${build_dir}" ]] && rm -rf "${build_dir}"

mkdir -p "${build_dir}"
mkdir -p "${rpm_build_dir}"

packageName="nkv-target"
targetVersion=${TARGET_VER}
gitVersion="$(git describe --abbrev=4 --always --tags)"


pushd "${target_dir}/oss" || die "Can't change to ${target_dir}/oss dir"
    ./apply-patch.sh
popd || die "Can't change exit ${target_dir}/oss dir"

pushd "${build_dir}" || die "Can't change to ${build_dir} dir"
    pushd "${target_dir}" || die "Can't change to ${target_dir} dir"
        updateVersionInHeaderFile "${targetVersion}" "${gitVersion}"
    popd || die "Can't exit ${target_dir} dir"

    if [ "$BUILD_TYPE" = "debug" ] ; then
        cmake "${target_dir}" -DCMAKE_BUILD_TYPE=Debug -DBUILD_MODE_DEBUG=ON
    elif [ "$BUILD_TYPE" = "release" ]; then
        cmake "${target_dir}" -DCMAKE_BUILD_TYPE=Debug -DBUILD_MODE_RELEASE=ON
    else
    echo "Making in default mode"
        cmake "${target_dir}" -DCMAKE_BUILD_TYPE=Debug
    fi

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

popd || die "Can't exit ${build_dir} dir"

# Restore default GCC paths
if test -f "$GCCRESTORE"; then
    source $GCCRESTORE
fi
