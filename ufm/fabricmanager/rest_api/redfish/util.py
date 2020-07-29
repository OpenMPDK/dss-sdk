import zmq
import json
from common.ufmdb.redfish.ufmdb_util import ufmdb_util

def post_to_switch(sw_uuid, payload):
    try:
        mq_port = ufmdb_util.get_mq_port(sw_uuid)
        ctx = zmq.Context()
        skt = ctx.socket(zmq.REQ)
        skt.connect("tcp://localhost:" + str(mq_port))

        json_request = json.dumps(payload, indent=4, sort_keys=True)
        json_response = None
        try:
            # print("post_to_switch request {}".format(json_request))
            skt.send_json(json_request)

            # block until response is received
            json_response = skt.recv_json()
            # print("post_to_switch response {}".format(json_response))
        except KeyboardInterrupt:
            pass

        skt.close()
        ctx.destroy()
    except Exception as e:
        print(e)
        return { "Status":"failed" }

    return json_response
