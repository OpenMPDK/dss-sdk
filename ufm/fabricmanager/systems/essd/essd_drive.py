import sys
import redfish
import json
import time
from redfish.rest.v1 import redfish_client

from systems.essd import essd_constants

class EssdDrive():
    def __init__(self, url, username=None, password=None, log=None):
        self.url=url
        self.username=username
        self.password=password
        self.logger=log
        self.essdPrefix=essd_constants.ESSD_KEY

        self.root = redfish_client(base_url=(self.url), timeout=1, max_retry=1)
        self.uuid = self.root.root['UUID']


    def __del__(self):
        if hasattr(self, 'root'):
            self.root.logout()


    def get(self, request):
        response = self.root.get(request, headers=None)
        if response.status != 200:
           return response.status, request, {}

        value = json.dumps(response.dict, indent=4, sort_keys=True)
        return response.status, request, json.loads(value)


    def printAllMembers(self, key, jsonData):
        '''
          Prints main members in a jsom message
        '''
        print("-"*60)
        print("key={}".format(key))
        for x in jsonData:
            print("  {}: {}".format(x, jsonData[x]))
        print("-"*60)


    def removeDuplicates(self, string):
        result=[]
        lastChar=''
        for char in string:
            if char == '/' and char == lastChar:
                lastChar=char
                continue

            result.append(char)
            lastChar=char

        return ''.join(result)


    def buildKey(self, tags):
        if not tags:
            return None

        result = '/'.join(tags)
        return self.removeDuplicates(result)


    def saveKeyValue(self, db, keyWithPrefix, jsonData):
        if not db:
            return

        try:
            value, _ = db.get(keyWithPrefix)
            if value and value.decode('utf-8') == jsonData:
                self.logger.info("=========== NOT saving data ===========")
                return
        except Exception as e:
            self.logger.exception(e)
            pass

        db.put(keyWithPrefix, str(jsonData))

    def save(self, db, key, jsonData):
        # If uuid doesn't exist, no data can be saved to DB
        if not self.uuid:
            self.logger.error('Unable to save data without UUID')
            return

        keyWithPrefix = self.buildKey([self.essdPrefix, str(self.uuid), str(key)])
        if not keyWithPrefix:
            self.logger.error('Unable to build key to save ESSD data')
            return
        self.saveKeyValue(db, keyWithPrefix, jsonData)

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


    def removeUuidOlderThan(self, db, sec):
        if db == None:
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

        allUptimes = uuid.values()
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
            db.delete(essd_constants.ESSD_KEY + "/" + u)
            # Remove the lookup entry
            db.delete(essd_constants.SYSTEMS_KEY + "/" + u)


    def readEssdSystemsData(self, db):
        status, key, jsonSystems = self.get(request="/redfish/v1/Systems")
        if status != 200:
            return

        self.save(db, key, jsonSystems)
        count = jsonSystems['Members@odata.count']
        for i in range(count):
            member = jsonSystems['Members'][i]['@odata.id']
            status, key, jsonMembers = self.get(request=member)
            if status != 200:
                continue

            self.save(db, key, jsonMembers)
            storage = jsonMembers['SimpleStorage']['@odata.id']
            status, key, jsonStorage = self.get(request=storage)
            if status != 200:
                continue

            self.save(db, key, jsonStorage)
            memberDrive = jsonStorage['Members']
            for drive in memberDrive:
                x = drive['@odata.id']
                status, key, jsonStorageMember = self.get(request=x)
                if status != 200:
                    continue

                self.save(db, key, jsonStorageMember)
                drives = jsonStorageMember['Drives']
                for d in drives:
                    driveId = d['@odata.id']
                    status, key, jsonDrives = self.get(request=driveId)
                    if status != 200:
                        continue

                    self.save(db, key, jsonDrives)

            ethernetCollection = jsonMembers['EthernetInterfaces']['@odata.id']
            status, key, jsonEthernetCollection = self.get(request=ethernetCollection)
            if status != 200:
                continue
            self.save(db, key, jsonEthernetCollection)
            interfaces = jsonEthernetCollection['Members']
            for interface in interfaces:
                ifc_url = interface['@odata.id']
                status, key, jsonEthernetInterface = self.get(request=ifc_url)
                if status != 200:
                    continue
                self.save(db, key, jsonEthernetInterface)



    def readEssdData(self, db):
        # Reads data from essd drive and writes it to db
        key="/redfish/v1"
        jsonData = self.root.root

        self.save(db, key, jsonData)
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
            self.logger.warning(f'Unable to add lookup entry for {self.url}: UUID not defined')

        uuid = str(self.uuid)
        key = self.buildKey([essd_constants.SYSTEMS_KEY, uuid])
        essdSystemKey = self.buildKey([self.essdPrefix, uuid, 'redfish/v1/Systems/System.eSSD.1'])
        if not key:
            self.logger.error('Unable to build key to save ESSD data')
            return

        jsonData = {
            'type': 'essd',
            'key': essdSystemKey,
            'type_specific_data': {
                'suuid': uuid
            }
        }
        # Save Function will check if it already exists
        self.saveKeyValue(db, key, jsonData)

