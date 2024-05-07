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

#include "formatter.h"

namespace Format {

uint64_t Formatter::format_bdev_write_rq_count = 0;
uint64_t Formatter::format_bdev_read_rq_count = 0;
dss_super_block_t *Formatter::written_super_block = nullptr;
uint64_t Formatter::total_super_write_blocks = 0;
struct spdk_bdev *Formatter::bdev = nullptr;
struct spdk_bdev_desc *Formatter::desc = nullptr;
struct spdk_io_channel *Formatter::channel = nullptr;

void Formatter::format_bdev_open_cb(enum spdk_bdev_event_type type,
        struct spdk_bdev *bdev,
        void *event_ctx) {
    return;
}

void Formatter::formatter_stop(struct spdk_bdev_io *bdev_io) {

    if (Formatter::channel == nullptr || 
            Formatter::desc == nullptr) {
        return;
    }

    // Complete the bdev io and close the channel
    spdk_bdev_free_io(bdev_io);
    spdk_put_io_channel(Formatter::channel);
    spdk_bdev_close(Formatter::desc);
    SPDK_NOTICELOG("Stopping app\n");
    spdk_app_stop(0);

    return;
}

bool Formatter::open_device(Parser::ParsedPayloadSharedPtr& payload) {

    bool is_dev_writable = true;
    int status = 1;
    uint64_t block_allocator_meta_size = 0;
    uint64_t num_physical_blocks = 0;
    uint64_t num_logical_blocks = 0;
    uint64_t sblock_size_in_bytes = 0;
    dss_blk_allocator_opts_t *ba_config = nullptr;

    // Procure device from name
    Formatter::bdev = spdk_bdev_get_by_name(payload->device_name.c_str());
    if (Formatter::bdev == nullptr) {
        assert(("ERROR", false));
    }

    // Open block device
    status = spdk_bdev_open_ext(
            payload->device_name.c_str(),
            is_dev_writable,
            Formatter::format_bdev_open_cb,
            nullptr,
            &Formatter::desc);
    if (status != 0) {
        assert(("ERROR", false));
    }

    // Procure the channel related to desc
    Formatter::channel = spdk_bdev_get_io_channel(Formatter::desc);
    if (Formatter::channel == nullptr) {
        assert(("ERROR", false));
    }

    // Procure total number of blocks on bdev
    this->bdev_total_num_physical_blocks_ =
        spdk_bdev_get_num_blocks(Formatter::bdev);

    // bdev_physical_block_size_ is procured from bdev
    this->bdev_physical_block_size_ =
        spdk_bdev_get_block_size(Formatter::bdev);

    // bdev_logical_block_size_ is procured from config/payload
    this->bdev_logical_block_size_ = payload->logical_block_size;

    this->bdev_total_num_logical_blocks_ =
        (this->bdev_total_num_physical_blocks_ *
            this->bdev_physical_block_size_) /
                this->bdev_logical_block_size_;

    // Compute space required for persisting block allocator meta-data
    // CXX TODO!
    // Procure optimal IO boundary
    this->bdev_optimal_io_boundary_ =
        spdk_bdev_get_optimal_io_boundary(Formatter::bdev);
    ba_config = 
        (dss_blk_allocator_opts_t *)malloc(sizeof(dss_blk_allocator_opts_t));
    if (ba_config == nullptr) {
        assert(("ERROR", false));
    }
    memset(ba_config, 0, sizeof(dss_blk_allocator_opts_t));
    ba_config->blk_allocator_type =
        const_cast<char*>(payload->block_allocator_type.c_str());
    ba_config->num_total_blocks = this->bdev_total_num_logical_blocks_;
    ba_config->num_block_states = payload->num_block_states;
    ba_config->shard_size = this->bdev_optimal_io_boundary_;
    ba_config->allocator_block_size = this->bdev_logical_block_size_;
    ba_config->logical_start_block_offset = 1; //dummy

    block_allocator_meta_size =
        dss_blk_allocator_get_physical_size(ba_config);

    if (block_allocator_meta_size == 0) {
        assert(("ERROR", false));
    }

    // Free the pseudo config
    free(ba_config);

    this->block_allocator_meta_physical_num_blocks_ =
        block_allocator_meta_size / this->bdev_physical_block_size_;

    if (block_allocator_meta_size % this->bdev_physical_block_size_ != 0) {
        this->block_allocator_meta_physical_num_blocks_++;
    }

    this->block_allocator_meta_logical_num_blocks_ =
        block_allocator_meta_size / this->bdev_logical_block_size_;

    if (block_allocator_meta_size % this->bdev_logical_block_size_ != 0) {
        this->block_allocator_meta_logical_num_blocks_++;
    }

    // bdev_super_block_physical_end_block_ is deduced as follows
    
    // Gather super block size in bdev blocks
    sblock_size_in_bytes = sizeof(dss_super_block_t);
    num_physical_blocks =
        sblock_size_in_bytes / this->bdev_physical_block_size_;
    if (sblock_size_in_bytes % this->bdev_physical_block_size_ != 0) {
        num_physical_blocks++;
    }

    // Gather number of logical blocks required to represent super-block
    num_logical_blocks =
        sblock_size_in_bytes / this->bdev_logical_block_size_;
    if (sblock_size_in_bytes % this->bdev_logical_block_size_ != 0) {
        num_logical_blocks++;
    }
    
    this->bdev_super_block_physical_end_block_ =
        (this->bdev_super_block_physical_start_block_ +
            num_physical_blocks) - 1;

    this->bdev_super_block_logical_end_block_ =
        (this->bdev_super_block_logical_start_block_ +
            num_logical_blocks) - 1;

    // bdev_block_alloc_meta_physical_start_block_ is deduced as follows
    this->bdev_block_alloc_meta_physical_start_block_ =
        this->bdev_super_block_physical_end_block_ + 1;

    // bdev_block_alloc_meta_logical_start_block_ is deduced as follows
    this->bdev_block_alloc_meta_logical_start_block_ =
        this->bdev_super_block_logical_end_block_ + 1;

    // bdev_block_alloc_meta_physical_end_block_ is deduced as follows
    this->bdev_block_alloc_meta_physical_end_block_ =
        (this->bdev_block_alloc_meta_physical_start_block_ +
            this->block_allocator_meta_physical_num_blocks_) - 1;

    // bdev_block_alloc_meta_logical_end_block_ is deduced as follows
    this->bdev_block_alloc_meta_logical_end_block_ =
        (this->bdev_block_alloc_meta_logical_start_block_ +
            this->block_allocator_meta_logical_num_blocks_) - 1;
    
    // bdev_user_physical_start_block_ is deduced from the following formula
    this->bdev_user_physical_start_block_ =
        this->bdev_block_alloc_meta_physical_end_block_ + 1;

    // bdev_user_logical_start_block_ is deduced from the following formula
    this->bdev_user_logical_start_block_ =
        this->bdev_block_alloc_meta_logical_end_block_ + 1;

    // bdev_user_physical_end_block_ is deduced from the following formula
    this->bdev_user_physical_end_block_ =
        this->bdev_total_num_physical_blocks_ - 1;

    // bdev_user_logical_end_block_ is deduced from the following formula
    this->bdev_user_logical_end_block_ =
        this->bdev_total_num_logical_blocks_ - 1;

    this->bdev_state_ = Format::DeviceState::OPEN;

    return true;
}

void Formatter::format_bdev_read_super_complete_cb(
        struct spdk_bdev_io *bdev_io, bool success, void *cb_arg) {

    //ReadCallbackPayload *read_cb_payload = (ReadCallbackPayload *)cb_arg;
    dss_super_block_t *read_sb = (dss_super_block_t *)cb_arg;

    // On completion reduce the counter for success
    if (success) {
        Formatter::format_bdev_read_rq_count--;
    } else {
        // Assert for now
        assert(("ERROR", false));
    }

    if (read_sb == nullptr) {
        assert(("ERROR", false));
    } else {
        std::cout<<"super_block->phy_blk_size_in_bytes = "
            <<read_sb->phy_blk_size_in_bytes<<std::endl;
        assert(read_sb->phy_blk_size_in_bytes ==
                Formatter::written_super_block->phy_blk_size_in_bytes);
        std::cout<<"super_block->logi_blk_size_in_bytes = "
            <<read_sb->logi_blk_size_in_bytes<<std::endl;
        assert(read_sb->logi_blk_size_in_bytes ==
                Formatter::written_super_block->logi_blk_size_in_bytes);
        std::cout<<"super_block->logi_usable_blk_start_addr = "
            <<read_sb->logi_usable_blk_start_addr<<std::endl;
        assert(read_sb->logi_usable_blk_start_addr ==
                Formatter::written_super_block->logi_usable_blk_start_addr);
        std::cout<<"super_block->logi_usable_blk_end_addr = "
            <<read_sb->logi_usable_blk_end_addr<<std::endl;
        assert(read_sb->logi_usable_blk_end_addr ==
                Formatter::written_super_block->logi_usable_blk_end_addr);
        std::cout<<"super_block->logi_blk_alloc_meta_start_blk = "
            <<read_sb->logi_blk_alloc_meta_start_blk<<std::endl;
        assert(read_sb->logi_blk_alloc_meta_start_blk ==
                Formatter::written_super_block->
                    logi_blk_alloc_meta_start_blk);
        std::cout<<"super_block->logi_blk_alloc_meta_end_blk = "
            <<read_sb->logi_blk_alloc_meta_end_blk<<std::endl;
        assert(read_sb->logi_blk_alloc_meta_end_blk == 
                Formatter::written_super_block->logi_blk_alloc_meta_end_blk);
        std::cout<<"super_block->logi_super_blk_start_addr = "
            <<read_sb->logi_super_blk_start_addr<<std::endl;
        assert(read_sb->logi_super_blk_start_addr ==
                Formatter::written_super_block->logi_super_blk_start_addr);
        std::cout<<"super_block->is_blk_alloc_meta_load_needed = "
            <<read_sb->is_blk_alloc_meta_load_needed<<std::endl;
        assert(read_sb->is_blk_alloc_meta_load_needed ==
                Formatter::written_super_block->
                    is_blk_alloc_meta_load_needed);
    }

    // Free all the super block memory allocated
    free(read_sb);
    free(Formatter::written_super_block);


    // Try to exit application
    if (Formatter::format_bdev_write_rq_count == 0 &&
            Formatter::format_bdev_read_rq_count == 0) {
        //spdk_app_stop(0);
        SPDK_NOTICELOG("Invoking formatter stop!\n");
        Formatter::formatter_stop(bdev_io);

    } else {
        spdk_bdev_free_io(bdev_io);
    }

    return;
}

bool Formatter::read_and_print_super_block(
        ReadCallbackPayload *payload) {

    int rc = 0;
    int num_blocks = 0;

    // Since this API is invoked from the write super block
    // callback device is already opened

    // Allocate memory for super block
    dss_super_block_t *super_block =
        (dss_super_block_t *)spdk_dma_zmalloc(
                sizeof(dss_super_block_t), FOUR_KILO_BYTE, NULL);
    memset(super_block, 0, sizeof(dss_super_block_t));

    num_blocks = Formatter::total_super_write_blocks;

    // Mark an in-flight request and trigger operation to bdev
    Formatter::format_bdev_read_rq_count++;
    rc = spdk_bdev_read_blocks(
            payload->desc,
            payload->channel,
            super_block, 
            payload->bdev_super_block_physical_start_block,
            num_blocks,
            Formatter::format_bdev_read_super_complete_cb,
            super_block);

    if (rc != 0) {
        assert(("ERROR", false));
        free(super_block);
        free(Formatter::written_super_block);
        // return false;
    }

    delete payload;

    return true;
}

void Formatter::format_bdev_write_super_complete_cb(
        struct spdk_bdev_io *bdev_io, bool success, void *cb_arg) {

    ReadCallbackPayload *payload = (ReadCallbackPayload *)cb_arg;

    // On completion reduce the counter for success
    if (success) {
        Formatter::format_bdev_write_rq_count--;
    } else {
        // Assert for now
        assert(("ERROR", false));
        // Free memory allocated for super_block
        free(Formatter::written_super_block);
        // Free payload
        delete payload;
    }

    if (payload->debug) {
        // App stop is performed inside 
        SPDK_NOTICELOG("Reading super block from device on debug\n");
        Formatter::read_and_print_super_block(payload);
    } else {
        // Free memory allocated for super_block
        free(Formatter::written_super_block);
        // Free payload
        delete payload;
        // Check for app stop
        if (Formatter::format_bdev_write_rq_count == 0 &&
                Formatter::format_bdev_read_rq_count == 0) {
            Formatter::formatter_stop(bdev_io);
        }
    }
    spdk_bdev_free_io(bdev_io);

    return;
}

void Formatter::format_bdev_write_complete_cb(
        struct spdk_bdev_io *bdev_io, bool success, void *cb_arg) {

    // On completion reduce the counter for success
    if (success) {
        Formatter::format_bdev_write_rq_count--;
    }

    // Check for app stop
    if (Formatter::format_bdev_write_rq_count == 0 &&
            Formatter::format_bdev_read_rq_count == 0) {
        //spdk_app_stop(0);
        SPDK_NOTICELOG("Invoking formatter stop!\n");
        Formatter::formatter_stop(bdev_io);
    } else {
        spdk_bdev_free_io(bdev_io);
    }

    return;
}


bool Formatter::format_device(bool is_debug) {

    int rc = 0;
    int num_blocks = 0;
    //bool is_debug = true;

    // This API assumes that the device is already opened and
    // all relevant details are computed
    if (this->bdev_state_ != DeviceState::OPEN) {
        return false;
    }

    // Allocate memory for callback
    ReadCallbackPayload *cb_payload = new ReadCallbackPayload();
    cb_payload->desc = Formatter::desc;
    cb_payload->channel = Formatter::channel;
    cb_payload->bdev_super_block_physical_start_block =
        this->bdev_super_block_physical_start_block_;
    cb_payload->debug = is_debug;


    // Build and write super block
    dss_super_block_t *super_block =
        (dss_super_block_t *)spdk_dma_zmalloc(
                sizeof(dss_super_block_t), FOUR_KILO_BYTE, NULL);
    memset(super_block, 0, sizeof(dss_super_block_t));
    super_block->phy_blk_size_in_bytes =
        this->bdev_physical_block_size_;
    super_block->logi_blk_size_in_bytes =
        this->bdev_logical_block_size_;
    super_block->logi_usable_blk_start_addr =
        this->bdev_user_logical_start_block_;
    super_block->logi_usable_blk_end_addr =
        this->bdev_user_logical_end_block_;
    super_block->logi_blk_alloc_meta_start_blk =
        this->bdev_block_alloc_meta_logical_start_block_;
    super_block->logi_blk_alloc_meta_end_blk =
        this->bdev_block_alloc_meta_logical_end_block_;
    super_block->is_blk_alloc_meta_load_needed = 0;
    super_block->logi_super_blk_start_addr =
        this->bdev_super_block_logical_start_block_;

    num_blocks = sizeof(dss_super_block_t)/this->bdev_physical_block_size_;
    Formatter::total_super_write_blocks = num_blocks;

    // Mark an in-flight request and trigger operation to bdev
    // Write superblock
    Formatter::format_bdev_write_rq_count++;
    // Assign this super block to written super block to verify during
    // debug.
    // Memory is freed during callback
    Formatter::written_super_block = super_block;
    //free(super_block);
    rc = spdk_bdev_write_blocks(
            Formatter::desc,
            Formatter::channel,
            super_block,
            this->bdev_super_block_physical_start_block_,
            num_blocks,
            Formatter::format_bdev_write_super_complete_cb,
            cb_payload);

    if (rc != 0) {
        free(super_block);
        Formatter::written_super_block = nullptr;
        delete cb_payload;
        assert(("ERROR", false));
        // return false;
    }

    // Mark an in-flight request and trigger operation to bdev
    // Write block allocator meta-data (initialized to zero)
    Formatter::format_bdev_write_rq_count++;
    rc = spdk_bdev_write_zeroes_blocks(
            Formatter::desc,
            Formatter::channel,
            this->bdev_block_alloc_meta_physical_start_block_,
            this->bdev_block_alloc_meta_physical_end_block_,
            Formatter::format_bdev_write_complete_cb,       
            nullptr);

    if (rc !=0) {
        assert(("ERROR", false));
        // return false;
    }

    // Format complete
    return true;
}

bool Formatter::is_format_complete() {
    if (Formatter::format_bdev_write_rq_count == 0) {
        return true;
    } else {
        return false;
    }
}

}// end Namespace Format

