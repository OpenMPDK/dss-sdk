#!/usr/bin/env python2

import functools
import sys
import argparse
from subprocess import Popen, PIPE
import netifaces
from threading import Timer

DELIMITER = ':'
SEPARATOR = ' '
CMD_SEPARATOR = ';'
g_event_list = []
g_event_processors = {}
g_cmd_timeout = 90


def parse_event(input_str):
    event = tuple(input_str.split(DELIMITER))
    # extraneous check?
    if not event:
        raise ValueError('Incorrect format in argument event')

    g_event_list.append(event)


def register_processor(event):
    def decorator_register(func):
        g_event_processors[event] = func
        return func
    return decorator_register


def generate_event(func):
    @functools.wraps(func)
    def wrapper_print_msg(*args, **kwargs):
        cmd = func(*args, **kwargs)
        tout = kwargs['timeout']
        # print('Running command {cmd} with timeout {tout}'.format(cmd=cmd, tout=tout))
        proc = Popen(cmd, stdout=PIPE, stderr=PIPE, universal_newlines=True, shell=True)
        timer = Timer(tout, proc.kill)
        try:
            timer.start()
            stdout, stderr = proc.communicate()
            if stderr:
                res = stderr
            else:
                res = stdout
            print('{result}'.format(result=res))
        finally:
            timer.cancel()
    return wrapper_print_msg


def get_default_interface():
    gateways = netifaces.gateways()
    return gateways['default'][netifaces.AF_INET][1]


def get_service_command(service=None, action=None):
    if service is None or action is None:
        print('service name and action are needed to form service command')
        raise ValueError
    cmd = 'systemctl' + SEPARATOR + action + SEPARATOR + service
    return cmd


# Toggle it
def get_interface_command(timeout):
    interface = get_default_interface()
    wait = timeout - 10
    if wait < 0:
        wait = 0
    cmd = 'ifconfig' + SEPARATOR + interface + SEPARATOR + 'down' + CMD_SEPARATOR\
          + 'sleep' + SEPARATOR + str(wait) + CMD_SEPARATOR\
          + 'ifconfig' + SEPARATOR + interface + SEPARATOR + 'up'
    return cmd


@register_processor('TARGET_DOWN')
@register_processor('TARGET_UP')
@generate_event
def target_event(timeout=g_cmd_timeout):
    return '/usr/bin/env reboot'


@register_processor('AGENT_DOWN')
@generate_event
def agent_event_down(timeout=g_cmd_timeout):
    return get_service_command('kv_cli', 'stop')


@register_processor('AGENT_UP')
@generate_event
def agent_event_up(timeout=g_cmd_timeout):
    return get_service_command('kv_cli', 'start')


@register_processor('SUBSYSTEM_DOWN')
@generate_event
def subsystem_event_down(timeout=g_cmd_timeout):
    return get_service_command('nvmf_tgt', 'stop')


@register_processor('SUBSYSTEM_UP')
@generate_event
def subsystem_event_up(timeout=g_cmd_timeout):
    return get_service_command('nvmf_tgt', 'start')


@register_processor('NETWORK_DOWN')
@register_processor('NETWORK_UP')
@generate_event
def network_event(timeout=g_cmd_timeout):
    return get_interface_command(timeout)


def generate_events():
    for e in g_event_list:
        if len(e) > 2:
            print('Invalid event format')
            continue
        tout = g_cmd_timeout
        if len(e) == 2:
            tout = int(e[1])
        evt_type = e[0]
        try:
            ep = g_event_processors[evt_type]
        except KeyError:
            print('Event type {event} is not supported'.format(event=evt_type))
            continue

        print('Generating event: {event}'.format(event=evt_type))
        ep(timeout=tout)


def main():
    global g_cmd_timeout
    parser = argparse.ArgumentParser(
         description='Script to genertate NKV events'
    )
    parser.add_argument('-e', '--event', dest='event',
                        help='event and timeout in format event:timeout',
                        type=parse_event)
    parser.add_argument('-t', '--timeout', dest='timeout',
                         help='default timeout')
    args = parser.parse_args()
    if args.timeout:
        g_cmd_timeout = args.timeout

    generate_events()


if __name__ == '__main__':
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print('Caught Ctrl-C, Exiting')
        sys.exit(0)
    except Exception as ex:
        print('Caught exception in main(): {exception}'.format(exception=ex))
        raise
