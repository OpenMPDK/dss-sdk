#!/bin/bash

set -o xtrace


cd ./spdk_tcp
git apply ../spdk-kv.patch 
cd ../dssd
git apply ../dssd-stats.patch 
cd ../
