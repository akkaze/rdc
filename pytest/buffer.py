#!/usr/bin/env python
import rdc
import ctypes
import numpy as np
arr = np.random.random_sample((200))
arr = arr.astype(np.float32)
s = 'a'
b = rdc.Buffer(s.encode())
print(s.encode())
print(b.bytes())
buffer = rdc.Buffer(arr)
print(buffer)
arr = np.array(buffer)
#print(arr)
