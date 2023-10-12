# DSS-Formatter
SPDK format application

## Introduction
The goal of this application is to write super block specific for target.

## Setup
```
* Before trying to run the application following things need to be done:
1. Generating a `spdk_conf` for the application
    1.a. This process tells spdk application about pcie devices on machine
        - run `nvme list` and `lspci | grep NVM` to select the drive of choice
        - Use pcie slot address in building the file
        - Example `spdk_conf` file would look like below:
        [Nvme]
          TransportID "trtype:PCIe traddr:0000:1d:00.0" "Nvme1"
        - Here the fields are indented by two spaces
        - `traddr` is procured from `lspci | grep NVM` command
        - The last field is arbitrary name provided to the device for app use
    1.b. Once generated place this file with the same name `spdk_conf` at  
         location of binary `dss_formatter`. This is mandatory.

2. Generating `conf-file` for the application
    2.a. This is used by dss-formatter stack to generate super-block
        - This is a tab separated file.
        - First field indicates the name of the device, this is the same name
          used in `spdk_conf` file for device prepended with `n1`
        - Second field represents the user defined logical block size
        - Third field represents the number of block states
        - Fourth field is the name of the block allocator type
        - Fifth field indicates whether the application needs to be run in debug mode
          - If run in debug mode, the application will try reading and printing the
            written super-block
        - Example `<conf-file>` file would look like below:
        Nvme1n1<\TAB>4096<\TAB>6<\TAB>block_impresario<\TAB>true
    2.b. This can be placed anywhere however path provided must be accessible.

3. Moving devices from kernel space to user space for application use
    3.a. Once `spdk_conf` and `conf-file` are generated we can move devices to
         user space using following command:
        - `./nkv-target/scripts/setup.sh`
        - This command needs to be run from `df_out` directory
        - This command is available after issuing `build.sh` from target directory
    3.b. To move the drives back into kernel space run the following command
        - `./nkv-target/scripts/setup.sh reset`
```

## Usage
```
* ./dss_formatter <conf-file> <parser-type>
- Only `TEXT` parser type is supported for now
```
