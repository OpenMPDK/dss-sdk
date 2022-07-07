from pyhaproxy.parse import Parser
from pyhaproxy.render import Render
import pyhaproxy.config as config
import argparse


def modifyCfg(server_list, filepath):
    # Build the configuration instance by calling Parser('config_file').build_configuration()
    cfg_parser = Parser('haproxy.cfg')
    configuration = cfg_parser.build_configuration()

    # Get the backend section
    backend = configuration.backend('nodes')

    # Remove existing 'Server' sections
    for server in backend.servers():
        backend.remove_server(server.name)

    # Add new server entries
    for server in server_list:
        server_args = server.split(':')
        name = server_args[0]
        ip = server_args[1]
        port = server_args[2]
        health_check_port = server_args[3]
        new_server = config.Server(name, ip, port, attributes=["check", "port", health_check_port])
        backend.add_server(new_server)

    # Save the changes to the new config file
    cfg_render = Render(configuration)
    if filepath != "":
        filepath = filepath + "/"
    cfg_render.dumps_to(str(filepath + 'haproxy.cfg'))


if __name__ == '__main__':

    parser = argparse.ArgumentParser(
        description='Update haproxy Configuration Settings.')
    parser.add_argument("--addservers", help="comma separated list of servers "
                        "in the format "
                        "<server name>:<IP addr>:<Port>:<Health Check Port>",
                        dest="server", required=True)
    parser.add_argument("--outdir", help="path to the output file",
                        dest="filepath", default="")

    args = parser.parse_args()
    server_list = args.server.split(',')
    modifyCfg(server_list, args.filepath)
