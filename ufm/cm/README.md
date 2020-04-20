# Ansible Configuration Management for FabricManager

This Ansible playbook is used to deploy and configure the build dependencies for FabricManager.

## Assumptions

The target system must be Ubuntu 16.04, and have Ansible installed (>= 2.8)

Versions of software installed (compiled) from non-distro repositories are defined in roles/FabricManager/vars/main.yml

## Usage

ansible-playbook fabricmanager.yml
