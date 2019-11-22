#!/usr/bin/env python
import rdc
import numpy as np

rdc.init()
comm = rdc.new_comm('main')
if rdc.get_rank() == 0:
    s = 'hello'
    buf = rdc.Buffer(s.encode())
    comm.send(buf, 1)
elif rdc.get_rank() == 1:
    s = '00000'
    buf = rdc.Buffer(s.encode())
    comm.recv(buf, 0)
    print(buf.bytes())
rdc.finalize()
