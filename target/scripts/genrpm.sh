#!/bin/sh

rpm_tmp=$1
rpm_spec_file="$rpm_tmp/SPECS/total.spec"

generateSpecFile()
{

rm -f "$rpm_spec_file"

cat > "$rpm_spec_file" <<LAB_SPEC

####### NKV Target Package ###########
Name:		nkv-target
Version:        $Target_ver
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
echo "rsyslog started ($?)"

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

echo "Done ($?)"
LAB_SPEC
}

generateRPM()
{
    if ! eval "rpmbuild -bb --clean $rpm_spec_file --define '_topdir $rpm_tmp'"
    then
        echo "Failed to build RPM"
        exit 1
    fi
    echo "RPM build success"
}

usage()
{
cat <<LAB_USAGE
$(basename "$0")

usage:
    genrpm.sh <buildroot> <Target_ver>
    -h    Show this help

LAB_USAGE
}


Target_ver=$2

mkdir -p "${rpm_tmp}"
for dir in BUILDROOT RPMS/x86_64 SRPMS SPECS
do
    mkdir -p "${rpm_tmp}"/$dir
done

generateSpecFile

generateRPM

exit 0
