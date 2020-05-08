#!/bin/sh

#rpm_tmp="/root/rpmbuild"
rpm_tmp=$1
rpm_spec_file="$rpm_tmp/SPECS/total.spec"

execute_command() {
    echo "$@"
    eval "$@"
    ret=$?
    [[ $ret -ne 0 ]] && echo "Failed: $@" && exit $ret
}


generateSpecFile()
{

rm -f $rpm_spec_file

cat > $rpm_spec_file <<LAB_SPEC

%global etcd_system_name etcd
####### NKV Package ###########
Name:		NKV
Version:        $Target_ver
Release:        1%{?dist}
Summary:        OSS Target Release
License:        GPLv3+
Vendor:         MSL-SSD

#%define dflypath  /usr/dragonfly
Prefix: /usr/dragonfly

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
%dir /usr/dragonfly
/usr/dragonfly/nvmf_tgt
/usr/dragonfly/scripts/setup.sh
/usr/dragonfly/scripts/common.sh
/usr/dragonfly/ustat
/usr/dragonfly/scripts/nkv_tgt_conf.py
%post
chmod +x /usr/dragonfly/scripts/setup.sh
chmod +x /usr/dragonfly/scripts/common.sh

############ Base Package ############
%package -n FM-Base
Version:        $FM_Base_ver
Release:        1%{?dist}
Summary:        Fabric Manager Dependencies

#%define dflypath  /usr/dragonfly

%description -n FM-Base
All Fabric Manager dependencies including virtual environment
are included in this package. To be installed before Agent, Rest/Monitor.

%files -n FM-Base
%defattr(-,root,root,-)
%dir /usr/dragonfly
%dir /usr/dragonfly/cm
%dir /usr/dragonfly/cm/clusterlib
%dir /usr/dragonfly/cm/etcdlib
%dir /usr/dragonfly/cm/events
%dir /usr/dragonfly/cm/logger
/usr/dragonfly/cm/clusterlib/*
/usr/dragonfly/cm/etcdlib/*
/usr/dragonfly/cm/events/*
/usr/dragonfly/cm/logger/*
/usr/dragonfly/cm/venv_centos_7.tgz

%post -n FM-Base
set -x 
{
rm -rf /usr/dragonfly/venv
tar -xf /usr/dragonfly/cm/venv_centos_7.tgz -C /usr/dragonfly/

} &> /tmp/fm-base-output

%postun -n FM-Base
# Remove the venv directory only during uninstall.
if [ \$1 == 0 ]; then
	rm -rf /usr/dragonfly/venv
	rm -rf /usr/dragonfly/cm/logger
	rm -rf /usr/dragonfly/cm/events
	rm -rf /usr/dragonfly/cm/etcdlib
	rm -rf /usr/dragonfly/cm/clusterlib
fi

############ Agent Package ############
%package -n FM-Agent
Version:        $FM_Agent_ver
Release:        1%{?dist}
Requires:       FM-Base
Summary:        Fabric Manager Agent

#%define dflypath  /usr/dragonfly

%description -n FM-Agent
This contains the Fabric Manager Agent component. The agent module will update
all Target related information to etcd database for effective cluster operation by
Fabric manager. FM-Base RPM need to be installed prior to this.

%files -n FM-Agent
%defattr(-,root,root,-)
%dir /usr/dragonfly
%dir /usr/dragonfly/cm
%dir /usr/dragonfly/cm/agent
%dir /usr/nkv_agent
%dir /etc/systemd/system
%dir /etc/rsyslog.d
/usr/dragonfly/cm/agent/*
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
/usr/dragonfly/
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
	rm -rf /usr/dragonfy/cm/agent
	rm -rf /usr/nkv_agent
	rm -f /etc/rsyslog.d/dfly.conf
	rm -f /etc/ld.so.conf.d/kvlibs.conf
	/usr/sbin/ldconfig
fi
%systemd_postun_with_restart rsyslog.service
systemctl daemon-reload

############## ETCD Package #################
%package -n etcd
Version:        $ETCD_ver
Release:        1%{?dist}
Summary:        A highly-available key value store for shared configuration
License:        ASL 2.0
Source1:        %{etcd_system_name}.conf

Requires(post): systemd
Requires(preun): systemd
Requires(postun): systemd

%description -n etcd
A highly-available key value store for shared configuration.

%post -n etcd
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

    if [ \$1 == 1 ]; then
        systemctl enable etcd
        check_service etcd start install
    fi
} &> /tmp/etcd-service-output

%postun -n etcd
if [ \$1 == 0 ]; then
	systemctl stop etcd
fi

%systemd_postun %{etcd_system_name}.service

%clean -n etcd
echo "Not cleaning up in this case."

%files -n etcd
%{_bindir}/%{etcd_system_name}
%{_bindir}/%{etcd_system_name}ctl
%dir %attr(-,etcd,etcd) %{_localstatedir}/lib/etcd



LAB_SPEC
}

generateRPM()
{
    #[[ -e ${rpm_tmp} ]] && rm -rf ${rpm_tmp}
    execute_command "rpmbuild -bb --clean $rpm_spec_file --define '_topdir ${rpm_tmp}'"
}

usage()
{
cat <<LAB_USAGE
$(basename "$0")

usage:
    genrpm.sh <buildroot> <Target_ver> <FM_Base_ver> <FM_Agent_ver> <FM_Rest_ver> <ETCD_ver>
    -h    Show this help

LAB_USAGE
}

if [ ! -f ${rpm_tmp}/BUILD/NKV/usr/bin/etcd ]; then
	echo "etcd binary not found for version"
	exit 1
fi
ETCD_ver=$(${rpm_tmp}/BUILD/NKV/usr/bin/etcd --version | grep etcd|awk '{print $3}')

if [ "$#" -ne 5 ]; then
  usage
  exit 1
fi

Target_ver=$2
FM_Base_ver=$3
FM_Agent_ver=$4
FM_Rest_ver=$5

mkdir -p ${rpm_tmp}
for dir in BUILDROOT RPMS/x86_64 SRPMS SPECS; do
    mkdir -p ${rpm_tmp}/$dir
done

generateSpecFile 

generateRPM
exit 0
