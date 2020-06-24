import json
import time

from redfish.rest.v1 import redfish_client

from systems.essd import essd_constants
from systems.essd.essd_subsystem_builder import EssdSubsystemBuilder
from systems.essd.essd_utils import EssdUtils


class EssdDrive:
    def __init__(self, url, username=None, password=None, log=None):
        self.url=url
        self.username=username
        self.password=password
        self.log=log
        self.essdPrefix=essd_constants.ESSD_KEY

        self.root = redfish_client(base_url=(self.url),
                                   timeout=1,
                                   max_retry=1)

        self.uuid = self.root.root['UUID']
        self.essd_util = EssdUtils(self.uuid, self.log)

    def __del__(self):
        if hasattr(self, 'root'):
            self.root.logout()

    def get(self, request):
        response = self.root.get(request, headers=None)
        if response.status != 200:
           return response.status, request, {}

        value = json.dumps(response.dict, indent=4, sort_keys=True)
        return response.status, request, json.loads(value)

    def updateUuid(self, db):
        if db == None:
            return

        tmpString=''
        lastUpdateKey = essd_constants.ESSD_UPTIME_KEY
        try:
            tmpString, _ = db.get(lastUpdateKey)
        except:
            pass

        uuids = dict()
        if tmpString:
            uuids = json.loads(tmpString.decode('utf-8'))

        new_uuids = dict()
        for u in uuids:
            new_uuids[u] = uuids[u]

        # Update current uptime
        new_uuids[self.uuid] = int(time.time())
        jsonString = json.dumps(new_uuids, indent=4, sort_keys=True)

        db.put(lastUpdateKey, jsonString)

    @staticmethod
    def removeUuidOlderThan(self, db, sec):
        if not db:
            return

        tmpString=''
        lastUpdateKey = essd_constants.ESSD_UPTIME_KEY
        try:
            tmpString, _ = db.get(lastUpdateKey)
        except:
            return

        uuids = dict()
        if tmpString:
            uuids = json.loads(tmpString.decode('utf-8'))

        allUptimes = uuids.values()
        latestUptime = max(allUptimes)

        remove_uuids = dict()
        keep_uuids = dict()
        for u in uuids:
            uptime=uuids[u]
            if uptime + sec < latestUptime:
                remove_uuids[u] = uuids[u]
            else:
                keep_uuids[u] = uuids[u]

        # Update the update list
        jsonString = json.dumps(keep_uuids, indent=4, sort_keys=True)
        db.put(lastUpdateKey, jsonString)

        if not bool(remove_uuids):
            return

        for u in remove_uuids:
            # The subsystem must be removed first as it needs to look at the
            # essd entry during removal
            EssdSubsystemBuilder.delete_subsystem(u)
            # Delete the ESSD
            # TODO: Shouldn't this be db.delete_prefix()?
            db.delete(essd_constants.ESSD_KEY + "/" + u)
            # Remove the lookup entry
            db.delete(essd_constants.SYSTEMS_KEY + "/" + u)

    def readEssdSystemsData(self, db):
        status, key, jsonSystems = self.get(request="/redfish/v1/Systems")
        if status != 200:
            return

        self.essd_util.save(db, key, jsonSystems)
        count = jsonSystems['Members@odata.count']
        for i in range(count):
            update_subsystem = False
            member = jsonSystems['Members'][i]['@odata.id']
            status, key, jsonMembers = self.get(request=member)
            if status != 200:
                continue

            subsystem_builder = EssdSubsystemBuilder(self.uuid, self.log, db)
            if self.essd_util.save(db, key, jsonMembers):
                update_subsystem = True
            storage = jsonMembers['Storage']['@odata.id']
            status, key, jsonStorage = self.get(request=storage)
            if status != 200:
                continue

            subsystem_builder.add_storage_info(jsonStorage)
            if self.essd_util.save(db, key, jsonStorage):
                update_subsystem = True
            memberDrive = jsonStorage['Members']
            for drive in memberDrive:
                x = drive['@odata.id']
                status, key, jsonStorageMember = self.get(request=x)
                if status != 200:
                    continue

                self.essd_util.save(db, key, jsonStorageMember)
                drives = jsonStorageMember['Drives']
                for d in drives:
                    driveId = d['@odata.id']
                    status, key, jsonDrives = self.get(request=driveId)
                    if status != 200:
                        continue

                    if self.essd_util.save(db, key, jsonDrives):
                        update_subsystem = True
                    subsystem_builder.add_drive_info(jsonDrives)

            ethernetCollection = jsonMembers['EthernetInterfaces']['@odata.id']
            status, key, jsonEthernetCollection = self.get(request=ethernetCollection)
            if status != 200:
                continue
            subsystem_builder.add_eth_interface_info()
            if self.essd_util.save(db, key, jsonEthernetCollection):
                update_subsystem = True
            if update_subsystem:
                subsystem_builder.save_subsystem()

            interfaces = jsonEthernetCollection['Members']
            for interface in interfaces:
                ifc_url = interface['@odata.id']
                status, key, jsonEthernetInterface = self.get(request=ifc_url)
                if status != 200:
                    continue
                self.essd_util.save(db, key, jsonEthernetInterface)

    def readEssdData(self, db):
        # Reads data from essd drive and writes it to db
        key="/redfish/v1"
        jsonData = self.root.root

        self.essd_util.save(db, key, jsonData)
        for root in jsonData:
            if root == "Systems":
                self.readEssdSystemsData(db)

            if root == "Chassis":
                # TODO
                pass

            if root == "Managers":
                # TODO
                pass

            if root == "JsonSchemas":
                # TODO
                pass

    def checkAddLookupEntry(self, db):
        if not self.uuid:
            self.log.error(f'Unable to add lookup entry for {self.url}: UUID not defined')
            return

        uuid = str(self.uuid)

        essdSystemKey = self.essd_util.build_key(
            [self.essdPrefix, uuid, essd_constants.ESSD_SYSTEM_SFX]
        )
        self.essd_util.add_lookup_entry(db, essdSystemKey)
