import ConfigParser
import os
import sys

CONFIG_SECTIONS = ['agent', 'logging']
PRODUCT_NAME = "nkv-agent"
DFLY_PRODUCT_NAME = "dragonfly"
AGENT_NAME = "agent"
AGENT_CONF_NAME = "%s.conf" % AGENT_NAME
AGENT_LOG_NAME = "%s.log" % AGENT_NAME

USTAT_NAME = "ustat"

BINARY_DIR = "/usr/%s/" % DFLY_PRODUCT_NAME
LOG_DIR = "/var/log/%s/" % PRODUCT_NAME
CONFIG_DIR = "/etc/%s/" % PRODUCT_NAME
TARGET_CONF_DIR = "/etc/%s/" % DFLY_PRODUCT_NAME

TARGET_BIN_NAME = "nvmf_tgt"
TARGET_CONF_NAME = "nvmf.in.conf"
TARGET_LOG_NAME = "nvmf_err.log"


DEFAULT_SETTINGS = {CONFIG_SECTIONS[0]: {"nvmf_tgt": BINARY_DIR + TARGET_BIN_NAME,
                                         "nvmf_conf_file": TARGET_CONF_DIR + TARGET_CONF_NAME,
                                         "ustat_binary": BINARY_DIR + USTAT_NAME,
                                         "hugepages": "8192",
                                         "stats_proto": "graphite",
                                         "stats_server": "127.0.0.1",
                                         "stats_poll": "10",
                                         "stats_port": "2004", },
                    CONFIG_SECTIONS[1]: {"log_dir": LOG_DIR,
                                         "log_file": AGENT_LOG_NAME,
                                         "log_level": "DEBUG",
                                         "console": "enabled",
                                         "console_level": "INFO",
                                         "syslog": "enabled",
                                         "syslog_level": "DEBUG",
                                         "syslog_facility": "local0", }, }

def create_config(filename):
    """
    Create daemon's configuration file if
    it does not exist
    :filename:
    :return:
    """
    settings = DEFAULT_SETTINGS

    if os.path.exists(filename):
        return None

    default_conf = ""
    for section in CONFIG_SECTIONS:
        if section in settings:
            for idx, key in enumerate(sorted(settings[section])):
                if idx == 0:
                    default_conf += "[%s]\n" % section
                default_conf += "%s=%s\n" % (key, settings[section][key])
            default_conf += "\n"

    print("Creating default CLI configuration file '%s'" % filename)
    try:
        f = open(filename, 'wb')
    except Exception as e:
        print("Open %s failed - %s" % (filename, str(e)))
        sys.exit(-1)
    f.write(default_conf)
    f.close()

    return settings


def load_config(filename):
    """
    Read daemon's configuration file for
    different start-up settings.
    :filename:
    :return: Dict of settings.
    """
    settings = create_config(filename)

    if settings is None:
        settings = DEFAULT_SETTINGS
        cfg_parser = ConfigParser.ConfigParser()
        try:
            cfg = cfg_parser.read(filename)
            for section in CONFIG_SECTIONS:
                options = cfg_parser.options(section)
                for option in options:
                    try:
                        settings[section][option] = cfg_parser.get(section, option)
                    except:
                        pass
        except Exception as e:
            print(e)
            pass

    return settings
