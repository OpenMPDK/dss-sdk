/**
 # The Clear BSD License
 #
 # Copyright (c) 2022 Samsung Electronics Co., Ltd.
 # All rights reserved.
 #
 # Redistribution and use in source and binary forms, with or without
 # modification, are permitted (subject to the limitations in the disclaimer
 # below) provided that the following conditions are met:
 #
 # * Redistributions of source code must retain the above copyright notice, 
 #   this list of conditions and the following disclaimer.
 # * Redistributions in binary form must reproduce the above copyright notice,
 #   this list of conditions and the following disclaimer in the documentation
 #   and/or other materials provided with the distribution.
 # * Neither the name of Samsung Electronics Co., Ltd. nor the names of its
 #   contributors may be used to endorse or promote products derived from this
 #   software without specific prior written permission.
 # NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY
 # THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 # CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT
 # NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 # PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 # CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 # EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 # PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 # OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 # WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 # OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 # ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 */

#ifndef NKV_EVENT_H
#define NKV_EVENT_H

#include <iostream>
#include <stdlib.h>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/foreach.hpp>
#include <boost/optional/optional.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include "csmglogger.h"
#include "nkv_utils.h"
#include "nkv_framework.h"
#include <unistd.h>
#include <zmqpp/zmqpp.hpp>
#include <string>
#include <fstream>
#include <unistd.h>
#include <queue>
#include <vector>
#include <mutex>
#include <condition_variable>


using namespace std;
using namespace boost::filesystem;
using boost::property_tree::ptree;
using boost::property_tree::read_json;
using boost::property_tree::write_json;


extern c_smglogger* logger;
const int max_poll_timeout = 1000;

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
          uint64_t nkv_handle);

/* Function Name: add_event()
 * Params       : std::queue<std::string>& - shared event_queue
 * Return       : None
 * Description  : Add event to the event_queue
 */
void add_events(std::queue<std::string>& event_queue, std::string& event);

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
void action_manager(std::queue<std::string>& event_queue);

/* Function Name: update_nkv_config
 * Params       : <string-address>,<string-port>,<string-nqn>,<string-event_name>
 * Returns      : <bool> Success/Failure
 * Description  : Check for presence of nvme remote mount paths
 *        update the status of subsystem_maps and nkv_remote_paths
 */
bool update_nkv_targets(ptree&);

/* Function Name: event_mapping
 * Params       : None
 * Return       : true/false
 * Description  : Update event_map shared data strcuture.
 *                Default location of event_mapping.json <repo>/config/event_mapping.json
 *                Either copy the same file to present execution directory or specify file
 *                path to the EVENT_MAP_PATH environment variable.
 */
bool event_mapping();

// Update subsystem status at FM datastructure on receiving event for subsystem.
void update_fm_subsystem_status(const string& nqn, const int32_t& status);

// Update interface status at FM datastructure on receiving event for interface.
void update_fm_interface_status(const string& nqn, const string& address, const int32_t& status);

#endif
