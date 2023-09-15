import pandas as pd
import re
import pdb
import subprocess
import matplotlib.pyplot as plt
import matplotlib
from pathlib import Path
import argparse

font = {'weight': 'bold',
        'size': 22}

matplotlib.rc('font', **font)


class spdk_trace_parser():

    def __init__(self, type, path, parser_fn) -> None:
        self.type = type
        self.path = path
        self.parser_fn = parser_fn
        self._max_words = 0
        self.states_in_order = []
        self.df = None
        self.parse_trace_to_df()
        self.get_states_in_order()

    def get_trace_text(self) -> list:
        trace_lines = []
        if not self.path.exists():
            raise Exception("failed to load trace.")
        with self.path.open() as file:
            lines = [line.rstrip() for line in file]
            for line in lines:
                if self.type in line:
                    trace_lines.append(line)
            return trace_lines

    def parse_trace_to_df(self):
        lines = self.get_trace_text()
        self.df = self.parser_fn(lines)

    def get_states_in_order(self):
        if len(self.states_in_order) != 0:
            return self.states_in_order
        if len(self.df) == 0:
            raise Warning("Call self.parse_trace_to_df() first.")
        # get any valid req and find all of its states in order.
        req_id = self.df.id.drop_duplicates().values[0]
        req_df = self.df[self.df.id == req_id]
        req_df = req_df.sort_values(by="tsc_us")
        self.states_in_order = req_df["state"].tolist()

# def get_spdk_trace_for_core(pid: int, lcore: int):
#     cmd = ["./df_out/oss/spdk_tcp/build/bin/spdk_trace",
#            "-f", "/dev/shm/nvmf_trace.pid{}".format(pid), "-c",
#            "{}".format(lcore)]
#     result = subprocess.run(cmd, stdout=subprocess.PIPE)
#     return result.stdout.decode('utf-8')


def parse_line_to_words(line: str):
    words = re.split(' ', line)
    words = [w for w in words if w != '']
    return words


def init_general_trace_df():
    return pd.DataFrame(None, columns=["lcore", "tsc_us", "state", "dreq_ptr", "id"])


def init_kv_df():
    return init_general_trace_df()


def parse_general_dss_df(lines: list):
    df = pd.DataFrame([parse_line_to_words(l) for line in lines],
                      columns=["lcore", "tsc_us", "state", "dreq_ptr", "dum1", "id", "dum2", "time"])
    df.tsc_us = df.tsc_us.astype(float)
    df.dreq_ptr = df.dreq_ptr.apply(lambda x: x.split("0x")[1])
    return df[["lcore", "tsc_us", "state", "dreq_ptr", "id", "time"]]


def parse_kv_df(lines: list):
    # 5: 4870708.000    KVT_KEY_IO_CMPL   kreq_id0x79b1   id:  k1490    time:  16.704
    return parse_general_dss_df(lines)


def parse_net_df(lines: list):
    # 72:  40309.543     NET_DEQUEUE_KREQ   dss_req0x7ffe23433100   id:  n25    time:  23278.518
    return parse_general_dss_df(lines)


def parse_io_task_df(lines: list):
    # 72:  40309.543     NET_DEQUEUE_KREQ   dss_req0x7ffe23433100   id:  n25    time:  23278.518
    return parse_general_dss_df(lines)


def parse_bdev_df(lines: list):
    # 76:  36995.652 b00 BDEV_IO_START       type:  2      id:    i23
    # 76:  37014.527 b00 BDEV_IO_DONE                      id:    i22     time:  19.810
    start_df = pd.DataFrame([parse_line_to_words(l) for line in lines if "START" in l],
                            columns=["lcore", "tsc_us", "dum0", "state", "dum1", "type", "dum2", "id"])
    stop_df = pd.DataFrame([parse_line_to_words(l) for line in lines if "DONE" in l],
                           columns=["lcore", "tsc_us", "dum0", "state", "dum1", "id", "dum2", "time"])
    df = pd.concat([start_df, stop_df])
    df.tsc_us = df.tsc_us.astype(float)
    return df[["lcore", "tsc_us", "state", "type", "id", "time"]]


def merge_df_from_spdk_parsers(parsers: list):
    full_df = pd.concat([p.df for p in parsers])
    full_df = full_df.sort_values(by="tsc_us", ignore_index=True)

    full_df.lcore = full_df.lcore.apply(lambda x: x.split(":")[0]).astype(int)
    return full_df


def get_latency_between_two_states_for_id(df, id: str, s1: str, s2: str):
    req_df = df[df["id"] == id]
    return abs(req_df.loc[req_df.state == s1, "tsc_us"].item() - req_df.loc[req_df.state == s2, "tsc_us"].item())


def _replace_duplicated_column_name(columns):
    offset = 0
    index = 0
    appear_times = {}
    for i in range(len(columns)):
        if i == len(columns) - 1:
            break
        offset = i + 1
        while columns[i] in columns[offset:]:
            if columns[i] not in appear_times:
                appear_times[columns[i]] = 1
            else:
                appear_times[columns[i]] += 1
            index = columns[offset:].index(columns[i])
            columns[index + offset] += "_{}".format(appear_times[columns[i]])
            if index == len(columns) - 1:
                break
            offset = offset + index + 1

    return columns


def generate_breakdown_latency_tbl(df, init_state: str, end_state: str):
    state_num = 0
    bl_df = None
    s_idx = 0
    e_idx = 0
    tsc_ms = 0
    req_ids = df.dreq_ptr.drop_duplicates().values
    for req in req_ids:
        req_df = df[df.dreq_ptr == req]
        req_df = req_df.reset_index(drop=True)
        for index in req_df.index:
            if req_df.loc[index, "state"] == init_state:
                s_idx = index
                tsc_ms = int(req_df.loc[index, "tsc_us"] / 1e3)
            elif req_df.loc[index, "state"] == end_state:
                e_idx = index
                if state_num == 0:
                    state_num = e_idx - s_idx
                    if bl_df is None:
                        columns = req_df.loc[s_idx:e_idx,
                                             "state"].values.tolist()
                        if len(columns) != len(set(columns)):
                            columns = _replace_duplicated_column_name(columns)
                        bl_df = pd.DataFrame(
                            None, columns=(columns + ["tsc_ms"]))
                bl_row = [0] + (req_df.loc[s_idx + 1:e_idx, "tsc_us"].values
                                - req_df.loc[s_idx:e_idx - 1, "tsc_us"].values).tolist() + [tsc_ms]
                if len(bl_row) != state_num + 2:
                    continue
                bl_df.loc[len(bl_df), :] = bl_row
    bl_df = bl_df.sort_values(by="tsc_ms", ignore_index=True)
    bl_df = bl_df.set_index('tsc_ms', drop=True)
    for key in bl_df.keys().values:
        bl_df[key] = bl_df[key].astype(float)
    return bl_df


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        prog='post_process_spdk_trace',
        description='Support post process of spdk traces. Generate statistics/plots for latency')
    parser.add_argument('filename')

    args = parser.parse_args()

    # trace_path = Path("/home/dan.jia/dss-sdk/9207_read.txt")
    trace_path = Path(args.filename)

    kvtp = spdk_trace_parser("KVT", trace_path, parse_kv_df)

    netp = spdk_trace_parser("NET", trace_path, parse_net_df)

    iop = spdk_trace_parser("IO_TASK", trace_path, parse_io_task_df)

    full_df = merge_df_from_spdk_parsers([kvtp, netp, iop])

    bl_df = generate_breakdown_latency_tbl(
        full_df, 'NET_ENQUEUE_KREQ', 'NET_DEQUEUE_KREQ')

    axs = bl_df.plot.area(figsize=(36, 24), subplots=True)
    plt.savefig("latency_{}.pdf".format(trace_path.stem))

    plt.clf()
    axs = bl_df.plot.box(figsize=(24, 16))
    axs.grid(True)
    axs.set_yscale('log')
    axs.set_ylabel("latency (us)")
    plt.xticks(rotation=30, ha='right')
    plt.tight_layout()
    plt.savefig("box_{}.pdf".format(trace_path.stem))

    print("The ave. time (us) for each state :")
    print(bl_df.mean(axis=0))
