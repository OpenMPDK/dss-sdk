#!/usr/bin/env bash

for i in {0..35}
    do ./nvme_download.sh /dev/nvme"${i}"n1 "${1}"
done
