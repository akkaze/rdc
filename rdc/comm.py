"""
Core communicator utilities.

Author: Ankun Zheng
"""
import ctypes
import rdc
import numpy as np


class WorkComp(object):
    """WorkComp"""
    __slots__ = ('handle', 'own_handle')

    def __init__(self, **kwargs):
        self.handle = ctypes.c_void_p()
        if 'own_handle' in kwargs:
            self.own_handle = kwargs['own_handle']
        else:
            self.own_handle = False

    def __del__(self):
        """__del__"""
        if self.own_handle:
            rdc._LIB.RdcDelWorkCompletion(self.handle)

    def wait(self):
        """wait util work is finished"""
        return rdc._LIB.RdcWorkCompletionWait(self.handle)

    def status(self):
        """report status of this work"""
        return rdc._LIB.RdcWorkCompletionStatus(self.handle)


class Comm(object):
    __slots__ = ('handle', 'own_handle')

    def __init__(self, **kwargs):
        self.handle = ctypes.c_void_p()
        if 'own_handle' in kwargs:
            self.own_handle = kwargs['own_handle']
        else:
            self.own_handle = False

    def isend(self, buf, dest_rank):
        """nonblocking send

        :param buf: buffer to send
        :type buf: rdc.Buffer or numpy.ndarray
        :param dest_rank: rank of destination process
        """
        wc = WorkComp(own_handle=True)
        if isinstance(buf, rdc.Buffer):
            rdc._LIB.RdcISend(
                ctypes.byref(wc.handle), self.handle, buf.handle, dest_rank)
        elif isinstance(buf, np.ndarray):
            rdc_buf = rdc.Buffer(buf=buf)
            rdc._LIB.RdcISend(
                ctypes.byref(wc.handle), self.handle, rdc_buf.handle, dest_rank)
        else:
            raise TypeError('Unsupported type')
        return wc

    def irecv(self, buf, src_rank):
        """nonblocking recv

        :param buf: buffer to recv
        :param src_rank: rank of source process
        """
        wc = WorkComp(own_handle=True)
        if isinstance(buf, rdc.Buffer):
            wc_handle = rdc._LIB.RdcIRecv(self.handle, buf.handle, src_rank)
        elif isinstance(buf, np.ndarray):
            rdc_buf = rdc.Buffer(buf=buf)
            wc_handle = rdc._LIB.RdcIRecv(self.handle, rdc_buf.handle, src_rank)
        else:
            raise TypeError('Unsupported type')
        wc.handle = wc_handle
        return wc


def new_comm(name):
    """new_comm

    :param name:
    """
    comm = Comm()
    if isinstance(name, str):
        rdc._LIB.RdcNewCommunicator(
            ctypes.byref(comm.handle), name.encode('utf-8'))
    elif isinstance(name, bytes):
        rdc._LIB.RdcNewCommunicator(ctypes.byref(comm.handle), name)
    else:
        raise TypeError('name must be a string or bytearray')
    return comm


def get_comm(name='main'):
    """get_comm

    :param name:
    """
    comm = Comm()
    if isinstance(name, str):
        rdc._LIB.RdcGetCommunicator(
            ctypes.byref(comm.handle), name.encode('utf-8'))
    elif isinstance(name, bytes):
        rdc._LIB.RdcGetCommunicator(ctypes.byref(comm.handle), name)
    else:
        raise TypeError('name must be a string or bytearray')
    return comm
