import signal

from log_setup import agent_logger


class UDEVMonitorSignalHandler:
    def __init__(self, poll_event, mode_event,
                 start_threads_fn=None, stop_threads_fn=None):
        self.poll_event = poll_event
        self.mode_event = mode_event
        self.start_monitor_threads_fn = start_threads_fn
        self.stop_monitor_threads_fn = stop_threads_fn
        self.exit = 0
        self.logger = agent_logger

    def signal_handler_sigint(self, signal, frame):
        if self.logger:
            self.logger.info('==== Received SIGINT ====')
        else:
            print('==== Received SIGINT ====')
        self.exit = 1
        self.mode_event.set()

    def signal_handler_sigusr1(self, signal, frame):
        if self.logger:
            self.logger.info('==== Received SIGUSR1 ====')
        else:
            print('==== Received SIGUSR1 ====')
        if self.start_monitor_threads_fn:
            self.logger.info('Calling start monitor threads function')
            self.start_monitor_threads_fn()

    def signal_handler_sigusr2(self, signal, frame):
        if self.logger:
            self.logger.info('==== Received SIGUSR2 ====')
        else:
            print('==== Received SIGUSR2 ====')
        if self.stop_monitor_threads_fn:
            self.logger.info('Calling stop monitor threads function')
            self.stop_monitor_threads_fn()

    def signal_handler_sighup(self, signal, frame):
        if self.logger:
            self.logger.info('==== Received SIGHUP ====')
        else:
            print('==== Received SIGHUP ====')
        self.poll_event.set()

    def catch_SIGUSR1(self):
        signal.signal(signal.SIGUSR1, self.signal_handler_sigusr1)

    def catch_SIGUSR2(self):
        signal.signal(signal.SIGUSR2, self.signal_handler_sigusr2)

    def catch_SIGINT(self):
        signal.signal(signal.SIGINT, self.signal_handler_sigint)

    def catch_SIGHUP(self):
        signal.signal(signal.SIGHUP, self.signal_handler_sighup)

    def catch_signal(self):
        self.catch_SIGHUP()
        self.catch_SIGINT()
        self.catch_SIGUSR1()
        self.catch_SIGUSR2()
