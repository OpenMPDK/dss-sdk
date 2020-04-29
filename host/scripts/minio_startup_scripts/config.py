#!/usr/bin/python

import os,sys
import json

class Config:

    def __init__(self, *agrs, **kwargs):
        self.config_file = self.get_config_file(agrs[0].pop("config", ""))
        self.command_line_params = agrs[0]



    def get_config(self):
        """
        Get configuration details from config file ...
        :return:<dict> complete configuration dictionary.
        """
        #config_file = self.get_config_file()
        with open(self.config_file, "rb") as cfg:
            config = json.loads(cfg.read().decode('UTF-8', "ignore"))

        # Override nkv config parameter by command line arguments.
        for key in self.command_line_params:
            if key in config and self.command_line_params[key]:
                config[key] = self.command_line_params[key]
        return config

    def get_config_file(self, config_file):
        """
        Return the configuration file.
        :param config_file:
        :return:<string> complete configuration file
        """
        if not config_file:
            config_file = os.path.dirname(__file__) + "config.json"

        config_file = os.path.abspath(config_file)
        print("INFO: processing config file {}".format(config_file))
        return config_file
