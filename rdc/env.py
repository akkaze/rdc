"""
Core enviroment variable related utilities.

Author: Ankun Zheng
"""
import ctypes
from rdc import _LIB


def find(key):
    proto = ctypes.PYFUNCTYPE(ctypes.py_object, ctypes.py_object)
    func = proto(('RdcEnvFind', _LIB))
    return func(key)


def get_env(key, default_val):
    proto = ctypes.PYFUNCTYPE(ctypes.c_int, ctypes.py_object, ctypes.c_int)
    func = proto(('RdcEnvGetEnv', _LIB))
    return func(key, default_val)


def get_int_env(key):
    proto = ctypes.PYFUNCTYPE(ctypes.c_int, ctypes.py_object)
    func = proto(('RdcEnvGetIntEnv', _LIB))
    return _LIB.RdcEnvGetIntEnv(key)
