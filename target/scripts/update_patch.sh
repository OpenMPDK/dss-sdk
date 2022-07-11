#!/usr/bin/env bash
set -e

warning="Make sure you are in the root directory of source git
if you are not running the script as './scripts/update_patches.sh'
    chances are this script might probably mess up something. ABORT NOW 

This script will delete all current pataches for the submodule project and
recreate them from the commit hash in base_commmit file.

Make sure all the changes needed in the sub-module are already commited

Make sure to add the new patch files and commit to master repo"

usage="Usage:
    ./scripts/update_patches.sh <project_name> [<patch_folder_name>] [<base>]"

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
read -r -e -p "Do you like to continue? [y/N]" choice
[[ "$choice" == [Yy]* ]] || exit 0 #Exit for choicse other than Yy

echo "Updating ${base}/${proj_name} ..."

patch_folder=${base}/patches/${patch_folder_name}

mkdir -p "${patch_folder}"

echo "Removing following patch files"
ls "${patch_folder}"/[0-9]*
rm "${patch_folder}"/[0-9]*

commit_hash=$(cat "${patch_folder}"/base_commit)
echo "Creating patches from base commit" "${commit_hash}"

pushd "${base}"/"${proj_name}"/
git am --abort
git format-patch -o ../../"${patch_folder}" "${commit_hash}"
popd

declare -A new_files=()

mapfile -t del_file_names < <(git ls-files -d "${patch_folder}")
mapfile -t new_file_names < <(git ls-files -o "${patch_folder}")

for name in "${new_file_names[@]}"; do
    idx_str=$(echo "$name" | cut -d- -f 1)
    new_files[$idx_str]=$name
done

echo "Restoring deleted files"
for name in "${del_file_names[@]}"; do
    idx_str=$(echo "$name" | cut -d- -f 1)
    echo "${new_files[$idx_str]} --> $name"
    mv "${new_files[$idx_str]}" "$name"
done
