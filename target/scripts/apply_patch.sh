#!/bin/bash

warning="

Make sure you are in the root directory of source git
if you are not running the script as './scripts/apply_patches.sh'
    chances are this script might probably mess up something. ABORT NOW 

This script will reset the submodule project to base commit and
apply the patches availabe.

Make sure you do not have any required local changes for the submodule

"

usage="Usage:
    ./scripts/apply_patches.sh <project_name> [<patch_folder_name>] [<base>]"

proj_name=$1
base="oss"
patch_folder_name=$1

if [ $# -lt 1 ]; then
	echo ""
	echo "Need atleast one argument for project name."
	echo ""
	echo "$usage"
	echo ""
	exit 1
fi

if [ $# -gt 3 ]; then
	echo ""
	echo "Too Many arguments."
	echo ""
	echo "$usage"
	echo ""
	exit 1
fi

if [ $# -gt 1 ]; then
	patch_folder_name=$2
fi

if [ $#  -eq 3 ]; then
	base=$3
fi

echo "$warning"

found_modified_content=$(git status -uno | grep "${base}/${proj_name}" | cut -d ':' -f 2 | grep "modified content" | wc -l)
found_new_commits=$(git status -uno | grep "${base}/${proj_name}" | cut -d ':' -f 2 | grep "new commits" | wc -l)
if [ "$found_modified_content" -eq 1 ]; then
	echo "Exiting because modified content found in ${base}/${proj_name}"
	exit 1
elif [ "$found_modified_content" -gt 1 ]; then
	echo "Unexpectedly found multiple modified lines for ${base}/${proj_name}"
	exit 1
else
	if [ "$found_new_commits" -eq 1 ]; then
		echo "New commits found in ${base}/${proj_name}"

		read -e -p "Do you like to continue? [y/N]" choice
		[[ "$choice" == [Yy]* ]] || exit 0 #Exit for choicse other than Yy

	elif [ "$found_new_commits" -gt 1 ]; then
		echo "Unexpectedly found multiple lines for ${base}/${proj_name}"
		exit 1
	fi
fi

echo "Applying patches for ${base}/${proj_name} ..."

patch_folder=${base}/patches/${patch_folder_name}

commit_hash=$(cat "${patch_folder}"/base_commit)


pushd "${base}/${proj_name}"/
if [ -d ../../"${patch_folder}" ]; then
	git reset --hard "$commit_hash"
	git am ../../"${patch_folder}"/0*
else
	( echo "Patch folder not found ${patch_folder} .... Exiting "; exit 1 )
fi
popd

