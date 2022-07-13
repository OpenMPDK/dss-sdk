#!/usr/bin/env bash

for i in {0..34}
    do nvme format /dev/nvme"${i}"n1 --namespace-id=1 --ses=1
done
