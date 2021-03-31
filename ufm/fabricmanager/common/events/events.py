"""

   BSD LICENSE

   Copyright (c) 2021 Samsung Electronics Co., Ltd.
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

     * Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
     * Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in
       the documentation and/or other materials provided with the
       distribution.
     * Neither the name of Samsung Electronics Co., Ltd. nor the names of
       its contributors may be used to endorse or promote products derived
       from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
"""

import json
import requests
import time

from common.events import event_constants

from common.events.events_def_en import components
from common.events.events_def_en import events
from common.events.events_def_en import severity


def get_events_from_etcd_db(db):
    """

    Not used by UFM!

    Save multiple events to DB

    A sample event is json.dumps of an event dictionary
    event = {'name': 'CM_NODE_UP',
             'node': 'NODE-1',
             'args': "{'node': 'NODE-1', 'cluster': 'CLS'} ",
             'timestamp': "<Time string in epoch>"}
    :param db: Instance of DB class
    :param event_list: List of events
    :return: True if successful or false.
    :exception Raises an exception in case of DB exception
    """
    event_list = []
    try:
        kv_out = db.get_key_with_prefix(event_constants.EVENT_KEY_PREFIX,
                                        raw=True,
                                        sort_order='ascend',
                                        sort_target='create')
        for k, v in kv_out.iteritems():
            event_list.append(json.loads(v))

    except Exception as ex:
        raise ex

    return event_list


# Gets an exception in case of error
def save_event_to_graphite_db(logger, graphite_ip_address, event):
    url = "http://{}/events".format(graphite_ip_address)
    try:
        name = event['name']
        event_id = event[name]['id']
        component_id = components[int(event_id[:2])]
        sev = severity[int(event_id[2:4])]

        payload = {
            'what': name,
            'tags': [event['node'], component_id, sev],
            'when': event['timestamp'],
            'data': {}
        }

        for k, v in event['args'].iteritems():
            payload['data'][str(k)] = str(v)

        resp = requests.post(url, json.dumps(payload))
        resp.raise_for_status()
    except Exception as ex:
        logger.exception("Exception in posting the event to Graphite DB {}".format(event))
        raise ex


def get_events_from_graphite_db(logger,
                                graphite_ip_address,
                                component=None,
                                severity=None,
                                start_time=None,
                                end_time=None):
    url = "http://{}/events/get_data".format(graphite_ip_address)

    tag_str = ' '.join([component, severity])
    payload = {'tags': tag_str,
               'from': start_time,
               'until': end_time}

    try:
        resp = requests.get(url, params=payload)
        resp.raise_for_status()
    except Exception as e:
        logger.exception('Exception in getting the events from GraphiteDB')
        raise e

    event_list = []
    out = json.loads(resp.text)
    for elem in out:
        event = dict()
        timestamp = elem['when']
        event_name = elem['what']
        event_tags = elem['data']

        event['time_epoch'] = int(timestamp)
        event['creation_time'] = time.strftime('%a %d %b %Y %H:%M:%S GMT', time.gmtime(float(timestamp)))
        event['severity'] = int(events[event_name]['id'][2:4])
        event['description'] = events[event_name]['msg'].format(**event_tags)

        event_list.append(event)

    return event_list
