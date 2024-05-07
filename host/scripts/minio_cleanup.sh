#!/usr/bin/env bash
# shellcheck disable=SC2059
set -e

# Set paths
HOST_BIN_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
HOST_CONF_DIR="$(realpath "$HOST_BIN_DIR/../conf")"

# Default Values
PORT=1030
VAL_SIZE=2097152
OPERATION=2
HOSTNAME=$(hostname)

# Mandatory Values (no default value)
START_DRIVE="$1"
END_DRIVE="$2"
CONF_FILE="$3"

# Parse command line arguments
while getopts ":s:e:c:p:v:o" opt; do
  case $opt in
    s) START_DRIVE="$OPTARG";;
    e) END_DRIVE="$OPTARG";;
    c) CONF_FILE="$OPTARG";;
    p) PORT="$OPTARG";;
    v) VAL_SIZE="$OPTARG";;
    o) OPERATION="$OPTARG";;
    \?) echo "Invalid option: -$OPTARG" >&2;;
  esac
done

# Print help if manditory args not provided or not sudoer
if [[ -z "$START_DRIVE" || -z "$END_DRIVE" || -z "$CONF_FILE" || "$EUID" -ne 0 ]]
then
  echo "Cleanup DSS MinIO metadata"
  echo "Usage: $0"
  echo "*** NOTE: Must run as root or sudoer ***"
  echo ""
  echo "Options:"
  cols="%-35s%s\n"
  printf "$cols" '  -s START_DRIVE'                     "Starting index of mounted NVMeOF subsystem (inclusive)"
  printf "$cols" '  -e END_DRIVE'                       "Ending index of mounted NVMeOF subsystem (inclusive)"
  printf "$cols" '  -c CONF_FILE'                       "Absolute path to nkv_config.json configuration file."
  printf "$cols" ' '                                    "If only filename, file is assumed to be in $HOST_CONF_DIR"
  printf "$cols" "  -p PORT (Default: $PORT)"           "DSS Target port"
  printf "$cols" "  -v VAL_SIZE (Default: $VAL_SIZE)"   "KV Value size to delete"
  printf "$cols" "  -o OPERATION (Default: $OPERATION)" "KV operation to perform"
  exit 1
fi

# Determine path to conf file
if [[ "$CONF_FILE" =~ ^/ ]]
then
    echo "Absolute path to CONF_FILE provided."
else
    echo "CONF_FILE provided without absolute path. Assumed to be located in $HOST_CONF_DIR"
    CONF_FILE="$HOST_CONF_DIR/$CONF_FILE"
fi

# chdir to host bin dir - nkv_test_cli only works from its PWD
pushd "$HOST_BIN_DIR"
# Loop over each NVMeOF subsystem disk number
for i in $(eval echo "{$START_DRIVE..$END_DRIVE}")
do
    # Loop over each key to delete
    for KEY in 'format.json' 'kv-volumes' 'config/config.json/xl.json'
    do
        NKV_TEST_CLI_CMD="./nkv_test_cli -c $CONF_FILE -i $HOSTNAME -p $PORT -w meta/.minio.sys/$KEY -s /dev/nvme${i}n1 -o $OPERATION -v $VAL_SIZE"
        echo "*** Executing command: $NKV_TEST_CLI_CMD"
        eval "$NKV_TEST_CLI_CMD"
    done
done
popd
