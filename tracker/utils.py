"""
basic utilities from tracker and luancher
"""

import logging
import socket
from loguru import logger

def basic_tracker_config(nworker, host_ip='auto', pscmd=None):
    """basic tracker configurations

    Paramaters
    ----------
    nworker : int
        number of workers
    host_ip : str, optional
        the host ip of the root node
    pscmd :
    """
    # get the root node ip
    if host_ip == 'auto':
        host_ip = 'ip'
    if host_ip == 'dns':
        host_ip = socket.getfqdn()
    elif host_ip == 'ip':
        from socket import gaierror
        try:
            host_ip = socket.gethostbyname(socket.getfqdn())
        except gaierror:
            logger.warn('gethostbyname(socket.getfqdn()) failed...'\
                    ' trying on hostname()')
            host_ip = socket.gethostbyname(socket.gethostname())
        if host_ip.startswith("127."):
            s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            # doesn't have to be reachable
            s.connect(('10.255.255.255', 0))
            host_ip = s.getsockname()[0]
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.bind((host_ip, 0))
    port = s.getsockname()[1]
    envs = {
        'RDC_NUM_WORKERS': nworker,
        'RDC_BACKEND': 'TCP',
        'RDC_RDMA_BUFSIZE': 1 << 25
    }
    return host_ip, port, envs


def hash_combine(values=[]):
    ret = 0
    for value in values:
        ret ^= hash(value) + 0x9e3779b9 + (ret << 6) + (ret >> 2)
    return ret


def build_addr(backend, host, port):
    return "%s:%s:%d" % (backend, host, port)


def config_logger(args):
    FORMAT = '[%(asctime)s (%(name)s:%(lineno)s) %(levelname)s] %(message)s'
    level = args.log_level if 'log_level' in args else 'DEBUG'
    level = eval('logging.' + level)
    if 'log_file' not in args or args.log_file is None:
        logging.basicConfig(format=FORMAT, level=level)
    else:
        logging.basicConfig(
            format=FORMAT, level=level, filename=args.log_file, filemode='w')
        console = logging.StreamHandler()
        console.setFormatter(logging.Formatter(FORMAT))
        console.setLevel(level)
        logging.getLogger('').addHandler(console)
