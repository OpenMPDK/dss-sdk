# nkv-sdk Ansible playbooks

## Requirements

* Ansible version 2.9 or later
* Target and UFM artifacts built (see scripts/README.md from project root)
* Cluster hosts populated in inventory file (hosts)
* Subsystem information populated in DFLY_CONFIG

## Deploy

* Deploy all (targets, haproxy, and UFM hosts): `ansible_playbook deploy_all.yml`
* Deploy targets only: `ansible_playbook deploy_targets.yml`
* Deploy haproxy only: `ansible_playbook deploy_haproxy.yml`
* Deploy UFM only: `ansible_playbook deploy_ufm.yml`

## Test

* Run tests to verify deployed cluster health: `ansible_playbook test.yml`

## Reset

* Reset all (targets, haproxy, and UFM hosts): `ansible_playbook reset_all.yml`
