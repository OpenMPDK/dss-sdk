# NKV SDK patch update process

This patch update process needs to be used for open source projects to keep track of changes to them.
The list of patches generated needs to be pushed to nkv-sdk repo.
These patches are applied during the pre build step by build.sh

## Steps to patch spdk

1. Change to spdk sub-module directory and commit changes that are required
```
cd target/oss/spdk_tcp
```
Below steps are just reference to add files and commit them
```
git add
git commit
```
This commit is shown inside the sub-module not in nkv-sdk repo

2. Run the update patch script at top level target directory
```
cd ../..
./scripts/update_patch.sh spdk_tcp spdk
```
Confirm with 'y' to regenerate the spdk patch list.

3. Add the generated patches to the changes to be commited list
```
git add oss/patches/spdk/*
```
4. Verify the changes to be committed.
```
git diff --cached
```
The new commits should be added as new files.
There will be update to other files for commit hashes/patch sequence number
There should not be any other unexpected changes

5. Commit the changes and verify build by re-applying the patches. Push the changes to gitlab after verification.
```
git commit
./build.sh
```
Confirm patch re-apply by typing 'y' when prompted
```
git push
```
## Steps to patch rocksdb

1. Change to rocksdb sub-module directory and commit changes that are required
```
cd target/oss/rocksdb
```
Below steps are just reference to add files and commit them
```
git add
git commit
```
This commit is shown inside the sub-module not in nkv-sdk repo

2. Run the update patch script at top level target directory
```
cd ../..
./scripts/update_patch.sh rocksdb
```
Confirm with 'y' to regenerate the rocksdb patch list.

3. Add the generated patches to the changes to be commited list
```
git add oss/patches/rocksdb/*
```
4. Verify the changes to be committed.
```
git diff --cached
```
The new commits should be added as new files.
There will be update to other files for commit hashes/patch sequence number
There should not be any other unexpected changes

5. Commit the changes and verify build by re-applying the patches. Push the changes to gitlab after verification.
```
git commit
./build.sh
```
Confirm patch re-apply by typing 'y' when prompted
```
git push
```
