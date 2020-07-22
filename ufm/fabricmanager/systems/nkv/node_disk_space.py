
import os
import sys
import threading



class Node_Disk_Space(threading.Thread):
    def __init__(self, stopper_event=None, db=None, hostname=None, check_interval=60, logger=None):
        super(Node_Disk_Space, self).__init__()

        self.stopper_event = stopper_event
        self.db = db
        self.hostname = hostname
        self.check_interval = check_interval
        self.logger = logger
        self.logger.debug("Monitor disk space thread Init")


    def run(self):
        self.logger.debug("Monitor disk space thread Started")

        while not self.stopper_event.is_set():
            # Read System Disk Space and write it to db
            df_struct = os.statvfs('/')
            if df_struct.f_blocks > 0:
                df_out = df_struct.f_bfree * 100 / df_struct.f_blocks
                if df_out:
                    self.db.save_key_value('/cluster/' + self.hostname + '/space_avail_percent', df_out)

            self.stopper_event.wait(self.check_interval)

        self.logger.debug("Monitor disk space thread Stopped")


    def stop(self):
        pass


