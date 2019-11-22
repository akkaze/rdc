#!/usr/bin/env python
"""
demo python script of allreduce
"""
from __future__ import print_function
from six.moves import range
import os
import sys
import numpy as np
import rdc

rdc.init()
n = 3 if len(sys.argv) < 2 else int(sys.argv[1])

rank = rdc.get_rank()
a = np.zeros(n)
for i in range(n):
    a[i] = rank + n + i
print('@node[%d] before-allreduce: a=%s' % (rank, str(a)))
a = rdc.allreduce(a, rdc.Op.MAX)
print('@node[%d] after-allreduce-max: a=%s' % (rank, str(a)))

for i in range(n):
    a[i] = rank + n + i
a = rdc.allreduce(a, rdc.Op.SUM)
print('@node[%d] after-allreduce-sum: a=%s' % (rank, str(a)))
rdc.finalize()
