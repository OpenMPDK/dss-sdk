import json
import uuid
from os.path import join

from rest_api.redfish import redfish_constants
from systems.essd import essd_constants
from systems.essd.essd_utils import EssdUtils

TBD = 'TBD'


class EssdSubsystemBuilder:
    def __init__(self, sys_uuid=None, log=None, db=None):
        if not log:
            raise ValueError
        self.log = log

        if not sys_uuid:
            self.log.error('No System UUID provided')
            raise ValueError

        if not db:
            self.log.error('No DB client instance provided')
            raise ValueError

        self.db = db
        # ConsumingComputerSystem
        self.system_uuid = sys_uuid
        self.system_link = None
        # Us
        self.subsystem_uuid = None
        self.subsystem_link = None
        # get_subsystem_base will update it if needed
        self.add_lookup_entries = False
        self.subsystem = self.get_subsystem_base()
        self.essd_util = EssdUtils(self.subsystem_uuid, self.log)

    def get_subsystem_base(self):
        # Update existing entry
        self.subsystem_uuid = self.get_subsystem_uuid()
        # Not found, generate a new entry
        if not self.subsystem_uuid:
            self.subsystem_uuid = str(uuid.uuid4())
            self.add_lookup_entries = True
        if not self.subsystem_uuid:
            self.log.error('Unable to generate a uuid for the subsystem')
            raise ValueError

        self.system_link = join(redfish_constants.SYSTEMS_URL,
                                self.system_uuid)
        self.subsystem_link = join(redfish_constants.SYSTEMS_URL,
                                   self.subsystem_uuid)

        base = {
            '@odata.context':
                '/redfish/v1/$metadata#ComputerSystem.ComputerSystem',
            redfish_constants.ODATA_ID: self.subsystem_link,
            '@odata.type': '#ComputerSystem.v1_9_0.ComputerSystem',
            'Description': 'System representing the drive resources',
            'Id': self.subsystem_uuid,
            'Name': 'System',
            redfish_constants.OEM: {
                redfish_constants.SERVERNAME: TBD,
                'NSID': 1,
                'NumaAligned': False
            },
            'Links': {
                'Links.ConsumingComputerSystems': [
                    {
                        redfish_constants.ODATA_ID: self.system_link
                    }
                ]
            }
        }
        return base

    def add_storage_info(self, storage):
        if not storage:
            self.log.error(f'No storage info provided to add to subsystem {self.subsystem_uuid}')
            return

        storage_link = join(self.subsystem_link, redfish_constants.STORAGE)
        self.subsystem[redfish_constants.STORAGE] = {
            redfish_constants.ODATA_ID: storage_link
        }

    def add_drive_info(self, drive):
        if not drive:
            return

        if redfish_constants.STATUS not in drive:
            self.log.debug(f"No Status found for drive {drive['Id']}")
            return

        # There aren't really going to be multiple drives ever but if there
        # are, show the health as bad if any of the drives is unhealthy.
        if redfish_constants.STATUS in self.subsystem:
            if redfish_constants.HEALTH in self.subsystem[redfish_constants.STATUS]:
                if self.subsystem[redfish_constants.STATUS][redfish_constants.HEALTH] != redfish_constants.STATUS_OK:
                    return

        self.subsystem[redfish_constants.STATUS] = drive[redfish_constants.STATUS]

        if redfish_constants.IDENTIFIERS in drive:
            self.subsystem[redfish_constants.IDENTIFIERS] = drive[redfish_constants.IDENTIFIERS]
            # Already set to a specific value from a previous drive
            if self.subsystem[redfish_constants.OEM][redfish_constants.SERVERNAME] != TBD:
                return
            for identifier in drive[redfish_constants.IDENTIFIERS]:
                if redfish_constants.DURABLENAME not in identifier:
                    continue
                durable_name = identifier[redfish_constants.DURABLENAME]
                if not durable_name:
                    continue
                # Example: nqn.2019-09.samsung:msl-ssg-tgt
                durable_name_tokens = durable_name.split(':')
                if len(durable_name_tokens) == 2:
                    server_name = durable_name_tokens[1]
                    self.subsystem[redfish_constants.OEM][redfish_constants.SERVERNAME] = server_name
                    break

    def add_eth_interface_info(self):
        eth_interfaces_link = join(self.subsystem_link,
                                   redfish_constants.ETH_INTERFACES)
        self.subsystem[redfish_constants.ETH_INTERFACES] = {
            redfish_constants.ODATA_ID: eth_interfaces_link
        }

    def save_subsystem(self):
        key = join(redfish_constants.SYSTEMS_URL, self.subsystem_uuid)
        self.essd_util.save(self.db, key, json.dumps(self.subsystem))
        if self.add_lookup_entries:
            # Add Cross reference entry in /essd/self.system_uuid/subsystems
            key = join(essd_constants.ESSD_KEY, self.system_uuid,
                       essd_constants.ESSD_SUBSYSTEM_KEY_STR)
            try:
                self.db.put(key, self.subsystem_uuid)
            except Exception as e:
                self.log.error(f'Caught exception {e}')

            # Add lookup entry in /Systems/self.subsystem_uuid
            self.add_subsystem_lookup_entry()

    def get_subsystem_uuid(self):
        key = join(essd_constants.ESSD_KEY, self.system_uuid,
                   essd_constants.ESSD_SUBSYSTEM_KEY_STR)

        return EssdUtils.get_value_from_db(key, self.log, self.db)

    def add_subsystem_lookup_entry(self):
        system_key = self.essd_util.build_key([essd_constants.ESSD_KEY,
                                               self.system_uuid,
                                               essd_constants.ESSD_SYSTEM_SFX])
        self.essd_util.add_lookup_entry(self.db, system_key, 'subsystem')

    @staticmethod
    def delete_subsystem(system_uuid=None, log=None, db=None):
        if not log:
            return

        if not system_uuid:
            log.error('No system UUID provided')
            return

        if not db:
            log.error(f'No db provided to delete subsystem for {system_uuid}')
            return

        key = join(essd_constants.ESSD_KEY,
                   system_uuid,
                   essd_constants.ESSD_SUBSYSTEM_KEY_STR)

        subsystem_uuid = EssdUtils.get_value_from_db(key, log, db)
        if not subsystem_uuid:
            log.warning(f'No subsystem found for system {system_uuid}')
            return

        log.debug(f'Deleting subsystem {subsystem_uuid}')

        # Delete the /essd/subsystem entry for this subsystem
        key = join(essd_constants.ESSD_KEY, subsystem_uuid)
        db.delete_prefix(key)
        # Delete the /Systems/subsystem lookup entry for this subsystem
        key = join(essd_constants.SYSTEMS_KEY, subsystem_uuid)
        db.delete(key)

    def print_subsystem(self):
        print(f'\n\nSubsystem for ESSD System: {self.system_uuid}')
        print(f'{json.dumps(self.subsystem, indent=2)}\n')
