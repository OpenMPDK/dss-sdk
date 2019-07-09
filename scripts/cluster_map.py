import os,sys
import requests
import json

from utility import exception


class ClusterMap:

    def __init__(self, url):
        self.url = url
        self.response = self.get_response()
        self.response_code = 200

    @exception
    def get_response(self):
        json_response = {}
        print("INFO: Calling rest API {}".format(self.url))
        response = requests.get(self.url)
        if response.status_code == 200:
            json_response = json.loads(response.content.decode('utf-8'))
        else:
            print("WARNING: Bad Response with code {}".format(response.status_code))
            self.response_code = response.status_code

        return json_response


    def get_cluster_map(self):
        if self.response:
            return self.response.get("subsystem_maps")
        else:
            return []
