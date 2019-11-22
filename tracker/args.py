import argparse
import configparser
import psutil
import socket


def parse_args():
    parser = argparse.ArgumentParser(description='script to submit jobs')

    parser.add_argument(
        '-n',
        '--num-workers',
        type=int,
        help='number of worker nodes to be launched')
    parser.add_argument(
        '-f',
        '--config-file',
        default='config/common.ini',
        type=str,
        help='configuration file')
    parser.add_argument(
        '--log-level',
        default='INFO',
        type=str,
        choices=['INFO', 'DEBUG'],
        help='logging level')
    parser.add_argument(
        '--log-file',
        type=str,
        default='myapp.log',
        help='output log to the specific log file')
    parser.add_argument(
        '--sync-dir',
        type=str,
        help='directory contains files shared by all workers')
    parser.add_argument(
        '--hostfile',
        type=str,
        default='hostfile',
        help='hostfile cantains all host on which porgram will be executed')
    parser.add_argument(
        'command', nargs='+', help='command for launching the program')
    args, unknown = parser.parse_known_args()
    return args, unknown


def get_ip_addrs(family):
    for interface, snics in psutil.net_if_addrs().items():
        for snic in snics:
            if snic.family == family:
                yield (interface, snic.address)


def get_ip_addr_from_hostname(hostname):
    return socket.gethostbyname(hostname)


def parse(value):
    if ',' in value:
        return value.split(',')
    else:
        return value


def parse_config_file(config_file_path):
    parser = configparser.ConfigParser()
    parser.read(config_file_path)
    sections = parser.sections()
    common_envs = dict()
    for name, value in parser.items('common'):
        common_envs[name] = value
    worker_envs_by_host = dict()
    for sec in sections:
        if sec == 'common':
            continue
        else:
            host = sec
            worker_envs_by_host[host] = dict()
            for name, value in parser.items(sec):
                worker_envs_by_host[host][name] = value

    return common_envs, worker_envs_by_host
