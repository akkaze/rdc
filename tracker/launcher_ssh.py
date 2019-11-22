#!/usr/bin/env python
"""
submission script by ssh

One need to make sure all wokers machines are ssh-able.
"""

import argparse
import sys
import os
import subprocess
import logging
from threading import Thread

from tracker import tracker
from tracker.args import parse_args, parse_config_file


class SSHLauncher(object):

    def __init__(self, args, unknown):
        self.args = args
        self.cmd = (' '.join(args.command) + ' ' + ' '.join(unknown))

        if args.num_workers is not None:
            self.num_workers = args.num_workers
        else:
            assert args.config_file is not None
        self.init_cmds, self.common_envs, self.worker_envs_by_host = parse_config_file(
            args.config_file)
        hosts = self.worker_envs_by_host.keys()
        self.num_workers = len(hosts)
        self.hosts = []
        for h in hosts:
            if len(h.strip()) > 0:
                self.hosts.append(h.strip())

    def sync_dir(self, local_dir, woker_node, woker_dir):
        """
        sync the working directory from root node into woker node
        """
        remote = woker_node + ':' + woker_dir
        logging.info('rsync %s -> %s', local_dir, remote)

        # TODO uses multithread
        prog = 'rsync -az --rsh="ssh -o StrictHostKeyChecking=no" %s %s' % (
            local_dir, remote)
        subprocess.check_call([prog], shell=True)

    def get_env(self, pass_envs):
        envs = []
        # get system envs
        keys = ['LD_LIBRARY_PATH', 'TERM']
        for k in keys:
            v = os.getenv(k)
            if v is not None:
                envs.append('export ' + k + '=' + v + ';')
        # get ass_envs
        for k, v in pass_envs.items():
            envs.append('export ' + str(k) + '=' + str(v) + ';')
        return (' '.join(envs))

    def submit(self):

        def ssh_submit(nworker, pass_envs, new_worker=False):
            """
            customized submit script
            """

            # thread func to run the job
            def run(prog):
                subprocess.check_call(prog, shell=True)

            # sync programs if necessary
            local_dir = os.getcwd() + '/'
            working_dir = local_dir
            if self.args.sync_dir is not None:
                working_dir = self.args.sync_dir
                for h in self.hosts:
                    self.sync_dir(local_dir, h, working_dir)

            # launch jobs
            for i in range(nworker):
                host = self.hosts[i % len(self.hosts)]
                node = host.split(':')[0]
                pass_envs.update(self.common_envs)
                pass_envs.update(self.worker_envs_by_host[host])
                prog = self.init_cmds + ';'
                prog += self.get_env(
                    pass_envs) + ' cd ' + working_dir + '; ' + self.cmd
                prog = 'ssh -o StrictHostKeyChecking=no ' + node + ' \'' + prog + '\''

                thread = Thread(target=run, args=(prog,))
                thread.setDaemon(True)
                thread.start()

        return ssh_submit

    def run(self):
        tracker.submit(
            self.num_workers, fun_submit=self.submit(), pscmd=self.cmd)


def main():
    args, unknown = parse_args()

    launcher = SSHLauncher(args, unknown)
    launcher.run()


if __name__ == '__main__':
    main()
