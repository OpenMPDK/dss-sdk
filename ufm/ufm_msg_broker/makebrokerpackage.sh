#!/bin/bash
#
#
#
WORKING_DIR="$(cd $(dirname $0) && pwd)"
SCRIPT_VERSION="1.00.02.00"


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


LAB01_FPM_INSTALL_MSG
}

removePackage()
{
    local package_name=$1

    for f in ${package_name}*.deb
    do
        [[ -e ${f} ]] && rm ${f}
    done

    for f in ${package_name}*.rpm
    do
        [[ -e ${f} ]] && rm ${f}
    done
}

buildPackage()
{
    local jenkingJobnumber=0
    if [[ $# -ne 0 ]]
    then
        jenkingJobnumber=$1
    fi

    [[ ! -e DEBIAN/control ]] && die "ERR: DEBIAN/control file does not exist!"

    gittag=${jenkingJobnumber}.$(git log -1 --format=%h)
    package_revision=1

    package_name=$(cat DEBIAN/control | sed 's/ //g' | awk -F: '/^Package/ {print $2}' | sed 's/ //g'  )
    sw_version=$(cat DEBIAN/control | sed 's/ //g' | awk -F: '/^Version/ {printf("%s\n", $2)}' | sed 's/ //g')

    full_package_name=$(echo ${package_name}_${sw_version}.${gittag}-${package_revision} | sed 's/ //g')

    dir_share=${full_package_name}/usr/share/${package_name}

    # Remove any pre-existin package in directory
    removePackage ${package_name}

    mkdir -p ${full_package_name}/DEBIAN
    cp DEBIAN/* ${full_package_name}/DEBIAN

    # Append git-it to version number
    cat DEBIAN/control | awk -F: -vTAG=$gittag ' /^Version/ {printf("Version:%s.%s\n", $2, TAG) }
                           !/^Version/ {print $0}
    ' > ${full_package_name}/DEBIAN/control

    # Copy code to working directory that should be
    # include in the deb install package
    mkdir -p ${dir_share}
    cp -a ufm_msg_broker.py ${dir_share}/

    mkdir -p ${dir_share}/systemd
    cp -a systemd_files/* ${dir_share}/systemd/

    # Create package
    dpkg-deb --build ${full_package_name}

    # remove working directory (clean up)
    rm -rf ${full_package_name}
}

convertDebToRpmPackage()
{
    # Convert a deb package to rpm
    for deb_filename in *.deb
    do
       fpm -f -s deb -t rpm $deb_filename
       [[ $? -ne 0 ]] && die "ERR: Failed to convert deb pkg to rpm"
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

pushd $WORKING_DIR

if [[ $# -eq 0 ]]
then
  usage
  die "ERR: No argument passed in"
fi

command -v rpmbuild > /dev/null
[[ $? -ne 0 ]] && die "ERR: rpmbuild is not installed"

# Check is fpm package installer is installed
command -v fpm > /dev/null
if [[ $? -ne 0 ]]
then
  fpm_install_msg
  exit 2
fi

[[ ${buildPackageFlag} -ne 0 ]] && buildPackage ${jenkinsJobNo}
[[ ${convertDeb2RPM} -ne 0 ]] && convertDebToRpmPackage

popd

exit 0
