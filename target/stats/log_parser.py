# import sqlite3
import os
import time
import json

g_license_text = """
 #   The Clear BSD License
 #
 #   Copyright (c) 2022 Samsung Electronics Co., Ltd.
 #   All rights reserved.
 #
 #   Redistribution and use in source and binary forms, with or without
 #   modification, are permitted (subject to the limitations in the
 #   disclaimer below) provided that the following conditions are met:
 #
 #   	* Redistributions of source code must retain the above copyright
 #   	  notice, this list of conditions and the following disclaimer.
 #   	* Redistributions in binary form must reproduce the above copyright
 #   	  notice, this list of conditions and the following disclaimer in
 #   	  the documentation and/or other materials provided with the distribution.
 #   	* Neither the name of Samsung Electronics Co., Ltd. nor the names of its
 #   	  contributors may be used to endorse or promote products derived from
 #   	  this software without specific prior written permission.
 #
 #   NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED
 #   BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 #   CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
 #   BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 #   FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 #   COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 #   INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 #   NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 #   USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 #   ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 #   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 #   THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
"""

name_map = {"f": "logged_fn",
            "p": "cpu_id",
            "s": "complete",
            "k": "key"}


def file_search(file_dir):
    dump_file_list = []
    for root, dirs, files in os.walk(file_dir):
        for name in sorted(files):
            if name.isdigit():
                dump_file_list.append(os.path.join(root, name))
    # print(json.dumps(dump_file_dict, sort_keys=True, indent=4))
    return dump_file_list


def collect_data(file_path, log_filter):
    try:
        file_obj = open(file_path, "r")
    except OSError as e:
        # print("Error: collect_data open %s - %s\n" % (file_path, e.message))
        # log.error("'kv-stats %s' raised an exception: %s" % (file_path, e.message))
        # return dump_file_dict
        return None
    file_content = file_obj.readlines()
    file_obj.close()

    print("%s Lines: %d" % (file_path, len(file_content)))

    corrupted_log_entries = 0
    time_start = time.time()
    start_log_info = []
    exit_log_info = []
    for line in file_content:
        if line[-1] == '\n':
            line = line[0:-1]
        entry_arr = line.split(':')
        temp_dict = {}
        temp_dict['timestamp'] = entry_arr[0]
        for entry in entry_arr[1:]:
            temp = entry.split(',')
            # print("file_content " + str(file_content))
            # print("entry >%s<" % entry)
            try:
                temp_dict[name_map[str(temp[0])]] = {}
                temp_dict[name_map[str(temp[0])]] = str(temp[1])
            except:
                print("Corruption: entry_arr " + str(entry_arr))
                corrupted_log_entries = corrupted_log_entries + 1
        temp_dict.setdefault('complete')
        try:
            if temp_dict['complete'] == '0':
                start_log_info.append(temp_dict)
            else:
                exit_log_info.append(temp_dict)
        except:
            # print("Corruption: Missing entry/exit var")
            corrupted_log_entries = corrupted_log_entries + 1
            continue
    print('Parsing took                   %0.3f ms' % ((time.time() - time_start) * 1000.0))
    # print(json.dumps(start_log_info, sort_keys=False, indent=4))
    # print(json.dumps(exit_log_info, sort_keys=False, indent=4))

    # conn = sqlite3.connect('example.db')
    # c = conn.cursor()
    # c.execute("CREATE TABLE IF NOT EXISTS dfly_logs (timestamp text, logged_fn text, key text, cpu_id int, complete int)")

    time_start = time.time()
    log_info_arr = (start_log_info, exit_log_info)
    for n in range(0, 2):
        for info in log_info_arr[n]:
            # print(info)
            try:
                if info['logged_fn'] is not None:
                    log_filter.setdefault(info['logged_fn'])
                    if log_filter[info['logged_fn']] is None:
                        log_filter[info['logged_fn']] = {}

                    log_filter[info['logged_fn']].setdefault(info['key'])
                    if log_filter[info['logged_fn']][info['key']] is None:
                        log_filter[info['logged_fn']][info['key']] = {}
                        log_filter[info['logged_fn']][info['key']]['entry_sum'] = float(0)
                        log_filter[info['logged_fn']][info['key']]['entry_cnt'] = 0
                        log_filter[info['logged_fn']][info['key']]['exit_sum'] = float(0)
                        log_filter[info['logged_fn']][info['key']]['exit_cnt'] = 0

                    if n == 0:
                        log_filter[info['logged_fn']][info['key']]['entry_sum'] = \
                            float(log_filter[info['logged_fn']][info['key']]['entry_sum']) + float(info['timestamp'])
                        log_filter[info['logged_fn']][info['key']]['entry_cnt'] = \
                            log_filter[info['logged_fn']][info['key']]['entry_cnt'] + 1
                    elif n == 1:
                        log_filter[info['logged_fn']][info['key']]['exit_sum'] = \
                            float(log_filter[info['logged_fn']][info['key']]['exit_sum']) + float(info['timestamp'])
                        log_filter[info['logged_fn']][info['key']]['exit_cnt'] = \
                            log_filter[info['logged_fn']][info['key']]['exit_cnt'] + 1
                    else:
                        assert()
            except:
                corrupted_log_entries = corrupted_log_entries + 1
                # if n == 0:
                #     print("Error: Filtering logged_fn entry - %s" % info)
                # elif n == 1:
                #     print("Error: Filtering logged_fn exit - %s" % info)
                # else:
                #     assert()
    print('Filtering entry/exit logs took %0.3f ms' % ((time.time() - time_start) * 1000.0))

    if corrupted_log_entries:
        print("Error: Found %d corrupted log entries in %s" % (corrupted_log_entries, file_path))
    # print(json.dumps(log_filter, sort_keys=False, indent=4))
    return log_filter


def calculate_latency(log_filter):
    for logged_fn in log_filter:
        missing_log_pairs = 0
        cnt = 0.0
        total = 0.0
        for key in log_filter[logged_fn]:
            diff = log_filter[logged_fn][key]['exit_cnt'] - log_filter[logged_fn][key]['entry_cnt']
            if diff != 0:
                # print("Missing log pair - Skipping %s %s" % (logged_fn, key))
                missing_log_pairs = missing_log_pairs + 1
                continue
            assert (diff == 0), "Different values found %d %s %s" % (diff, logged_fn, key)

            delta = (log_filter[logged_fn][key]['exit_sum'] - log_filter[logged_fn][key]['entry_sum']) / log_filter[logged_fn][key]['exit_cnt']
            '''
            delta1 = (log_filter['kvs_wrapper_put_key'][key]['exit_sum'] - log_filter['kvs_wrapper_put_key'][key]['entry_sum']) / \
                    log_filter['kvs_wrapper_put_key'][key]['exit_cnt']
            delta2 = (log_filter['kvv_router_put'][key]['exit_sum'] - log_filter['kvv_router_put'][key]['entry_sum']) / \
                    log_filter['kvv_router_put'][key]['exit_cnt']

            if delta2 < delta1 :
                print("key %s delta1 %.9f delta2 %.9f" % (key, delta1, delta2))
            '''
            total = total + delta
            if delta:
                cnt = cnt + 1
        if cnt:
            calculated = float(total / cnt)
            print("Matching %d log pairs" % cnt)
            print("Missing  %d log pairs" % missing_log_pairs)
            print("%-32s   Average %-10.4f seconds" % (logged_fn, calculated))
            print("                                   Average %-10.4f ms" % (calculated * 1000))
            print("                                   Average %-10.4f us" % (calculated * 1000 * 1000))


if __name__ == "__main__":
    def print_cmd_help():
        print("parse.py help")
        print("parse.py stats path")
    from sys import argv

    filtered_logs = {}
    if len(argv) == 1:
        file_list = \
            file_search("C:\\Users\\kenneth.yip\\Desktop\\GIT_Folders\\CSS_proj\\kvb_kvv_stats\\log_parse\\log\\2017\\7\\11\\15")
        for file in file_list:
            # print(temp[file])
            filtered_logs.update(collect_data(file, filtered_logs))
        # print(json.dumps(filtered_logs, sort_keys=False, indent=4))
        calculate_latency(filtered_logs)
    elif len(argv) == 2:
        file_path_arg = argv[1]
        file_list = file_search(file_path_arg)
        print(file_list)
        for file_path in file_list:
            # print(temp[file])
            filtered_logs.update(collect_data(file_path, filtered_logs))
        calculate_latency(filtered_logs)
    else:
        print_cmd_help()
