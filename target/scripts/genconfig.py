#!/usr/bin/env python

import os
import re
import sys

comment = re.compile(r'^\s*#')
assign = re.compile(r'^\s*([a-zA-Z_]+[a-zA-Z_0-9]*)\s*(\?)?=\s*([^#]*)')

args = os.environ.copy()
for arg in sys.argv:
    m = assign.match(arg)
    if m:
        var = m.group(1).strip()
        val = m.group(3).strip()
        args[var] = val

if "CONFIG_MODULE_PREFIX" in arg:
    config_module_prefix = arg["CONFIG_MODULE_PREFIX"]
else:
    config_module_prefix = "DFLY"

defs = {}
for config in ('CONFIG', 'CONFIG.local'):
    try:
        with open(config) as f:
            for line in f:
                line = line.strip()
                if not comment.match(line):
                    m = assign.match(line)
                    if m:
                        var = m.group(1).strip()
                        default = m.group(3).strip()
                        val = default
                        if var in args:
                            val = args[var]
                        if default.lower() == 'y' or default.lower() == 'n':
                            if val.lower() == 'y':
                                defs["{0}_{1}".format(config_module_prefix, var)] = 1
                            else:
                                defs["{0}_{1}".format(config_module_prefix, var)] = 0
                        else:
                            strval = val.replace('"', '\"')
                            defs["{0}_{1}".format(config_module_prefix, var)] = strval
    except IOError:
        continue

print ("#ifndef __{0}_CONFIG__".format(config_module_prefix))
print ("#define __{0}_CONFIG__".format(config_module_prefix))
print ("")

for key, value in defs.items():
    if value == 0:
        print ("#undef  {0}".format(key))
    else:
        print ("#define {0} {1}".format(key, value))

print ("")
print ("#endif")
