#!/usr/bin/env bash
#
#
# set -o xtrace

target_dir=$(readlink -f "$(dirname "$0")")
top_dir=${target_dir}/..


die()
{
    echo "$*"
    exit 1
}

buildTarget()
{
    local build_dir=$1

    local TGT_HASH=""
    TGT_HASH=$(git rev-parse --verify HEAD) || die "ERR: Fail to get git hash of repo"

    [[ -z ${TGT_VER} ]] && TGT_VER=6.6.666

    sed -i -e "s/^\#define OSS_TARGET_VER.\+$/#define OSS_TARGET_VER \"$TGT_VER\"/" include/version.h
    sed -i -e "s/^\#define OSS_TARGET_HASH.\+$/#define OSS_TARGET_HASH \"$TGT_HASH\"/" include/version.h

    #source ~/.dragonfly

    rm -rf "${build_dir}"
    mkdir -p "${build_dir}"

    pushd "${target_dir}/oss"
      ./apply-patch.sh
    popd

    pushd "${build_dir}"
      cmake "${target_dir}" -DCMAKE_BUILD_TYPE=Debug
      # || die "ERR: Target build failed"
      make spdk_tcp
      # || die "ERR: Failed to build spdk_tcp"
    popd
}

makeRpmBuildDirectories() {
  echo "empty"
}

buildPackage()
{
    local build_dir=$1
    local targetVersion=$2

    local rpm_build_dir="${buildDir}/rpm-build"

    rm -rf "${rpm_build_dir}"
    mkdir -p "${rpm_build_dir}"

    pushd "${rpm_build_dir}"

    mkdir -p SRPMS
    mkdir -p SPECS
    mkdir -p RPMS/x86_64
    mkdir -p BUILD/nkv-target
    mkdir -p BUILD/nkv-target/usr/dss
    mkdir -p BUILD/nkv-target/etc/rsyslog.d
    mkdir -p BUILD/nkv-target/etc/systemd/system
    mkdir -p BUILDROOT

    #cp -rf "${target_dir}"/scripts/*.service BUILD/nkv-target/etc/systemd/system/

    cat > BUILD/nkv-target/etc/rsyslog.d/dfly.conf << LAB_CONF
if \$programname == 'dfly' or \$syslogtag == '[dfly]:' \\
then -/var/log/dragonfly/dfly.log
&  stop
LAB_CONF

    rpmSpecFile="${rpm_build_dir}/SPECS/total.spec"

    generateSpecFile "${rpmSpecFile}" "${targetVersion}"

    generateRPM "${rpmSpecFile}" "${rpm_build_dir}"

    cp -rf "${build_dir}/nkv-target" BUILD/nkv-target/usr/dss/
    cp RPMS/x86_64/*.rpm "${build_dir}/"

    popd
}

generateSpecFile()
{
    local rpmSpecFile=$1
    local targetVersion=$2

    [[ -e ${rpmSpecFile} ]] && rm -f "${rpmSpecFile}"

cat > "${rpmSpecFile}" <<LAB_SPEC

####### NKV Target Package ###########
Name:		nkv-target
Version:        ${targetVersion}
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
#/etc/systemd/system/nvmf_tgt.service
#/etc/systemd/system/nvmf_tgt@.service
/etc/rsyslog.d/dfly.conf
/usr/dss/nkv-target/include/spdk/pci_ids.h
/usr/dss/nkv-target/lib/libdssd.a
/usr/dss/nkv-target/lib/liboss.a

%post
chmod +x /usr/dss/nkv-target/scripts/setup.sh
chmod +x /usr/dss/nkv-target/scripts/common.sh

set -x
{
function check_service(){
	serv=\$1
	action=\$2
	stage=\$3
        status=1
	try=0
	while [ \$try -lt 3 ]; do
		systemctl \$action \$serv
		sleep 1
		systemctl status \$serv|grep "Active: active"
		if [ \$? -eq 0 ]; then
			status=0
			break
		fi
		sleep 1
		try=\$((try+1))
	done
	if [ \$stage == 'upgrade' ]; then
		if [ \$status -eq 0 ]; then
			etcdctl put /software/upgrade/progress/node/\$(hostname -s)/services/\$serv up
		else
			etcdctl put /software/upgrade/progress/node/\$(hostname -s)/services/\$serv down
		fi
	fi
}

cat > /etc/ld.so.conf.d/kvlibs.conf << EOF
/usr/dss/nkv-target/
EOF

/usr/sbin/ldconfig
systemctl daemon-reload
if [ \$1 -eq 1 ];then
	# Fresh installation
	systemctl enable nvmf_tgt
	systemctl enable nvmf_tgt@internal_flag.service
	check_service nvmf_tgt start install
	check_service nvmf_tgt@internal_flag start install
	check_service rsyslog restart install
else
	# Upgrade stage
	check_service nvmf_tgt restart upgrade
	check_service rsyslog restart upgrade
fi
} &> /tmp/fm-agent-output

%postun
if [ \$1 -eq 0 ];then
	systemctl stop nvmf_tgt
	rm -f /etc/rsyslog.d/dfly.conf
	rm -f /etc/ld.so.conf.d/kvlibs.conf
	/usr/sbin/ldconfig
fi
%systemd_postun_with_restart rsyslog.service
systemctl daemon-reload

LAB_SPEC
}

generateRPM()
{
    local spectFile=$1
    local rpmBuildDir=$2

    [[ ! -e ${spectFile} ]] && die "ERR: Spec file does not exist"
    [[ ! -d ${rpmBuildDir} ]] && die "ERR: RPM build directory does not exist"

    if ! eval "rpmbuild -bb --clean ${spectFile}  --define '_topdir ${rpmBuildDir}'"
	then
        die "ERR: Failed to build RPM"
    fi
}

usage()
{
    cat <<LAB_USAGE

$(basename "$0")  v1.0.0

usage:
    -b    RPM build directory
    -v    Version number string
    -d    Do not build package

    -h    Show this help

LAB_USAGE
}


targetVersion="0.5.0"
buildDir="${top_dir}/df_out"
excludePackangeBuild=0

while getopts "b:v:dhu" opt
do
    case $opt in
        b)
            [[ -z ${OPTARG} ]] && die "ERR: Invalid directory name"
            buildDir="${top_dir}/${OPTARG}"
            ;;
        v)
            [[ -z ${OPTARG} ]] && die "ERR: Invalid version string"
            targetVersion="${OPTARG}"
            ;;
        d)
            excludePackangeBuild=1
            ;;
        h|u|?)
            usage
            exit 1
            ;;
    esac
done

[[ ! -e ${buildDir} ]] && mkdir -p "${buildDir}" 

buildTarget "${buildDir}"

[[ ${excludePackangeBuild} -eq 1 ]] && exit 0

buildPackage "${buildDir}" "${targetVersion}"

exit 0

