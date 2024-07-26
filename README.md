# dss-sdk

dss-sdk is a sub-component of the larger project, [DSS](https://github.com/OpenMPDK/DSS).

## Major components

- The target component is located under the [target](target) directory.
- The host component is located under the [host](host) directory.

## Dependencies

### Supported operating systems

dss-sdk target and host can be built using one of the following:

- CentOS 7.8
- Rockylinux 8
- Rockylinux 9
- Ubuntu 22.04

### Install build dependencies

```bash
sudo ./scripts/dependencies/install.sh
```

### Build dss-sdk

Prior to building dss-sdk, ensure that you have checked-out submodules:

```bash
git clone https://github.com/OpenMPDK/dss-sdk
git submodule update --init --recursive
```

#### Build dss-sdk target

```bash
./scripts/build_target.sh
```

#### Build dss-sdk host

note: dss-sdk target must be built prior to building host.

```bash
./scripts/build_host.sh kdd-samsung-remote
```

#### Build dss-sdk all (target + host)

```bash
./scripts/build_all.sh kdd-samsung-remote
```

## Contributing

We welcome any contributions to dss-sdk. Please fork the repository and submit a pull request with your proposed changes. Before submitting a pull request, please make sure that your changes pass the existing unit tests and that new unit tests have been added to cover any new functionality.

## DSS-SDK Pytest Framework

Unit tests for testing DataMover utility and module code. Leverages pytest-cov to generate a code coverage report

## Pytest Requirements

This module requires the following modules:

pytest
pytest-mock
pytest-cov
pytest-gitcov

Also refer to requirements.txt file if you would like to install these packages with pip

## Run Pytest

You must be in the dss-sdk directory

Structure:
`python3 -m pytest <path to test folder or file> -v -rA --cov=<path to root folder of dss-sdk> --cov-report term --color=yes --disable-warnings`

Here are some examples run from the dss-ecosystem directory

Run all tests by specifying the test folder
`python3 -m pytest tests -v -rA --cov=. --cov-report term --color=yes --disable-warnings`

Run on a specific test file
`python3 -m pytest tests/test_dss_host.py -v -rA --cov=. --cov-report term --color=yes --disable-warnings`

Run on a specific test class
`python3 -m pytest tests/test_dss_host.py::TestDSSHost -v -rA --cov=. --cov-report term --color=yes --disable-warnings`

Run on a specific unittest
`python3 -m pytest tests/test_dss_host.py::TestDSSHost::test_is_ipv4 -v -rA --cov=. --cov-report term --color=yes --disable-warnings`
