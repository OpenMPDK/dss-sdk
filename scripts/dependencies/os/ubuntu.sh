#! /usr/bin/env bash
set -e

export DEBIAN_FRONTEND=noninteractive
export TZ=Etc/UTC

# Build Dependencies
install_build_deps() {
    BUILD_DEPS=()
    package_manager 'update'

    # Build dependencies
    BUILD_DEPS+=('bc')
    BUILD_DEPS+=('bison')
    BUILD_DEPS+=('check')
    BUILD_DEPS+=('cmake')
    BUILD_DEPS+=('dejagnu')
    BUILD_DEPS+=('dpkg')
    BUILD_DEPS+=('expect')
    BUILD_DEPS+=('flex')
    BUILD_DEPS+=('g++')
    BUILD_DEPS+=('git')
    BUILD_DEPS+=('libaio-dev')
    BUILD_DEPS+=('libboost-dev')
    BUILD_DEPS+=('libboost-filesystem-dev')
    BUILD_DEPS+=('libc6-dev')
    BUILD_DEPS+=('libcppunit-dev')
    BUILD_DEPS+=('libcunit1-dev')
    BUILD_DEPS+=('libcurl4-openssl-dev')
    BUILD_DEPS+=('libelf-dev')
    BUILD_DEPS+=('libjemalloc-dev')
    BUILD_DEPS+=('libjudy-dev')
    BUILD_DEPS+=('libnuma-dev')
    BUILD_DEPS+=('libpulse-dev')
    BUILD_DEPS+=('libpython3-dev')
    BUILD_DEPS+=('librdmacm-dev')
    BUILD_DEPS+=('libsnappy-dev')
    BUILD_DEPS+=('libssl-dev')
    BUILD_DEPS+=('libtbb-dev')
    BUILD_DEPS+=('linux-headers-generic')
    BUILD_DEPS+=('meson')
    BUILD_DEPS+=('ncurses-dev')
    BUILD_DEPS+=('python3')
    BUILD_DEPS+=('python3-pip')
    BUILD_DEPS+=('uuid-dev')
    BUILD_DEPS+=('wget')
    BUILD_DEPS+=('zlib1g-dev')
    package_manager 'install'
}

# Install from package mananger
package_manager() {
    OPERATION=$1
    # Detect package manager tool
    INSTALLER_BIN=""

    if [[ -f '/usr/bin/apt-get' ]]
    then
        echo "using apt-get"
        INSTALLER_BIN='apt-get'
    else
        # Can't find an appropriate installer
        echo "can't find a valid installer"
        exit 1
    fi

    INSTALL_STRING="$INSTALLER_BIN $OPERATION -y ${BUILD_DEPS[*]}"
    echo "executing command: $INSTALL_STRING"
    eval "$INSTALL_STRING"
}

install_build_deps
