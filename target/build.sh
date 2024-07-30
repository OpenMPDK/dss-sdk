#!/usr/bin/env bash
# shellcheck source=/dev/null
# shellcheck disable=SC1090
#
# set -o xtrace
set -e

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

load_and_test_gcc() {
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
}

TARGET_DIR=$(cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd)
PROJECT_ROOT=$(realpath "$TARGET_DIR/../")
BUILD_DIR="$PROJECT_ROOT/df_out"
RPM_BUILD_DIR="$BUILD_DIR/rpm-build"
RPM_SPEC_FILE="$RPM_BUILD_DIR/SPECS/total.spec"

updateVersionInHeaderFile()
{
    local targetVersion=$1
    local gitHash=$2

    if [[ -z "$targetVersion" ]]
    then
        die "ERR: Version string is empty!"
    fi

    if [[ -z "$gitHash" ]]
    then
        die "ERR: Invalid git hash "
    fi

    sed -i -e "s/^\#define OSS_TARGET_VER.\+$/#define OSS_TARGET_VER \"$targetVersion\"/" include/version.h
    sed -i -e "s/^\#define OSS_TARGET_GIT_VER.\+$/#define OSS_TARGET_GIT_VER \"$gitHash\"/" include/version.h
}

makePackageDirectories()
{
    local rpmBuildDir=$1

    mkdir -p "$rpmBuildDir/SRPMS"
    mkdir -p "$rpmBuildDir/SPECS"
    mkdir -p "$rpmBuildDir/RPMS/x86_64"
    mkdir -p "$rpmBuildDir/BUILD/nkv-target"
    mkdir -p "$rpmBuildDir/BUILDROOT"
    mkdir -p "$rpmBuildDir/BUILD/nkv-target/usr/dss/"
    mkdir -p "$rpmBuildDir/BUILD/nkv-target/etc/rsyslog.d"
}

generateSpecFile()
{
    local rpmSpecFile=$1
    local packageName=$2
    local gitVer=$3

    #replace hyphen with underscore for file name
    gitVer=${gitVer//[-]/_}

    if [[ -z "$gitVer" ]]
    then
        die "ERR: Git Version string is empty!"
    fi
    
    if [[ -e "$rpmSpecFile" ]]
    then
        rm -f "$rpmSpecFile"
    fi


    cat > "$rpmSpecFile" <<LAB_SPEC

####### NKV Target Package ###########
Name: $packageName
Version: $gitVer
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
/usr/dss/nkv-target/bin/$DSS_FORMAT_TOOL
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

    cat > "$filename" << LAB_DFLY_CONF
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
        --only-rocksdb)
        BUILD_ROCKSDB=true
        shift # past argument=value
        ;;
        --with-rocksdb-kv)
        BUILD_WITH_ROCKSDB_KV=true
        shift # past argument=value
        ;;
        -v=*|--version=*)
        TARGET_VER="${i#*=}"
        shift # past argument=value
        ;;
        -b=*|--build-type=*)
        TYPE="${i#*=}"
        if [ "$TYPE" == "debug" ]
        then
            BUILD_TYPE="debug"
        else
            BUILD_TYPE="release"
        fi
        shift # past argument=value
        ;;
        --with-coverage)
        BUILD_WITH_COVERAGE=true
        ;;
        --run-tests)
        RUN_TESTS=true
        ;;
        *)
              # unknown option
        ;;
    esac
    done
}
####################### main #######################################

BUILD_ROCKSDB=false
BUILD_WITH_ROCKSDB_KV=false
BUILD_WITH_COVERAGE=false
RUN_TESTS=false
TARGET_VER="0.5.0"

parse_options "$@"
echo "Build rockdb only: $BUILD_ROCKSDB"
echo "Building with coverage: $BUILD_WITH_COVERAGE"
echo "Target Version: $TARGET_VER"
echo "Build Type: $BUILD_TYPE"

if [[ -d "$BUILD_DIR" ]]
then
    rm -rf "$BUILD_DIR"
fi

if [[ 'true' =  "$BUILD_WITH_ROCKSDB_KV" ]]
then
    DSS_FORMAT_TOOL="mkfs_blobfs"
else
    DSS_FORMAT_TOOL="dss_formatter"
fi

mkdir -p "$BUILD_DIR"
mkdir -p "$RPM_BUILD_DIR"

packageName="nkv-target"
targetVersion=$TARGET_VER
gitVersion="$(git describe --abbrev=4 --always --tags)"

DSS_TARGET_CMAKE_OPTIONS=()

pushd "$TARGET_DIR/oss"
    ./apply-patch.sh
popd

pushd "$BUILD_DIR"
    pushd "$TARGET_DIR"
        updateVersionInHeaderFile "$targetVersion" "$gitVersion"
    popd

    if $BUILD_WITH_COVERAGE
    then
       DSS_TARGET_CMAKE_OPTIONS+=("-DWITH_COVERAGE=ON")
    fi

    if [[ "$BUILD_WITH_ROCKSDB_KV" = true || "$BUILD_ROCKSDB" = true ]]
    then
       DSS_TARGET_CMAKE_OPTIONS+=("-DWITH_ROCKSDB_KV=ON")
       load_and_test_gcc
    fi

    DSS_TARGET_CMAKE_OPTIONS+=("-DCMAKE_BUILD_TYPE=Debug")

    if [ "$BUILD_TYPE" = "debug" ]
    then
        DSS_TARGET_CMAKE_OPTIONS+=("-DBUILD_MODE_DEBUG=ON")
    elif [ "$BUILD_TYPE" = "release" ]
    then
        DSS_TARGET_CMAKE_OPTIONS+=("-DBUILD_MODE_RELEASE=ON")
    else
        echo "Making in default mode"
    fi

    cmake "${DSS_TARGET_CMAKE_OPTIONS[@]}" "$TARGET_DIR"

    if $BUILD_ROCKSDB
    then
        make "-j$(nproc)" rocksdb
    else
        make "-j$(nproc)"
    fi

    if $RUN_TESTS
    then
        make "-j$(nproc)" test
        if $BUILD_WITH_COVERAGE
        then
            echo "Generating sonarqube coverage report"
            mkdir -p reports
            #gcovr needs to be installed from pip
            gcovr \
                --sonarqube reports/sonar_qube_ut_coverage_report.xml \
                --xml reports/cobertura.xml \
                --txt \
                --root "$PROJECT_ROOT" \
                "$BUILD_DIR"
        fi
    fi

    makePackageDirectories "$RPM_BUILD_DIR"

    createDragonflyConfigFile "$RPM_BUILD_DIR/BUILD/nkv-target/etc/rsyslog.d/dfly.conf"

    cp -rf "$BUILD_DIR/nkv-target" "$RPM_BUILD_DIR/BUILD/nkv-target/usr/dss/"

    # Only build RPM for centos or rocky
    source /etc/os-release
    if [[ ( $ID == 'centos' ) || ( $ID == 'rocky' )]]; then
        generateSpecFile "$RPM_SPEC_FILE" "$packageName" "$gitVersion"
        generateRPM "$RPM_SPEC_FILE" "$RPM_BUILD_DIR" || die "ERR: Failed to build RPM"
        cp "$RPM_BUILD_DIR"/RPMS/x86_64/*.rpm "$BUILD_DIR"/
    fi

popd

# Restore default GCC paths
if test -f "$GCCRESTORE"
then
    source $GCCRESTORE
fi
