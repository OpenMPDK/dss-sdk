#!/bin/sh

sudo /usr/local/sbin/nvme fw-download $1 -f $2
sudo /usr/local/sbin/nvme fw-activate $1 --slot=0 --action=1
