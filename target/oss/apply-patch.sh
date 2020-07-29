#!/bin/bash

set -o xtrace

cd ./dssd
git apply ../dssd-stats.patch 
cd ../
