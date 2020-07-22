import os,sys
import zmq
import time

from common.events.events_def_en import events as event_list


class EventNotification:
    def __init__(self, address="10.1.51.205", port="6000", logger=None):
        self.address  = "tcp://" + address
        self.port     = port
        self.logger   = logger
        self.ctx      = zmq.Context()
        self.con      = self.connection()
        self.events   = event_list
        self.REQUEST_TIMEOUT = 2500 # Milliseconds
        self.REQUEST_RETRIES = 3    # Max three times it tries for connection.
        self.logger.debug("EventNotification address={}".format(address) )

    def __del__(self):
        try:
            self.con.close()
            self.ctx.term()
        except Exception as e:
            self.logger.error("Closing connection to zeromq broker: ".format(str(e)))

    def connection(self):
        """
        Create the socket connection
        :return:<socket>
        """
        # print('RAIN: EVENTNOTIFICATION Connection')
        try:
            socket  = self.ctx.socket(zmq.REQ)
            socket.connect("{}:{}".format(self.address,self.port))
            self.logger.info("Connected: {}:{}".format(self.address,self.port))
        except Exception as e:
            self.logger.error(str(e))

        return socket

    def get_event_list(self):
        """
        TODO
        Retrieve category data from ETCD database
        :return:
        """
        return category

    def process_events(self, events):
        """
        - Make pre-processing of the events if required.
        - Send events to the broker
        :param events:<list> list of events, event is is the form of dictionary
        :return:<list> List of delivered events
        """
        delivered_events = []

        for event in events:
            category = self.get_category(event)
            print(event)
            # Store delivered events.
            if self.send_events(category, event):
                delivered_events.append(event)

        return delivered_events

    def get_category(self,event):
        """
        Categorize event
        :param event: <dict>
        :return:<string>  return category name
        """
        event_name = event.get("name","")

        if event_name in self.events:
            return self.events[event_name].get('category', '')
        return ""

    def send_events(self, category="", event={}):
        """
        Send events to the ZeroMQ broker ...
        On failure keep re-sending event upto self.REQUEST_RETRIES times.
        :param category: <string>
        :param event: <dict > list of events
        :return: True/False-> success/failure
        """
        try:
            # Use lazy pirate pattern
            poller = zmq.Poller()
            index = 0
            while index < self.REQUEST_RETRIES:
                #self.logger.info("Sending event {}:{}".format(category, str(event)))
                message = str((category, event))

                poller.register(self.con, zmq.POLLIN)
                self.con.send_string("{}".format(message))

                sockets = dict(poller.poll(self.REQUEST_TIMEOUT))
                if sockets.get(self.con) == zmq.POLLIN:
                    is_done = self.con.recv()
                    if is_done == b'RECEIVED':
                        return True
                    elif is_done == b'FAILED':
                        #print("Delivery Failed for the Event:{}".format(event))
                        self.logger.error("Delivery Failed for the Event:{}".format(event))
                    else:
                        self.logger.error(f'Malformed response... {is_done}')
                    break
                else:
                    #print("No response from zeromq broker, retrying ...")
                    self.logger.error("No response from zeromq broker, retrying ...")
                    self.con.setsockopt(zmq.LINGER, 0)
                    self.con.close()
                    poller.unregister(self.con)
                    time.sleep(5)  # Added delay before re-connection
                    # Create a new connection
                    self.con = self.connection()
                    index +=1
                    if index == self.REQUEST_RETRIES:
                        self.logger.error("Failed to connect to ZeroMq broker. Looks like the server is down at the host {} ...".format(self.address))

        except Exception as e:
            self.logger.error(str(e))

        return False



