#!/usr/bin/env bash
#
#
mkdir -p source/_static
mkdir -p source/_templates/

make clean

# build html
make html

# build pdf from html
# sphinx-build -b rinoh source/ buildpdf/rinoh

