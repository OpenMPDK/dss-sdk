#!/bin/bash

var="$(pwd)"
echo "Current working dir is $var"

rm $var/release/*.deb
rm $var/release/*.rpm

cd $var/../ufm/fabricmanager
./makeufmpackage.sh -pr
mv $var/../ufm/fabricmanager/*.deb $var/release
mv $var/../ufm/fabricmanager/*.rpm $var/release

cd $var/../ufm/ufm_msg_broker
./makebrokerpackage.sh -pr
mv $var/../ufm/ufm_msg_broker/*.deb $var/release
mv $var/../ufm/ufm_msg_broker/*.rpm $var/release

ls -al $var/release
