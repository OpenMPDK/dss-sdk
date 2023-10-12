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

#pragma once
#include <cassert>
#include <iostream>
#include "spdk/stdinc.h"
#include "spdk/bdev.h"
#include "spdk/event.h"
#include "spdk/env.h" // for spdk_dma_zmalloc
#include "parser.h"
#include "dss_block_allocator.h"
#include "apis/dss_superblock_apis.h"
//#include "dss_format_apis.h"
#define KILO_BYTE 1024
#define FOUR_KILO_BYTE 4096

namespace Format {

class ReadCallbackPayload {
public:
    ReadCallbackPayload()
        : desc(nullptr),
        channel(nullptr),
        bdev_super_block_physical_start_block(0),
        debug(false)
    {}
    virtual ~ReadCallbackPayload() = default;
    struct spdk_bdev_desc *desc;
    struct spdk_io_channel *channel;
    uint64_t bdev_super_block_physical_start_block;
    bool debug;
};

enum class DeviceState {
    OPEN,
    CLOSED
};

/* @brief Format interface with DSS and spdk
 * Formatter specifc API
 */
class Formatter {
public:
    Formatter()
        : bdev_physical_block_size_(0),
        bdev_logical_block_size_(0),
        bdev_user_physical_start_block_(0),
        bdev_user_physical_end_block_(0),
        bdev_block_alloc_meta_physical_start_block_(0),
        bdev_block_alloc_meta_physical_end_block_(0),
        bdev_super_block_physical_start_block_(SUPER_BLOCK_START),
        bdev_super_block_physical_end_block_(0),
        bdev_state_(DeviceState::CLOSED),
        bdev_optimal_io_boundary_(0),
        bdev_total_num_logical_blocks_(0),
        bdev_total_num_physical_blocks_(0),
        block_allocator_meta_num_blocks_(0)
    {}
    virtual ~Formatter() = default;
    static void format_bdev_open_cb(
            enum spdk_bdev_event_type type,
            struct spdk_bdev *bdev,
            void *event_ctx);
    bool open_device(Parser::ParsedPayloadSharedPtr& payload);
    static void format_bdev_write_super_complete_cb(
            struct spdk_bdev_io *bdev_io,
            bool success,
            void *cb_arg);
    static void format_bdev_write_complete_cb(
            struct spdk_bdev_io *bdev_io,
            bool success,
            void *cb_arg);
    static void format_bdev_read_super_complete_cb(
            struct spdk_bdev_io *bdev_io,
            bool success,
            void *cb_arg);
    static bool read_and_print_super_block(
            ReadCallbackPayload *payload);
    static void formatter_stop(struct spdk_bdev_io *bdev_io);
    bool format_device(bool is_debug);
    bool is_format_complete();
    static uint64_t format_bdev_write_rq_count;
    static uint64_t format_bdev_read_rq_count;
    static dss_super_block_t *written_super_block;
    static uint64_t total_super_write_blocks;
    // bdev specific variables
    static struct spdk_bdev *bdev;
    static struct spdk_bdev_desc *desc;
    static struct spdk_io_channel *channel;

private:
    uint32_t bdev_physical_block_size_; //block size in bytes
    uint64_t bdev_logical_block_size_; //block size in kilo-bytes
    uint64_t bdev_user_physical_start_block_; // start block for user data
    uint64_t bdev_user_physical_end_block_; // end block for user data
    uint64_t bdev_block_alloc_meta_physical_start_block_; // logical start
                                                        // for block alloc
                                                        // meta-data
    uint64_t bdev_block_alloc_meta_physical_end_block_; // logical end for
                                                       // block allocator
                                                       // meta-data
    uint64_t bdev_super_block_physical_start_block_;
    uint64_t bdev_super_block_physical_end_block_;
    DeviceState bdev_state_;
    uint32_t bdev_optimal_io_boundary_;
    uint64_t bdev_total_num_logical_blocks_;
    uint64_t bdev_total_num_physical_blocks_;
    uint64_t block_allocator_meta_num_blocks_;
};

using FormatterSharedPtr = std::shared_ptr<Formatter>;

}// end Namespace Format
