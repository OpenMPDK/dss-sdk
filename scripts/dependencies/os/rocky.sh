#! /usr/bin/env bash
set -e

# Build Dependencies
install_build_deps() {
    
    # Install dnf and epel-release - must be installed first
    BUILD_DEPS=('dnf')
    BUILD_DEPS+=('epel-release')
    package_manager 'install'

    # Install / Enable CRB (PowerTools)
    BUILD_DEPS=("'dnf-command(config-manager)'")
    package_manager 'install'
    crb enable

    # Install and enable mariadb-devel module on Rocky8 only (needed for Judy-devel)
    if [[ $ROCKY_SUPPORT_PRODUCT == "Rocky-Linux-8" ]]
    then
        BUILD_DEPS=('mariadb-devel')
        package_manager 'install'
        package_manager 'module enable'
    fi

    # Build dependencies
    BUILD_DEPS=('bc')
    BUILD_DEPS+=('bison')
    BUILD_DEPS+=('boost-devel')
    BUILD_DEPS+=('check')
    BUILD_DEPS+=('cmake')
    BUILD_DEPS+=('cmake3')
    BUILD_DEPS+=('cppunit-devel')
    BUILD_DEPS+=('CUnit-devel')
    BUILD_DEPS+=('dejagnu')
    BUILD_DEPS+=('dpkg')
    BUILD_DEPS+=('elfutils-libelf-devel')
    BUILD_DEPS+=('flex')
    BUILD_DEPS+=('expect')
    BUILD_DEPS+=('gcc-c++')
    BUILD_DEPS+=('git')
    BUILD_DEPS+=('glibc-devel')
    BUILD_DEPS+=('jemalloc-devel')
    BUILD_DEPS+=('Judy-devel')
    BUILD_DEPS+=('kernel-devel')
    BUILD_DEPS+=('libaio-devel')
    BUILD_DEPS+=('libcurl-devel')
    BUILD_DEPS+=('libuuid-devel')
    BUILD_DEPS+=('meson')
    BUILD_DEPS+=('ncurses-devel')
    BUILD_DEPS+=('numactl-devel')
    BUILD_DEPS+=('openssl-devel')
    BUILD_DEPS+=('pulseaudio-libs-devel')
    BUILD_DEPS+=('python3')
    BUILD_DEPS+=('python3-devel')
    BUILD_DEPS+=('python3-pip')
    BUILD_DEPS+=('rdma-core-devel')
    BUILD_DEPS+=('rpm-build')
    BUILD_DEPS+=('snappy-devel')
    BUILD_DEPS+=('tbb-devel')
    BUILD_DEPS+=('wget')
    BUILD_DEPS+=('zlib-devel')

    package_manager 'install'
}

# Install from package mananger
package_manager() {
    OPERATION=$1
    # Detect package manager tool
    INSTALLER_BIN=""

    if [[ -f '/usr/bin/dnf' ]]
    then
        echo "using dnf"
        INSTALLER_BIN='dnf'
    elif [[ -f '/usr/bin/yum' ]]
    then
        echo "using yum"
        INSTALLER_BIN='yum'
    elif [[ -f '/usr/bin/microdnf' ]]
    then
        echo "using microdnf"
        INSTALLER_BIN='microdnf'
    else
        # Can't find an appropriate installer
        echo "can't find a valid installer"
        exit 1
    fi

    # Optimizations for Docker build
    if [[ $DOCKER ]]
    then
        if [[ $INSTALLER_BIN != 'yum' ]]
        then
            BUILD_DEPS+=('--nodocs')
        fi
        BUILD_DEPS+=('--noplugins')
        BUILD_DEPS+=('--setopt=install_weak_deps=0')
    fi

    INSTALL_STRING="$INSTALLER_BIN $OPERATION -y ${BUILD_DEPS[*]}"
    echo "executing command: $INSTALL_STRING"
    eval "$INSTALL_STRING"
}


install_build_deps

# Farther cleanup if Docker environment
if [[ -f /.dockerenv ]]
then
    CLEANUP_STRING="$INSTALLER_BIN clean all"
    echo "executing command: $CLEANUP_STRING"
    eval "$CLEANUP_STRING"
    rm -rf /var/lib/dnf/history* \
        /var/lib/yum/history* \
        /var/lib/dnf/repos/* \
        /var/lib/yum/repos/* \
        /var/lib/rpm/__db* \
        /usr/share/man \
        /usr/share/doc \
        /usr/share/licenses \
        /tmp/* \
        /var/log/dnf* \
        /var/log/yum*
fi
