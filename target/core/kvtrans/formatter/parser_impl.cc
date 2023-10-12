/**
 *  The Clear BSD License
 *
 *  Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted (subject to the limitations in the
 *  disclaimer below) provided that the following conditions are met:
 *
 *  	* Redistributions of source code must retain the above copyright
 *  	  notice, this list of conditions and the following disclaimer.
 *  	* Redistributions in binary form must reproduce the above copyright
 *  	  notice, this list of conditions and the following disclaimer in
 *  	  the documentation and/or other materials provided with the distribution.
 *  	* Neither the name of Samsung Electronics Co., Ltd. nor the names of its
 *  	  contributors may be used to endorse or promote products derived from
 *  	  this software without specific prior written permission.

 *  NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED
 *  BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 *  CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
 *  BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 *  FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 *  USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "parser.h"
#include <fstream>
#include <iterator>
#include <sstream>
#include <iostream>

namespace Parser {

ParsedPayloadSharedPtr TextParser::parse_payload(
        ParserType type, std::string& filename) const {

    std::ifstream infile (filename);
    std::string line;
    std::stringstream ss;
    char delim = '\t';
    std::string delim_val;
    uint32_t count = 1;
    std::string::size_type sz = 0;

    // Allocate memory for ParsedPayload
    ParsedPayloadSharedPtr payload =
        std::make_shared<ParsedPayload>();


    while (std::getline(infile, line)) {

        ss.str(line);
        while (std::getline(ss, delim_val, delim)) {
            // Expected config will be tab-delimited
            // in the order defined in parser.h for
            // ParsedPayload variables
            if (count == 1) {
                // Extract block device name
                payload->device_name = delim_val;
            }

            if (count == 2) {
                // Extract logical block size
                sz = 0;
                payload->logical_block_size = std::stoull(
                        delim_val, &sz, 0);
            }

            if (count == 3) {
                // Extract number of block states
                sz = 0;
                payload->num_block_states = std::stoull(
                        delim_val, &sz, 0);
            }

            if (count == 4) {
                // Extract block allocator type
                payload->block_allocator_type = delim_val;
            }

            if (count == 5) {
                // Extract if debug
                if (delim_val.compare("true") == 0) {
                    payload->is_debug = true;
                } else {
                    payload->is_debug = false;
                }
            }
            count++;
        }

        if (count != PARSER_EXPECTED_FIELDS) {
            payload->status = false;
        } else {
            payload->status = true;
        }

    }

    return std::move(payload);
}

}// end Namespace Parser
