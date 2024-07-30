#! /usr/bin/env bash
set -e

# Path variables
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
REQUIREMENTS=$(realpath "$SCRIPT_DIR/../python/requirements.txt")

# Upgrade python3 pip to latest
python3 -m pip install pip --upgrade

# Install python modules from requirements.txt
PIP_ARGS=()
PIP_ARGS+=("-r")
PIP_ARGS+=("$REQUIREMENTS")

# Optimizations for Docker build
if [[ $DOCKER ]]
then
    PIP_ARGS+=("--no-cache-dir")
fi

# Install python modules from requirements.txt via pip
INSTALL_STRING="python3 -m pip install ${PIP_ARGS[*]}"
echo "executing command: $INSTALL_STRING"
eval "$INSTALL_STRING"

# Set git config if not already set
for CONFIG in name email
do
    if git config --list | grep "user.$CONFIG"
    then
        echo "git user.$CONFIG is configured."
    else
        echo "WARNING: git user.$CONFIG is not configured. Setting a temporary user.$CONFIG."
        echo "You should set a proper git "user.$CONFIG" with command: git config --global user.$CONFIG <<your-details>>"
        git config --global user.$CONFIG "builder@msl.lab"
    fi
done

# Set git safe.directory globally if docker
if [[ $DOCKER ]]
then
    git config --global --add safe.directory '*'
fi
