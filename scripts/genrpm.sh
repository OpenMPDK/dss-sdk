#!/bin/sh

rpm_tmp=$1
rpm_spec_file="$rpm_tmp/SPECS/total.spec"

generateSpecFile()
{

rm -f "$rpm_spec_file"

cat > "$rpm_spec_file" <<LAB_SPEC

%global etcd_system_name etcd
####### NKV Package ###########
Name:		nkv-target
Version:        $Target_ver
Release:        1%{?dist}
Summary:        OSS Target Release
License:        GPLv3+
Vendor:         MSL-SSD

#%define dflypath  /usr/dss/nkv-target
Prefix: /usr/dss/nkv-target

%description
An OSS Target binary providing all features related NVMeoF Target and more.

# Common install for all below subpackages
%install
rm -rf %{buildroot}
cp -a %{name} %{buildroot}
exit 0

# File section for NKV-Target
%files
%defattr(-,root,root,-)
%dir /usr/dss/nkv-target
/usr/dss/nkv-target/bin/nvmf_tgt
/usr/dss/nkv-target/bin/dss_target.py
/usr/dss/nkv-target/bin/ustat
/usr/dss/nkv-target/scripts/setup.sh
/usr/dss/nkv-target/scripts/common.sh
/usr/dss/nkv-target/include/spdk/pci_ids.h
/usr/dss/nkv-target/lib/libdssd.a
/usr/dss/nkv-target/lib/liboss.a

%post
chmod +x /usr/dss/nkv-target/scripts/setup.sh
chmod +x /usr/dss/nkv-target/scripts/common.sh

############ Agent Package ############
%package -n FM-Agent
Version:        $FM_Agent_ver
Release:        1%{?dist}
Summary:        Fabric Manager Agent

#%define dflypath  /usr/dss/nkv-target

%description -n FM-Agent
This contains the Fabric Manager Agent component. The agent module will update
all Target related information to etcd database for effective cluster operation by
Fabric manager. FM-Base RPM need to be installed prior to this.

%files -n FM-Agent
%defattr(-,root,root,-)
/usr/nkv_agent/*
/etc/systemd/system/kv_cli.service
/etc/systemd/system/nvmf_tgt.service
/etc/systemd/system/nvmf_tgt@.service
/etc/rsyslog.d/dfly.conf

%post -n FM-Agent
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
if [ \$1 == 1 ]; then
	# Fresh installation
	systemctl enable etcd-gateway
	systemctl enable kv_cli
	systemctl enable nvmf_tgt
	systemctl enable nvmf_tgt@internal_flag.service
	check_service etcd-gateway start install
	check_service kv_cli start install
	check_service nvmf_tgt start install
	check_service nvmf_tgt@internal_flag start install
	check_service rsyslog restart install
else
	# Upgrade stage
	check_service etcd-gateway restart upgrade
	check_service kv_cli restart upgrade
	check_service nvmf_tgt restart upgrade
	check_service rsyslog restart upgrade
fi
} &> /tmp/fm-agent-output

%postun -n FM-Agent
if [ \$1 == 0 ]; then
	systemctl stop nvmf_tgt
	systemctl stop kv_cli
	systemctl stop etcd-gateway
	rm -rf /usr/nkv_agent
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
		if ! eval "rpmbuild -bb --clean $rpm_spec_file --define '_topdir $rpm_tmp'"
		then
			echo "Failed to build RPM"
			exit
		fi
		echo "RPM build success"
}

usage()
{
cat <<LAB_USAGE
$(basename "$0")

usage:
    genrpm.sh <buildroot> <Target_ver> <FM_Agent_ver>
    -h    Show this help

LAB_USAGE
}


Target_ver=$2
FM_Agent_ver=$3

mkdir -p "${rpm_tmp}"
for dir in BUILDROOT RPMS/x86_64 SRPMS SPECS; do
    mkdir -p "${rpm_tmp}"/$dir
done

generateSpecFile 

generateRPM
exit 0
