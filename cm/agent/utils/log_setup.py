import os
import sys

from logger import cm_logger

cm_dir = os.path.dirname(os.path.dirname(os.path.realpath(__file__))) + "/.."
sys.path.append(cm_dir)
# print(cm_dir)

agent_logger = None


def get_logger(name='KV-agent', config_path=None):
    global agent_logger
    if not agent_logger:
        try:
            log = cm_logger.CMLogger(name, config_path)
            agent_logger = log.create_logger()
        except Exception as e:
            print('Error in creating logger handle - ', e.message)
            sys.exit(-1)
    return agent_logger
