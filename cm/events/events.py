import ast
import json
import requests
import time
from uuid import uuid1

from events_def_en import components, events, severity

# Don't change this. The change here needs to be validated in
# get_events_from_etcd_db and monitor/etcd_monitor.py
# Don't add '/' at the end of the prefix which breaks the logic
# as we are using EVENT_KEY_PREFIX to get the events both
# processed and unprocessed for REST calls
ETCD_EVENT_KEY_PREFIX = '/events'
ETCD_EVENT_TO_PROCESS_KEY_PREFIX = '/events_to_process'


def save_events_to_etcd_db(db_handle, event_list):
    """Save multiple events to DB
    A sample event is json.dumps of an event dictionary
    event = {'name': 'CM_NODE_UP',
             'node': 'NODE-1',
             'args': "{'node': 'NODE-1', 'cluster': 'CLS'} ",
             'timestamp': "<Time string in epoch>"}
    :param db_handle: Instance of DB class
    :param event_list: List of events
    :return: True if successful or false.
    :exception Raises an exception in case of DB exception
    """
    if not isinstance(event_list, list):
        return

    kv_dict = dict()
    for event in event_list:
        if (('name' not in event) or
            ('timestamp' not in event) or
            ('node' not in event)):
            db_handle.logger.info('Received an invalid event %s. Skipping it',
                                  str(event))
            continue
        ev_key = '/'.join([ETCD_EVENT_TO_PROCESS_KEY_PREFIX,
                           str(event['timestamp']),str(uuid1())])
        kv_dict[ev_key] = json.dumps(event)
        '''
        try:
            timestamp = event['timestamp']
            node = event['node']
            event_name = event['name']
            ev_key = '/'.join([EVENT_KEY_PREFIX, str(timestamp), node,
                              event_name])
            kv_dict[ev_key] = str(event['args'])
        except:
            db_handle.logger.exception('Error in parsing the event %s',
                                       str(event))
        '''
    try:
        ret = db_handle.save_multiple_key_values(kv_dict)
    except Exception as e:
        raise e
    return ret


def get_events_from_etcd_db(db_handle):
    """Save multiple events to DB
    A sample event is json.dumps of an event dictionary
    event = {'name': 'CM_NODE_UP',
             'node': 'NODE-1',
             'args': "{'node': 'NODE-1', 'cluster': 'CLS'} ",
             'timestamp': "<Time string in epoch>"}
    :param db_handle: Instance of DB class
    :param event_list: List of events
    :return: True if successful or false.
    :exception Raises an exception in case of DB exception
    """
    event_list = []
    '''
    try:
        json_out = db_handle.get_key_with_prefix(ETCD_EVENT_KEY_PREFIX)
        json_out = json_out['cluster']
        for cat, elist in json_out.iteritems():
            for ts, ne in elist.iteritems():
                for node, event in ne.iteritems():
                    for event_name, event_args in event.iteritems():
                        e = dict()
                        e['timestamp'] = ts
                        # e['node'] = node
                        e['name'] = event_name
                        e['args'] = ast.literal_eval(event_args)
                        event_list.append(e)
    except Exception as e:
        raise e
    '''
    try:
        kv_out = db_handle.get_key_with_prefix(ETCD_EVENT_KEY_PREFIX,
                                               raw=True,
                                               sort_order='ascend',
                                               sort_target='create')
        for k, v in kv_out.iteritems():
            event_list.append(json.loads(v))
    except Exception as e:
        raise e
    return event_list


# Gets an exception in case of error
def save_event_to_graphite_db(logger, graphite_ip_address, event):
    url = "http://" + graphite_ip_address + "/events"
    try:
        name = event['name']
        event_id = events[name]['id']
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
        '''
        data = ('{"what": ' + name +', "tags":["' + component_id + '","' + sev
                + '","' + event['node'] + '"], "when":' +
                str(event['timestamp']) + ', "data": "{')
        for k, v in event['args'].iteritems():
            data += '\\"' + str(k) + '\\":\\"' + str(v) + '\\",'
        data += '\\"id:' + event_id + '\\"'
        data += '}"}'
        '''
        resp = requests.post(url, json.dumps(payload))
        resp.raise_for_status()
    except Exception as e:
        logger.exception("Exception in posting the event to Graphite DB %s ",
                         str(event))
        raise e


def get_events_from_graphite_db(logger, graphite_ip_address, component=None,
                                severity=None, start_time=None, end_time=None):
    events = []
    url = "http://" + graphite_ip_address + "/events/get_data"
    # tags can be in a string separated with spaces or multiple tags keys in
    #  dict
    tag_str = ' '.join([component, severity])
    payload = {'tags': tag_str, 'from': start_time,
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
        # node = elem['tags']
        event['time_epoch'] = int(timestamp)
        event['creation_time'] = time.strftime('%a %d %b %Y %H:%M:%S GMT',
                                               time.gmtime(float(timestamp)))
        event['severity'] = int(events[event_name]['id'][2:4])
        event['description'] = events[event_name]['msg'].format(**event_tags)
        event_list.append(event)
    # event_list = sorted(events, key=lambda e: e['time_epoch'], reverse=True)
    return event_list
