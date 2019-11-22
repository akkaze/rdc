#!/usr/bin/env
"""
demo python script of broadcast
"""
from __future__ import print_function
import os

rdc.init()
n = 3
rank = rdc.get_rank()
s = None
if rank == 0:
    s = {'hello world':100, 2:3}
print('@node[%d] before-broadcast: s=\"%s\"' % (rank, str(s)))
s = rdc.broadcast(s, 0)

print('@node[%d] after-broadcast: s=\"%s\"' % (rank, str(s)))
rdc.finalize()
