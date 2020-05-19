import json
import redfish
from redfish.rest.v1 import redfish_client

login_host="http://172.22.4.36:5000"
login_account = ''
login_password = ''

#login_host = "https://192.168.1.100"
#login_account = "admin"
#login_password = "password"


print("Connect to a Redfish device")

root = redfish_client(base_url=login_host, max_retry=1, timeout=1)

A = {
     "/redfish/v1/",
     # "/redfish/v1/Systems",
     # "/redfish/v1/Systems/System.eSSD.1",
     # "/redfish/v1/Systems/System.eSSD.1/Storage",
     # "/redfish/v1/Chassis/1",
     # "/redfish/v1/Chassis/1/Thermal",
     # "/redfish/v1/Managers",
    }

print("======")

for a in A:
    response = root.get(a, headers=None)

    if response.status != 200:
        continue

    print("Type {}".format(type(response.dict)))

    d = response.dict

    # Print out the response
    print("{}".format(a) )
    print("{}".format( json.dumps(d, indent=4, sort_keys=True) ))

    for key, value in d.items():
        if type(value) != type(dict()):
            continue

        print("{} {} {}".format(type(value), key, value))

print("======")


# Logout of the current session
# root.logout()

