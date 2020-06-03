
from systems.switch.switch_arg import SwitchArg

class SwitchMellanoxClient(object):
    def __init__(self, swArg):
        self.swArg = swArg
        self.log = swArg.log
        self.url = None
        self.log.info("SwitchMellanoxClient ip = {}".format(self.swArg.sw_ip))
        self._running = False
        self.session = None
        self.log.info("Init {}".format(self.__class__.__name__))

    def do_something(self):
        print('LUFAN: I am SwitchMellanoxClient.do_something()')


def client(swArg):
    """Return an instance of SwitchMellanoxClient."""
    print('LUFAN: will return a SwitchMellanoxClient object')
    return SwitchMellanoxClient(swArg)


