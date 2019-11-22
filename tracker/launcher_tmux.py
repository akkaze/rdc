#!/usr/bin/env python
"""
submission script, tmux version, for debugging
"""

import argparse
import sys
import os
import signal

import subprocess
from threading import Thread
import signal
import logging
import libtmux

from tracker import tracker
from tracker.args import parse_args

keepalive = """
nrep=0
rc=254
while [ $rc -eq 254 ];
do
    export DMLC_NUM_ATTEMPT=$nrep
    %s
    rc=$?;
    nrep=$((nrep+1));
done
"""
global launcher
launcher = None


class TmuxLauncher(object):

    def __init__(self, args, unknown):
        self.args = args
        self.cmd = ' '.join(args.command) + ' ' + ' '.join(unknown)
        self.server = libtmux.Server()

    def exec_cmd(self, cmd, window, pass_env):

        def export_env(env):
            export_str = ''
            for k, v in env.items():
                export_str += 'export %s=%s;' % (k, v)
            return export_str

        #env = os.environ.copy()
        env = dict()
        for k, v in pass_env.items():
            env[k] = str(v)
        ntrial = 0
        export_str = export_env(env)
        export_str += ';'
        #bash = keepalive % (cmd)
        bash = cmd
        window.panes[0].send_keys(export_str)
        window.panes[0].send_keys(bash)

    def submit(self):

        def mthread_submit(nworker, envs, new_worker=False):
            """
            customized submit script
            """
            procs = {}
            windows = dict()
            for i in range(nworker):
                if i == 0:
                    self.session = self.server.new_session(
                        session_name='tracker')
                    window = self.session.windows[0]
                else:
                    window = self.session.new_window()
                self.exec_cmd(self.cmd, window, envs)
            self.session.attach_session()

        return mthread_submit

    def run(self):
        tracker.submit(
            self.args.num_workers,
            fun_submit=self.submit(),
            new_worker=self.args.new_worker,
            pscmd=self.cmd)


def signal_handler(sig, frame):
    global launcher
    launcher.session.kill_session()
    sys.exit(0)


def main():
    args, unknown = parse_args()
    signal.signal(signal.SIGINT, signal_handler)
    global launcher
    launcher = TmuxLauncher(args, unknown)
    launcher.run()
    signal.pause


if __name__ == '__main__':
    main()
