#!/bin/bash
#
# Description:
#  This script is called by the gunicorn service file
#

[[ -x /usr/bin/gunicorn ]] && /usr/bin/gunicorn $@

[[ -x /usr/local/bin/gunicorn ]] && /usr/local/bin/gunicorn $@

