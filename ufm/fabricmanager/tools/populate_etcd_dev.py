#!/usr/bin/python3
# The Clear BSD License
#
# Copyright (c) 2022 Samsung Electronics Co., Ltd.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted (subject to the limitations in the disclaimer
# below) provided that the following conditions are met:
#
# * Redistributions of source code must retain the above copyright notice,
#   this list of conditions and the following disclaimer.
# * Redistributions in binary form must reproduce the above copyright notice,
#   this list of conditions and the following disclaimer in the documentation
#   and/or other materials provided with the distribution.
# * Neither the name of Samsung Electronics Co., Ltd. nor the names of its
#   contributors may be used to endorse or promote products derived from this
#   software without specific prior written permission.
# NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY
# THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
# CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT
# NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
# PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


#
# Description:
#   The purpose of this script is to populate DB for local development
#
import os
import os.path
import sys
import socket
import time

import argparse
import uuid
import datetime
import time
import etcd3
import logging

import yaml


logger = logging.getLogger("populate_db")
logger.setLevel(logging.DEBUG)

formatter = logging.Formatter('%(asctime)s:%(levelname)s:%(name)s:%(message)s')

fh = logging.FileHandler('populate_db.log')
fh.setLevel(logging.ERROR)
fh.setFormatter(formatter)
logger.addHandler(fh)

ch = logging.StreamHandler()
ch.setLevel(logging.DEBUG)
ch.setFormatter(formatter)
logger.addHandler(ch)


def printType(db, dbp, p, t):
    if type(t) is int or type(t) is str:
        dbp = dbp + "/" + p
        db.put(str(dbp), str(t))
        return

    if type(t) is list:
        for v in t:
            printType(db, dbp, p, v)
        return

    if type(t) is dict:
        dbp = dbp + "/" + p
        for k, v in t.items():
            printType(db, dbp, k, v)
        return


def read_data_from_file(db, filename):
    if not os.path.isfile(filename):
        logger.error("ERR: File {} doesn't exist".format(filename))
        sys.exit(-1)

    with open(filename) as fp:
        doc = yaml.load(fp)
        dbkey = ''
        for key, value in doc.items():
            printType(db, dbkey, key, value)


def main():
    parser = argparse.ArgumentParser(description='Configuration db for UFM.')
    parser.add_argument("--port", help="Port of Server", dest="port", default=2379)
    parser.add_argument("--ip_address", help="ip-address of the etcd node", dest="ip_address", default="0.0.0.0")
    parser.add_argument("--filename", help="YAML filename", dest="filename", default='cluster.yaml')

    args = parser.parse_args()

    logger.info("============> Connect to {} <=================".format(args.ip_address))
    if args.ip_address == "0.0.0.0":
        db = etcd3.client()
    else:
        db = etcd3.client(host=args.ip_address, port=args.port)

    read_data_from_file(db, args.filename)

    logger.info("Done")


if __name__ == '__main__':
    main()
