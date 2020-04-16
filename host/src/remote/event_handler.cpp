/**
 *   BSD LICENSE
 *
 *   Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Samsung Electronics Co., Ltd. nor the names of
 *       its contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "event_handler.h"

std::mutex _mtx;
boost::property_tree::ptree event_map;
std::atomic<bool> is_event_mapping_done (false);


/* Function Name: receive_events
 * Input Args   : <std:queue<std::string>> = shared event queue
 *                <std::string> = message queue address
 *                <std::vector<std::string>&> = list of subscribed channels
 *                <int32_t> = event polling interval
 *                <uint64_t> = nkv handle
 * Return       : None
 * Description  : Receive the events from a zeromq message broker for specified
 *                channels.
 */
void receive_events(std::queue<std::string>& event_queue,
          std::string& mq_address, 
          std::vector<std::string>& event_subscribe_channels,
          int32_t nkv_event_polling_interval,
          uint64_t nkv_handle )
{
  // Initialize context ...
  zmqpp::context context;
  // create and bind a server socket
  zmqpp::socket subscriber (context, zmqpp::socket_type::subscribe);
  std::string subscribed_channel;
  for( auto& event_subscribe_channel: event_subscribe_channels) {
    boost::trim(event_subscribe_channel);
    subscribed_channel.append("\n\t\t\t\t\t\t\t[" + event_subscribe_channel + "]");
    subscriber.set(zmqpp::socket_option::subscribe, event_subscribe_channel);
  }
  smg_alert(logger, "Receving events to the following subscribed channels ... %s", subscribed_channel.c_str());

  int interval;
  interval = event_subscribe_channels.size();
  //zmq_setsockopt(subscriber, ZMQ_SNDHWM, &interval , sizeof(int)); // Commented as we are not sending back any message.
  zmq_setsockopt(subscriber, ZMQ_RCVHWM, &interval , sizeof(int));
  /*int rinterval = 1000;
  zmq_setsockopt(subscriber, ZMQ_RECONNECT_IVL, &rinterval , sizeof(int));
  zmq_setsockopt(subscriber, ZMQ_RECONNECT_IVL_MAX, &rinterval , sizeof(int));*/

  int timeout = 5000;
  zmq_setsockopt(subscriber, ZMQ_HEARTBEAT_TIMEOUT,  &timeout, sizeof(int));
  int heartbeat_interval = 50000;
  zmq_setsockopt(subscriber, ZMQ_HEARTBEAT_IVL,&heartbeat_interval, sizeof(int));

  subscriber.connect(mq_address.c_str());


  // Pollin ...
  zmqpp::poller poller;
  poller.add(subscriber);

  // Response socket
  //zmqpp::socket response (context, zmqpp::socket_type::push);
  //response.connect(ADDRESS_PORT_RESPONSE);
  smg_alert(logger, "Receiving events from %s", mq_address.c_str());
  
  try {
    while(1) {
      poller.add(subscriber, ZMQ_POLLIN);
      int event = poller.poll(max_poll_timeout);
      zmqpp::message message;
    
      if (event & ZMQ_POLLIN) {
        subscriber.receive(message);
        smg_info(logger, "*** Received Event:%s", (message.get(0)).c_str() );
        std::vector<string> lines;
        std::string line = message.get(0);
        boost::split(lines, line , [](char c){return c== '=';});

        // Need to add event to the queue if that matches with selected channel
        add_events(event_queue, lines[1]);

      } else {
        subscriber.connect(mq_address.c_str());
      }
    
      std::this_thread::sleep_for (std::chrono::seconds(nkv_event_polling_interval));
 
      if( nkv_stopping ){
        smg_alert(logger, "Stopping event_receiver for NKV = %u ", nkv_handle);
        break;
      }
    } // End of while
  }
  catch(std::exception const& e) {
    smg_error(logger, "EXCEPTION: %s", e.what());
  }

   //zmq_close(subscriber);
   //zmq_term(context);
   subscriber.close();
   context.terminate();
}

/* Function Name: add_event()
 * Params       : std::queue<std::string>& - shared event_queue
 * Return       : None
 * Description  : Add event to the event_queue
 */
void add_events(std::queue<std::string>& event_queue, std::string& event)
{
  smg_debug(logger,"EVENT RECEIVED = %s", event.c_str());
  std::lock_guard<std::mutex> lock(_mtx);
  event_queue.push(event);
}


/* Function Name: action_manager
 * Params     : std::queue<std::string>&  - A shared quque
 * Returns    : None 
 * Description  : Works in a different thread, read event from event_queue and
 *        update internal datastructure
 *
 *  Sample event structure received from Fabric Manager
 *  { "name" : <event name>,
 *    "node" : <target node where event was generated>
 *    "args" : {"nqn": <subsystem_nqn>, "address":<ip address>, "port":<port>}
 *    "timestamp": "123456"
 *   }
 */
void action_manager(std::queue<std::string>& event_queue)
{
  std::lock_guard<std::mutex> lock(_mtx);
  while (is_event_mapping_done.load(std::memory_order_relaxed) && event_queue.size() ) {
    
    std::string event = event_queue.front();
    event_queue.pop();
    
    // Process event as required 
    ptree event_tree;
    try {
      std::istringstream is (event.c_str());
      read_json(is, event_tree);
      
      // Look at the nkv internal data_structure
      update_nkv_targets(event_tree);
    }
    catch (std::exception const& e) {
      smg_error(logger, "%s - %s", event_tree.get<std::string>("name").c_str(), e.what());
    }
  } 
}

/* Function Name: update_nkv_config
 * Params       : <string-address>,<string-port>,<string-nqn>,<string-event_name>
 * Returns      : <bool> Success/Failure
 * Description  : Check for presence of nvme remote mount paths
 *        update the status of subsystem_maps and nkv_remote_paths
 */

bool update_nkv_targets(ptree& event_tree) 
{
  std::string event_name = event_tree.get<std::string>("name");
  ptree args = event_tree.get_child("args");
  
  std::string category = event_map.get<std::string>(event_name+".CATEGORY", "");
  int32_t     status   = event_map.get<int32_t>(event_name+".STATUS", 0);
  
  if ( nkv_cnt_list ) {
    // Update remote mount paths status 
    return nkv_cnt_list->update_container( category, event_tree.get<std::string>("node", ""), args , status );
  }

  smg_error(logger, "Empty target container list ...");
  return false;
}

/* Function Name: event_mapping
 * Params       : None
 * Return       : true/false
 * Description  : Update event_map shared data strcuture.
 *                Default location of event_mapping.json <repo>/config/event_mapping.json
 *                Either copy the same file to present execution directory or specify file
 *                path to the EVENT_MAP_PATH environment variable.
 */
bool event_mapping() 
{
  std::string event_map_path("event_mapping.json");
  try {
    if ( const char* env_event_map_path = std::getenv("EVENT_MAP_PATH") ) {
      event_map_path = env_event_map_path;
    }
    boost::property_tree::read_json(event_map_path, event_map);
    smg_alert(logger,"Reading event_map from %s", event_map_path.c_str());
  }
  catch (std::exception& e) {
    smg_error(logger, "%s%s", "event_map reading FAILED - ", e.what());
    smg_warn(logger, "%s or %s" ,"Copy event_mapping.json to execution directory",
                      "Set EVENT_MAP_PATH env variable with that file path."); 
    return false;
  }
  is_event_mapping_done.store(true, std::memory_order_relaxed);
  return true;
}
