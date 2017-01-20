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

import argparse
from datetime import datetime
import logging as lg
import os
import shutil
from aztest.scanner import scan, get_output_dir
from aztest.log import setup_logging
logger = lg.getLogger(__name__)


def get_parser():
    parser = argparse.ArgumentParser(description="AZ Test Scanner")
    subparsers = parser.add_subparsers()

    p_scan = subparsers.add_parser("scan", help="scans a directory for modules to test and executes them",
                                   epilog="Extra parameters are assumed to be for the test framework and will be "
                                          "passed along to the modules/executables under test. Returns 1 if any "
                                          "module has failed (for any reason), or zero if all tests in all modules "
                                          "have passed.")
    p_scan.add_argument('--dir', '-d', required=True, help="root directory to scan")
    p_scan.add_argument('--runner-path', '-r', required=False,
                        help="path to the test runner, default is {dir}/AzTestRunner(.exe)")
    p_scan.add_argument('--add-path', required=False, action="append",
                        help="path(s) to libraries that are required by modules under test")
    p_scan.add_argument('--output-path', required=False,
                        help="sets the path for output folders, default is dev/TestResults")
    p_scan.add_argument('--integ', '-i', required=False, action="store_true",
                        help="if set, runs integration tests instead of unit tests")
    p_scan.add_argument('--no-timestamp', required=False, action="store_true",
                        help="if set, removes timestamp from output files")
    p_scan.add_argument('--wait-for-debugger', required=False, action="store_true",
                        help="if set, tells the AzTestRunner to wait for a debugger to be attached before continuing")
    p_scan.add_argument('--bootstrap-config', required=False,
                        help="json configuration file to bootstrap applications for modules which require them")

    g_filters = p_scan.add_argument_group('Filters', "Arguments for filtering what is scanned")
    g_filters.add_argument('--limit', '-n', required=False, type=int,
                           help="limit number of modules to scan")
    g_filters.add_argument('--only', '-o', required=False,
                           help="scan only the file(s) named in this argument (comma separated), matches using "
                                "'endswith'")
    g_filters.add_argument('--whitelist', required=False, choices=['none', 'spec', 'file'], default='spec',
                           help="tells scanner what kind of whitelist file to use, if any, default is 'spec'")
    g_filters.add_argument('--whitelist-file', required=False, metavar='FILE',
                           help="path to a spec file or generic, newline-delimited file used for whitelisting modules; "
                                "the default spec file is _WAF_/specs/all.json")
    g_filters.add_argument('--include-gems', '-g', required=False, action="store_true",
                           help="if set, tells scanner to find and include gems in the whitelist")
    g_filters.add_argument('--include-projects', '-p', required=False, action="store_true",
                           help="if set, tells scanner to find and include game projects in the whitelist")
    g_filters.add_argument('--blacklist-file', required=False, metavar='FILE',
                           help="path to a generic, newline-delimited file used for blacklisting modules")
    g_filters.add_argument('--exe', required=False, action="store_true",
                           help="if set, scans executables as well as shared modules")

    p_scan.set_defaults(func=scan)

    return parser


def execute(args=None):
    parser = get_parser()

    args, extra = parser.parse_known_args(args)

    # create output directory
    timestamp = datetime.now() if not args.no_timestamp else None
    if args.output_path:
        output_dir = get_output_dir(timestamp, os.path.abspath(args.output_path))
    else:
        output_dir = get_output_dir(timestamp)
    if os.path.exists(output_dir):
        shutil.rmtree(output_dir)
    os.makedirs(output_dir)

    # setup logging
    setup_logging(os.path.join(output_dir, "aztest.log"), lg.DEBUG)
    logger.info("AZ Test Scanner")

    # execute command
    return args.func(args, extra, output_dir)