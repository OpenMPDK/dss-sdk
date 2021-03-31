#! /usr/bin/env python
#####################################################################################
#   BSD LICENSE
#
#   Copyright (c) 2021 Samsung Electronics Co., Ltd.
#   All rights reserved.#
#
#   Redistribution and use in source and binary forms, with or without
#   modification, are permitted provided that the following conditions
#   are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in
#       the documentation and/or other materials provided with the
#       distribution.
#     * Neither the name of Samsung Electronics Co., Ltd. nor the names of
#       its contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
#   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
#   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
#   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
#   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
#   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
#   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
#   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
#   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
#   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#####################################################################################

"""
KV Storage Management CLI.

Usage: kv-cli [--interactive] [-h|--help] [--version]
                  <command> [<args>...]

Options:
  -h --help          Show this screen.
  --version          Show version.

Subcommands:
  cpu                Display local CPU attributes.
  network            Display local network interfaces' attributes.
  nvme               Display local NVMe devices' attributes.
  daemon             Start local target daemon.
  kv                 KV Storage management.
  log                Log management. (TBD)
  security           Security management. (TBD)
  alert              Alert management. (TBD)
"""
import sys
from subprocess import call

from docopt import docopt

CLI_NAME = __file__.split('/')[-1].rstrip('.py')
CLI_VERSION = '1.0'

if __name__ == '__main__':
    args = docopt(__doc__, version=CLI_NAME + ' ' + CLI_VERSION, options_first=True)
    args_meta = {"__CLI_NAME__": CLI_NAME,
                 "__CLI_VERSION__": CLI_VERSION, }

    argv = [args['<command>']] + args['<args>']
    if args['<command>'] == 'cpu':
        import subcommands.cpu.cpu as cpu
        cpu.main(docopt(cpu.__doc__, argv=argv))
    elif args['<command>'] == 'network':
        import subcommands.network.network as network
        network.main(docopt(network.__doc__, argv=argv))
    elif args['<command>'] == 'nvme':
        import subcommands.nvme.nvme as nvme
        nvme.main(docopt(nvme.__doc__, argv=argv))
    elif args['<command>'] == 'log':
        import subcommands.log.log as log
        log.main(docopt(log.__doc__, argv=argv))
    elif args['<command>'] == 'daemon':
        import subcommands.daemon.daemon as daemon
        daemon.main(args_meta, docopt(daemon.__doc__, argv=argv))
    elif args['<command>'] == 'kv':
        import subcommands.kv.kv as kv
        kv.main(docopt(kv.__doc__, argv=argv))
    elif args['<command>'] in ['help', None]:
        sys.exit(call(['python', CLI_NAME + '.py', '--help']))
    else:
        sys.exit("%r is not a %s command. See '%s help'." % (args['<command>'], CLI_NAME, CLI_NAME))
