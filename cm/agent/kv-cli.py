#! /usr/bin/env python
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
