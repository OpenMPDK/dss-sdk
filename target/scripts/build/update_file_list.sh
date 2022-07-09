#!/bin/bash

warning="Make sure you are in the root directory of source
if you are not running the script as './scripts/build/update_file_list.sh'
    chances are this script might probably mess up something. ABORT NOW "

usage="Usage:
    ./scripts/build/update_file_list.sh <project_name> [<base>]"

proj_name=$1
base="oss"

if [ $# -lt 1 ]; then
	echo ""
	echo "Need atleast one argument for project name."
	echo ""
	echo "$usage"
	echo ""
	exit 1
fi

if [ $# -gt 2 ]; then
	echo ""
	echo "Too Many arguments."
	echo ""
	echo "$usage"
	echo ""
	exit 1
fi

if [ $#  -eq 2 ]; then
	base=$2
fi

echo "$warning"
read -e -p "Do you like to continue? [y/N]" choice
[[ "$choice" == [Yy]* ]] || exit 0 #Exit for choicse other than Yy

echo "Updating ${base}/${proj_name} ..."

echo "#Auto Generated File do not modify" > mk/filelists/"${proj_name}"_filelist.txt

{
find "${base}"/"${proj_name}" -type f  | awk -F: '{printf "configure_file(../../%s ../../%s COPYONLY)\n",$0, $0}' | sort
echo "#Symbolic links"
echo "#TODO: Resolve this:configure_file does not support copy directory and copy symlink without dereference"
echo "ADD_CUSTOM_TARGET(copy_${proj_name}_links"
find "${base}"/"${proj_name}" -type l  | awk -F: '{printf "COMMAND cp -d ${CMAKE_CURRENT_SOURCE_DIR}/../../%s ${CMAKE_CURRENT_BINARY_DIR}/../../%s\n",$0, $0}' | sort
echo ")" 
} >> mk/filelists/"${proj_name}"_filelist.txt
