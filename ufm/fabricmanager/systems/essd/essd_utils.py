import ast

from systems.essd import essd_constants


class EssdUtils:
    def __init__(self, uuid=None, log=None):
        self.uuid = uuid
        self.log = log
        self.essd_prefix = essd_constants.ESSD_KEY

    def print_all_members(self, key, json_data):
        '''
          Prints main members in a jsom message
        '''
        print("-"*60)
        print("key={}".format(key))
        for x in json_data:
            print("  {}: {}".format(x, json_data[x]))
        print("-"*60)

    def remove_duplicates(self, string):
        result = []
        last_char = ''
        for char in string:
            if char == '/' and char == last_char:
                last_char = char
                continue

            result.append(char)
            last_char = char

        return ''.join(result)

    def build_key(self, tags):
        if not tags:
            return None

        result = '/'.join(tags)
        return self.remove_duplicates(result)

    def save_key_value(self, db, keyWithPrefix, json_data):
        if not db:
            return False

        try:
            value, _ = db.get(keyWithPrefix)
            if value:
                value = ast.literal_eval(value.decode('utf-8'))
                if value == json_data:
                    self.log.info("=========== NOT saving data ===========")
                    return False
        except Exception as e:
            self.log.exception(e)
            pass

        db.put(keyWithPrefix, str(json_data))
        return True

    def save(self, db, key, json_data):
        # If uuid doesn't exist, no data can be saved to DB
        if not self.uuid:
            self.log.error('Unable to save data without UUID')
            return False

        key_with_prefix = self.build_key([self.essd_prefix, str(self.uuid),
                                          str(key)])
        if not key_with_prefix:
            self.log.error('Unable to build key to save ESSD data')
            return False
        return self.save_key_value(db, key_with_prefix, json_data)

    def add_lookup_entry(self, db=None, system_key=None, sub_type=None):
        if not system_key or not db:
            self.log.error(f'Unable to add lookup entry for {self.uuid}')
            return

        key = self.build_key([essd_constants.SYSTEMS_KEY, self.uuid])
        if not key:
            self.log.error(f'Unable to add lookup entry for {self.uuid}')
            return

        lookup_entry = {
            'type': 'essd',
            'key': system_key,
            'type_specific_data': {
                'suuid': self.uuid
            }
        }

        if sub_type:
            lookup_entry['type_specific_data']['sub_type'] = sub_type
        # Save Function will check if it already exists
        self.save_key_value(db, key, lookup_entry)

    @staticmethod
    def get_value_from_db(key=None, log=None, db=None):
        if not log:
            return None
        if not key or not db:
            log.error(f'No key or db provided to get value from db')
            return None
        try:
            value, _ = db.get(key)
            if not value:
                return None
            return value.decode('utf-8')
        except Exception as e:
            log.error(f'Caught exception {e} getting key {key}')
            log.exception(e)
            return None
