"""
Tracker script for RDC
Implements the tracker control protocol
 - start rdc tracker
 - help nodes to establish links with each other

AnKun Zheng
"""

import sys
import os
import socket
import struct
import subprocess
import time
import random
import inspect
import threading
from threading import Thread
from threading import Lock
from threading import Condition
from enum import Enum
from loguru import logger
from collections import defaultdict
import traceback
import configparser
from tracker import utils
from tracker.topo import TopoHelper
"""
Extension of socket to handle recv and send of special data
"""

logger.add('myapp.log', mode='w')

HEARTBEAT_INTERVAL_MS = 5000


class ExSocket:

    def __init__(self, sock):
        self.sock = sock

    def recvall(self, nbytes):
        res = []
        sock = self.sock
        nread = 0
        while nread < nbytes:
            chunk = self.sock.recv(min(nbytes - nread, 1024))
            nread += len(chunk)
            res.append(chunk)
        return b''.join(res)

    def recvint(self):
        return struct.unpack('@i', self.recvall(4))[0]

    def sendint(self, n):
        return self.sock.send(struct.pack('@i', n))

    def sendstr(self, s):
        size = 0
        size += self.sendint(len(s))
        size += self.sock.send(s.encode())
        return size

    def recvstr(self):
        slen = self.recvint()
        return self.recvall(slen).decode()

    def sendbytes(self, b):
        size = 0
        size += self.sendint(len(b))
        size += self.sock.send(b)
        return size

    def recvbytes(self):
        blen = self.recvint()
        return self.recvall(blen)


class State(Enum):
    CMD = 1
    FIN = 2
    UNKNOWN = 3


class TrackerHandler:

    def __init__(self, sock, tracker, worker_id):
        self.sock = sock
        self.tracker = tracker
        self.worker_id = worker_id
        self.state = State.FIN
        self.cmd = None

        now = time.time()
        self.tracker.last_heartbeat_timepoint[self.worker_id] = now

        self.lock = Lock()
        self.shutdown = False
        self.deamon_thrd = Thread(target=self.deamon)
        self.deamon_thrd.start()

    def handle(self):
        if self.state == State.FIN:
            self.cmd = self.recvstr()
            logger.info('receive cmd {}'.format(self.cmd))
            self.state = State.CMD
        elif self.state == State.CMD:
            if self.cmd == 'print':
                self.handle_print()
            elif self.cmd == 'start':
                self.handle_start()
            elif self.cmd == 'restart':
                self.handle_start(self.cmd)
            elif self.cmd == 'register':
                self.handle_register()
            elif self.cmd == 'barrier':
                self.handle_barrier()
            elif self.cmd == 'exclude':
                self.handle_exclude()
            elif self.cmd == 'unexclude':
                self.handle_unexclude()
            elif self.cmd == 'heartbeat':
                self.handle_heartbeat()
            elif self.cmd == 'checkpoint':
                self.handle_checkpoint()
            elif self.cmd == 'load_checkpoint':
                self.handle_load_checkpoint()
            elif self.cmd == 'shutdown':
                self.shutdown = True
                self.deamon_thrd.join()
                return False
            self.state = State.FIN
            self.cmd = None
        return True

    def handle_start(self, cmd='start'):
        rank = self.recvint()

        if cmd == 'restart':
            logger.info('restart cluster')
            n_new_worker = self.recvint()

            with self.tracker.restart_cond:
                self.tracker.new_node_counter += 1
                if self.tracker.new_node_counter != n_new_worker:
                    self.tracker.restart_cond.wait()
                else:
                    self.tracker.new_node_counter = 0
                    self.tracker.nworker += n_new_worker
                    with self.tracker.node_lock:
                        self.tracker.pending_nodes = n_new_worker
                    self.tracker.restart_cond.notify_all()

        with self.tracker.tracker_lock:
            self.tracker.worker_id_to_ranks[self.worker_id] = rank
            self.addr = self.recvstr()

        with self.tracker.rank_cond:
            self.tracker.rank_counter += 1
            if self.tracker.rank_counter != self.tracker.nworker:
                self.tracker.rank_cond.wait()
            else:
                self.tracker.rank_counter = 0
                # trigger a rank reallocation
                self.tracker.realloc_ranks()
                self.tracker.pending_nodes = 0
                self.tracker.rank_cond.notify_all()

        # sync pending nodes and dead nodes to all workers
        with self.tracker.node_lock:
            self.sendint(len(self.tracker.dead_nodes))
            if len(self.tracker.dead_nodes):
                for d in self.tracker.dead_nodes:
                    self.sendint(d)
        self.sendint(self.tracker.pending_nodes)

        self.rank = self.tracker.worker_id_to_ranks[self.worker_id]
        with self.tracker.rank_cond:
            self.tracker.addrs[self.rank] = self.addr
            if len(self.tracker.addrs) != self.tracker.nworker:
                self.tracker.rank_cond.wait()
            else:
                self.tracker.addr_to_ranks = utils.invert_dict(
                    self.tracker.addrs)
                self.tracker.rank_cond.notify_all()

        # sync ranks of peer nodes which have same host to all worker
        peers_with_same_addr = self.tracker.addr_to_ranks[self.addr]
        self.sendint(len(peers_with_same_addr))
        for peer_with_same_addr in peers_with_same_addr:
            self.sendint(peer_with_same_addr)

        # send world size
        self.sendint(self.tracker.nworker)
        # send rank
        with self.tracker.tracker_lock:
            self.sendint(self.rank)
            num_conn = 0
            num_accept = 0
            for rank, addr in self.tracker.addrs.items():
                if rank < self.rank:
                    num_conn += 1
                elif rank > self.rank:
                    num_accept += 1
            self.sendint(num_conn)
            self.sendint(num_accept)
            for rank, addr in self.tracker.addrs.items():
                if rank < self.rank:
                    self.sendstr(addr)
                    self.sendint(rank)
                elif rank > self.rank:
                    self.sendint(rank)

    def handle_print(self):
        msg = self.recvstr()
        if self.rank != -1:
            msg = 'rank %d: %s ' % (self.rank, msg.strip())
        logger.info(msg)

    '''A distributed lock impletentation, only communicator or group
    with same name can continue, otherwise will be blocked
    '''

    def handle_exclude(self):
        comm = self.recvstr()
        with self.tracker.comm_lock:
            if self.tracker.last_comm != comm:
                if self.tracker.last_comm == None:
                    self.tracker.last_comm = comm
                else:
                    if not self.tracker.comm_added[comm]:
                        self.tracker.pending_comms.add(comm)
                        self.tracker.comm_added[comm] = True
                self.sendstr('exclude_undone')
            else:
                self.sendstr('exclude_done')

    def handle_unexclude(self):
        comm = self.recvstr()
        with self.tracker.comm_cond:
            self.tracker.lock_counter += 1
            if self.tracker.lock_counter != self.tracker.nworker:
                self.tracker.comm_cond.wait()
            else:
                self.tracker.lock_counter = 0
                with self.tracker.comm_lock:
                    if len(self.tracker.pending_comms):
                        self.tracker.last_comm = self.tracker.pending_comms.pop(
                        )
                    else:
                        self.tracker.last_comm = None
                self.tracker.comm_cond.notify_all()
        self.sendstr('unexclude_done')

    def handle_barrier(self):
        name = self.recvstr()
        with self.tracker.name_to_barrier_cond[name]:
            self.tracker.name_to_barrier_counter[name] += 1
            if self.tracker.name_to_barrier_counter[name] != \
                    self.tracker.nworker:
                self.tracker.name_to_barrier_cond[name].wait()
            else:
                self.tracker.name_to_barrier_counter[name] = 0
                self.tracker.name_to_barrier_cond[name].notify_all()
        self.sendstr("barrier_done")

    def handle_register(self):
        name = self.recvstr()
        with self.tracker.register_lock:
            if name not in self.tracker.names:
                self.tracker.names.add(name)
                self.tracker.name_to_ranks[name] = set()
                self.tracker.name_to_barrier_counter[name] = 0
                self.tracker.name_to_barrier_cond[name] = Condition()
                self.tracker.name_to_barrier_lock[name] = Lock()
                self.tracker.comm_added[name] = False

            self.tracker.name_to_ranks[name].add(self.rank)

    '''keep heartbeat'''

    def handle_heartbeat(self):
        now = time.time()
        with self.lock:
            self.tracker.last_heartbeat_timepoint[self.worker_id] = now
        self.sendstr('heartbeat_done')
        with self.tracker.node_lock:
            self.sendint(len(self.tracker.dead_nodes))
            if len(self.tracker.dead_nodes):
                for d in self.tracker.dead_nodes:
                    self.sendint(d)
        self.sendint(self.tracker.pending_nodes)

    def handle_checkpoint(self):
        self.tracker.checkpoints[self.rank] = self.recvbytes()

    def handle_load_checkpoint(self):
        if self.rank in self.tracker.checkpoints:
            logger.info('{}'.format(len(self.tracker.checkpoints[self.rank])))
            self.sendbytes(self.tracker.checkpoints[self.rank])
        else:
            logger.warning('Please ensure that you alreay checkpoint')

    def deamon(self):
        while not self.shutdown:
            time.sleep(HEARTBEAT_INTERVAL_MS / 1000.0)
            now = time.time()
            with self.lock:
                last_heartbeat_timepoint = self.tracker.last_heartbeat_timepoint[
                    self.worker_id]
            if now - last_heartbeat_timepoint > 2 * HEARTBEAT_INTERVAL_MS:
                with self.tracker.node_lock:
                    self.tracker.dead_nodes.add(self.rank)

    def recvint(self):
        return self.sock.recvint()

    def recvstr(self):
        return self.sock.recvstr()

    def recvbytes(self):
        return self.sock.recvbytes()

    def sendint(self, data):
        return self.sock.sendint(data)

    def sendstr(self, data):
        return self.sock.sendstr(data)

    def sendbytes(self, data):
        return self.sock.sendbytes(data)


class Tracker:

    def __init__(self, host_ip, port, nworker):
        self.cur_rank = 0
        # trakcer addr
        self.host_ip = host_ip
        self.port = port
        self.nworker = nworker
        # create track sock then lisen
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.bind((host_ip, port))
        self.sock.listen(128)
        self.addr_to_ranks = defaultdict(list)
        self.addrs = dict()

        # communicator name associated members
        # register related
        self.names = set()
        self.name_to_ranks = dict()
        self.register_lock = Lock()
        self.last_rank = 0

        self.worker_id_to_ranks = dict()
        self.rank_cond = threading.Condition()
        self.rank_counter = 0
        # thread associated members
        self.tracker_lock = Lock()
        self.name_lock = Lock()

        # barrier related
        self.name_to_barrier_counter = dict()
        self.name_to_barrier_cond = dict()
        self.name_to_barrier_lock = dict()
        # exclude related
        self.last_comm = None
        self.pending_comms = set()
        self.comm_added = dict()
        self.comm_lock = Lock()

        # heartbeat related
        self.last_heartbeat_timepoint = dict()
        self.lock_counter = 0
        self.comm_cond = Condition()

        # checkpoint related
        self.checkpoints = dict()
        # construct initial tree map
        self.topohelper = TopoHelper()
        self.tree_map, self.parent_map, self.ring_map = \
            self.topohelper.get_link_map(nworker)
        self.node_lock = Lock()
        self.dead_nodes = []
        self.pending_nodes = 0

        self.restart_cond = Condition()
        self.new_node_counter = 0

        self.listen_thread = Thread(target=self.listen, args=())
        self.listen_thread.start()

        self.threads = dict()

    def listen(self):

        def run(worker_id, fd):
            sock = ExSocket(fd)
            handler = TrackerHandler(sock, self, worker_id)
            ret = True
            while ret:
                ret = handler.handle()

        logger.info('start listen on %s:%d' % (self.host_ip, self.port))
        worker_id = 0
        while True:
            fd, s_addr = self.sock.accept()
            logger.info('accept a connection from {0}'.format(s_addr))
            thread = Thread(target=run, args=(worker_id, fd))
            self.threads[worker_id] = thread
            thread.setDaemon(True)
            thread.start()
            worker_id += 1

    def realloc_ranks(self):
        existing_ranks = set()
        for worker_id, rank in self.worker_id_to_ranks.items():
            if rank != -1:
                existing_ranks.add(rank)
        last_rank = 0
        for worker_id, rank in self.worker_id_to_ranks.items():
            if rank != -1:
                continue
            else:
                while last_rank in existing_ranks:
                    last_rank += 1
                self.worker_id_to_ranks[worker_id] = last_rank
                last_rank += 1

    def join(self):
        for thread in self.threads.values():
            while thread.isAlive():
                thread.join(100)
        while self.listen_thread.isAlive():
            self.listen_thread.join(100)


def submit(nworker,
           fun_submit,
           new_worker=False,
           host_ip='auto',
           port=-1,
           pscmd=None):
    """submit job

    Paramaters
    ----------
    nworker : int
        number of workers
    fun_sumbit : func
        the function to submit the jobs for servers and workers
    host_ip : str, optional
        the host ip of the root node
    pscmd :
    """
    # start the root
    if not new_worker:
        host_ip, port, envs = utils.basic_tracker_config(host_ip)
        tracker = Tracker(host_ip=host_ip, port=port, nworker=nworker)

    else:
        logger.info("connect to tracker at {0}@{1}".format(host_ip, port))
        _, _, envs = utils.basic_tracker_config(host_ip)
        assert host_ip != 'auto' and port != -1
    common_envs = {
        'RDC_TRACKER_URI': host_ip,
        'RDC_TRACKER_PORT': port,
        'RDC_HEARTBEAT_INTERVAL': HEARTBEAT_INTERVAL_MS,
        'RDC_SHMEM_SIZE': 1024,
    }

    envs.update(common_envs)
    if new_worker:
        envs['RDC_RESTART'] = 1
        envs['RDC_PENDING_NODES'] = nworker
    # start the workers
    fun_submit(nworker, envs, new_worker)

    if not new_worker:
        # wait the root finished
        tracker.join()
