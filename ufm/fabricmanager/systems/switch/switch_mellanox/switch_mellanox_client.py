
import json
import requests
from systems.switch.switch_arg import SwitchArg
from systems.switch.switch import EthSwitch

class SwitchMellanoxClient(EthSwitch):
    def __init__(self, swArg):
        self.swArg = swArg
        self.log = swArg.log
        self.url = 'https://'
        self.log.info("SwitchMellanoxClient ip = {}".format(self.swArg.sw_ip))
        self._running = False
        self.session = None


    def __del__(self):
        pass

    def start(self):
        self.log.info("======> SwitchMellanoxClient <=========")

        if self._running:
            self.log.debug("==> SwitchMellanoxClient is already running <==")
            return

        self.session = self.connect()
        self._running = True
        self.log.info("======> SwitchMellanoxClient has started <=========")


    def stop(self):
        self._running = False
        self.log.info("======> SwitchMellanoxClient Stopped <=========")


    def is_running(self):
        return self._running

    def connect(self):
        self.url = 'https://' + self.swArg.sw_ip + '/admin/launch'
        self.log.info('Switch login url: ' + self.url)
        ses = requests.session()

        params = (
            ('script', 'rh'),
            ('template', 'login'),
            ('action', 'login'),
        )
        data = {
            'f_user_id': 'admin',
            'f_password': 'admin',
        }

        response = ses.post(self.url, params=params, data=data, verify=False)
        print(str(response.status_code))
        self.log.info('Switch response status_code (200=OK): ' + str(response.status_code))
        self.log.info('Switch response: %s', response.text)

        # hardcoded the future cmds to be json format
        self.url = self.url + '?script=json'
        self.log.info('Switch json cmd url: ' + self.url)

        return ses


    def send_cmd(self, json_data):
        '''
        example_data = {
            "commands":
            [
                "show version", # can keep adding lines
            ]
        }
        '''
        self.log.info('Switch cmd url: ' + self.url)

        self.log.info('Sending Switch cmd: ')
        self.log.info(json.dumps(json_data))

        response = self.session.post(self.url, json=json_data, verify=False)

        self.log.info('Switch response status_code (200=OK): ' + str(response.status_code))
        self.log.info('Switch response: %s', response.text)
        return response


    def show_interface_vlan(self, vlan_id):
        json_cmd = {
            "commands":
            [
                "show interface vlan " + str(vlan_id)
            ]
        }
        resp = self.send_cmd(self, json_cmd)
        return resp


def client(swArg=None):
    """Return an instance of SwitchMellanoxClient."""
    return SwitchMellanoxClient(swArg)


