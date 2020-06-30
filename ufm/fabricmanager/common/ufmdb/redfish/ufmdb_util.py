
from common.ufmdb import ufmdb

class ufmdb_util():

    # Given a prefix, return a dict of full key and value pairs
    @classmethod
    def query_prefix(self, qstring):
        db = ufmdb.client(db_type='etcd')

        kv_dict = dict()
        query = db.get_prefix(qstring)
        if query == None:
            query = db.get_prefix(qstring)
            if query == None:
                return dict()

        for value, metadata in query:
            kv_dict.update({metadata.key.decode('utf-8'):value.decode('utf-8')})

        return(kv_dict)

    @classmethod
    def has_fabrics(self):
        return True

        db = ufmdb.client(db_type='etcd')
        key = '/' + redfish_constants.FABRICS + '/list'
        result = db.get_prefix(key)
        if result:
            return True
        else:
            return False


    @classmethod
    def is_valid_fabric(self, fab_id):
        return True

        db = ufmdb.client(db_type='etcd')
        key = '/' + redfish_constants.FABRICS + '/list/' + fab_id
        result = db.get(key)
        if result:
            return True
        else:
            return False



    @classmethod
    def is_valid_switch(self, sw_id):
        return True

        db = ufmdb.client(db_type='etcd')
        key = '/switches/list/' + sw_id
        result = db.get(key)
        if result:
            return True
        else:
            return False


    @classmethod
    def get_mq_port(self, sw_uuid):
        pre = '/switches/' + sw_uuid + '/switch_attributes/mq_port/'
        kv_dict = self.query_prefix(pre)

        mq_port = None
        for k in kv_dict:
            mq_port = k.split('/')[-1]

        return mq_port


    @classmethod
    def convert_ranges_to_list(self, ranges_str):
        ranges = [(lambda l: range(l[0], l[-1]+1))(list(map(int, r.split('-')))) for r in ranges_str.split(',')]
        ret_list = [y for x in ranges for y in x]
        return ret_list



