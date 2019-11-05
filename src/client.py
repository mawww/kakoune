#!/usr/bin/env python3

import ctypes
import os.path
import sys
import time
from hashlib import md5

from grope import rope

int_size = ctypes.sizeof(ctypes.c_int)
base = rope()
lens = []


def read_modifications(file):
    def read_int():
        st = 0.0
        data = file.read(int_size)
        while len(data) == 0:
            if st < 0.25:
                st += 0.01
            time.sleep(st)
            data = file.read(int_size)
        return int.from_bytes(data, sys.byteorder)

    head = (read_int(), (read_int(), read_int()), read_int())
    mod = (head, file.read(head[2]))
    return mod


def update_modifaction(mod):
    global base
    global lens
    head = mod[0]
    data = mod[1]
    data_len = len(data)
    line = head[1][0]
    column = head[1][1]
    offset = 0
    for i in range(line):
        offset += lens[i]
    split = offset + column
    if head[0] == 0:
        base = rope(base[:split], data, base[split:])
    else:
        base = rope(base[:split], base[data_len + split :])

    # TODO remove need to materialize
    materialized = bytes(base)
    lens = [len(line) + 1 for line in materialized.split(b"\n")]
    print(chr(27) + "[2J")
    print(materialized.decode("UTF-8"))


client = sys.argv[1]
id_ = md5(os.path.abspath(sys.argv[2]).encode("UTF-8")).hexdigest().upper()
file_ = f"/dev/shm/kakoune_{client}_{id_}"

with open(file_, "rb") as f:
    while True:
        mod = read_modifications(f)
        update_modifaction(mod)
