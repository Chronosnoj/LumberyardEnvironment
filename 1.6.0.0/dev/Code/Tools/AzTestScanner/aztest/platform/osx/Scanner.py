#
# All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
# its licensors.
#
# For complete copyright and license terms please see the LICENSE at the root of this
# distribution (the "License"). All use of this software is governed by the License,
# or, if provided, by the license below or the license accompanying this file. Do not
# remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#

import subprocess
import logging as lg
import os
logger = lg.getLogger(__name__)

def run(cmd, cwd=None, chain=None):
    """
    Run a command on OSX in a subprocess, optionally chain stdout to a second command
    :type cmd: list
    :type cwd: None or str
    :return: generator, output lines
    """
    logger.info("run cmd=" + ', '.join(cmd) + " cwd={}".format(cwd or "<none>"))

    p0 = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, cwd=cwd)
    if chain:
        p1 = subprocess.Popen(chain, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, stdin = p0.stdout, cwd=cwd)
        proc = p1
    else:
        proc = p0

    while True:
        line = proc.stdout.readline()
        if line != '':
            yield line.strip()
        else:
            return


class Scanner:
    __runner_exe__ = 'AzTestRunner'
    
    def enumerate_modules(self, dir):
        # find only the .dylibs
        cmd = ['find', dir, '-type', 'f', '-name', '*.dylib']
        for f in run(cmd):
            yield f

    def enumerate_executables(self, dir):
        # this command will finds all executables including dynamic libraries
        # so filter out the dynamic libraries
        cmd = ['find', dir, '-type', 'f', '-perm', '+1']
        for path in run(cmd):
            if not path.endswith('.dylib') and not path.endswith('.dll'):
                yield path

    def call(self, filename, method, runner_path, args):
        try:
            dir, fn = os.path.split(filename)

            # run and log output
            for l in run([runner_path, fn, method] + args, cwd=dir):
                logger.info(l)

        except Exception as ex:
            logger.exception()

    def run(self, filename, args):
        try:
            dir, fn = os.path.split(filename)
            fn = "./" + fn  # fix up the filename so it can be executed on osx

            # run and log output
            for l in run([fn] + args, cwd=dir):
                logger.info(l)

        except Exception as ex:
            logger.exception()

    def exports_symbol(self, filename, symbol):
        # C prefix symbol
        # http://stackoverflow.com/questions/2627511/why-do-c-compilers-prepend-underscores-to-external-names
        prefixed_symbol = "_" + symbol

        cmd = ['nm', '-j', filename]
        chain = ['grep', prefixed_symbol]

        found = False
        for l in run(cmd, chain=chain):
            if prefixed_symbol == l:
                return True

        return False

