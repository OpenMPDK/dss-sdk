#!/usr/bin/env bash
set -e

WORKING_DIR="$(cd "$(dirname "$0")" && pwd)"
SCRIPT_VERSION="1.00.03.00"


die()
{
    echo "$*"
    exit 1
}

usage()
{
cat <<LAB_USAGE
$(basename "$0")  v$SCRIPT_VERSION

usage:
    -p    Generate deb and rpm packages
    -r    Convert deb package to rpm package
    -J    Add Jenkins buildnumber

    -h    Show this help

LAB_USAGE
}

fpm_install_msg()
{
cat <<LAB01_FPM_INSTALL_MSG


 To install fpm do following:

     sudo apt-get update

     sudo apt-get install ruby-dev build-essential

     sudo gem install fpm

     sudo apt-get install rpm


LAB01_FPM_INSTALL_MSG
}

removePackage()
{
    local package_name=$1

    for f in "${package_name}"*.deb
    do
        echo "rm -f $f"
        if [ -f "$f" ]
        then
            rm "$f"
        fi
    done

    for f in "${package_name}"*.rpm
    do
        echo "rm -f $f"
        if [ -f "$f" ]
        then
            rm "$f"
        fi
    done
}

buildPackage()
{
    local jenkingJobnumber=0
    if [[ $# -ne 0 ]]
    then
        jenkingJobnumber=$1
    fi

    if [[ ! -e DEBIAN/control ]]
    then
        die "ERR: DEBIAN/control file does not exist!"
    fi

    gittag=${jenkingJobnumber}.$(git log -1 --format=%h)
    package_revision=1

    package_name=$(< DEBIAN/control sed 's/ //g' | awk -F: '/^Package/ {print $2}' | sed 's/ //g')
    sw_version=$(< DEBIAN/control sed 's/ //g' | awk -F: '/^Version/ {printf("%s\n", $2)}' | sed 's/ //g')

    full_package_name=$(echo "${package_name}"_"${sw_version}"."${gittag}"-"${package_revision}" | sed 's/ //g')

    dir_share=${full_package_name}/usr/share/${package_name}

    # Remove any pre-existin package in directory
    removePackage "${package_name}"

    mkdir -p "${full_package_name}"/DEBIAN
    cp DEBIAN/* "${full_package_name}"/DEBIAN

    # Append git-it to version number
    < DEBIAN/control awk -F: -vTAG="${gittag}" ' /^Version/ {printf("Version:%s.%s\n", $2, TAG) }
                           !/^Version/ {print $0}
    ' > "${full_package_name}"/DEBIAN/control

    # Copy code to working directory that should be
    # include in the deb install package
    mkdir -p "${dir_share}"
    cp ./*.py         "${dir_share}"/
    cp -aR backend/   "${dir_share}"/
    cp -aR common/    "${dir_share}"/
    cp -aR tools/     "${dir_share}"/
    cp -aR rest_api/  "${dir_share}"/
    cp -aR systems/   "${dir_share}"/
    cp -aR templates/ "${dir_share}"/
    cp ufm.yaml       "${dir_share}"/
    cp ../requirements.txt "${dir_share}"/
    cp ../LICENSE.md "${dir_share}"/

    mkdir -p "${dir_share}"/systemd
    cp -a systemd_files/* "${dir_share}"/systemd/

    # Create package
    dpkg-deb --build "${full_package_name}"

    # remove working directory (clean up)
    rm -rf "${full_package_name}"
}

convertDebToRpmPackage()
{
    # Convert a deb package to rpm
    for deb_filename in *.deb
    do
       if ! fpm -f -s deb -t rpm "$deb_filename";
       then
           die "ERR: Failed to convert deb pkg to rpm"
       fi
    done
}

jenkinsJobNo="0"
buildPackageFlag=0
convertDeb2RPM=0
while getopts "prJ:hu" opt
do
    case $opt in
        p)
            buildPackageFlag=1
            ;;
        r)
            convertDeb2RPM=1
            ;;
        J)
            jenkinsJobNo="${OPTARG}"
            ;;
        h|u|?)
            usage
            exit 1
            ;;
    esac
done

pushd "${WORKING_DIR}"

if [[ $# -eq 0 ]]
then
    usage
    die "ERR: No argument passed in"
fi

if ! command -v rpmbuild > /dev/null;
then
    die "ERR: rpmbuild is not installed"
fi

# Check is fpm package installer is installed
if ! command -v fpm > /dev/null;
then
    fpm_install_msg
    exit 2
fi

if [[ ${buildPackageFlag} -ne 0 ]]
then
    buildPackage "${jenkinsJobNo}"
fi

if [[ ${convertDeb2RPM} -ne 0 ]]
then
    convertDebToRpmPackage
fi

popd
