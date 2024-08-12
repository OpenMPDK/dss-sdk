#! /usr/bin/env bash
# shellcheck source=/dev/null
set -e

# Path variables
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

if [[ -e /etc/os-release ]]; then
	source /etc/os-release
else
	ID=unknown
fi

# Default paths in case they are not exported automatically
export PATH=$PATH:/usr/local/bin:/usr/local/sbin

for id in $ID $ID_LIKE; do
    if [[ -e $SCRIPT_DIR/os/$id.sh ]]; then
        echo "os: $id"
        source "$SCRIPT_DIR/os/$id.sh"
        source "$SCRIPT_DIR/os/common.sh"
        exit 0
    fi
done

printf "Non-supported distribution detected: %s\n" "$ID" >&2
echo "Aborting!" 
exit 1
