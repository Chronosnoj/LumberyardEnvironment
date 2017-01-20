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

import logging as lg
import os
import sys
import json
from aztest.filters import FileApprover
from aztest.report import HTMLReporter
from aztest.errors import RunnerReturnCodes
from collections import namedtuple
from bootstrap import BootstrapConfig
logger = lg.getLogger(__name__)

ScanResult = namedtuple("ScanResult", ("path", "return_code", "xml_path", "error_msg"))

if sys.platform.startswith("darwin"):
    from aztest.platform.osx import Scanner
elif sys.platform.startswith("win32"):
    from aztest.platform.win import Scanner
else:
    logger.error("Unrecognized platform: {}".format(sys.platform))
    exit()

__module_unit_export_symbol__ = "AzRunUnitTests"
__module_integ_export_symbol__ = "AzRunIntegTests"
__executable_export_symbol__ = 'ContainsAzUnitTestMain'
__executable_unittest_arg__ = '--unittest'
__no_dll__ = False


class ModuleType:
    EXECUTABLE = 1
    LIBRARY = 2

    def __init__(self):
        pass


def clean_timestamp(s):
    s = s.replace(':', '_')
    s = s.replace('.', '_')
    return s


def get_output_dir(timestamp, prefix_dir="TestResults"):
    return os.path.join(prefix_dir, clean_timestamp(timestamp.isoformat()) if timestamp else "NO_TIMESTAMP")


def create_xml_output_filename(path_to_file, output_dir):
    _, filename = os.path.split(path_to_file)
    base_filename, ext = os.path.splitext(filename)
    return os.path.abspath(os.path.join(output_dir, 'test_results_' + base_filename + '.xml'))


def add_dirs_to_path(path_dirs):
    if path_dirs:
        os.environ['PATH'] += os.pathsep + os.pathsep.join([os.path.abspath(path_dir) for path_dir in path_dirs])


def scan_one(args, extra, type_, scanner, runner_path, bootstrap_config, file_name, output_dir):
    """ Scan one module or executable

    :param args: command line arguments
    :param extra: extra parameters
    :param int type_: module or executable
    :param scanner: platform-specific scanner instance
    :param runner_path: path to test runner executable
    :param BootstrapConfig bootstrap_config: configuration object for bootstrapping modules
    :param file_name: filename of module to scan
    :param output_dir: directory for output
    :return: ScannerResult
    """

    logger.info("{}: {}".format(type_, file_name))
    xml_out = create_xml_output_filename(file_name, output_dir)
    if os.path.exists(xml_out):
        return  # module has already been tested

    # for a more exhaustive list of options:
    # https://github.com/google/googletest/blob/master/googletest/docs/AdvancedGuide.md#running-a-subset-of-the-tests
    # --help                lists command options
    # --gtest_list_tests    just list what tests are in the module
    # --gtest_shuffle       shuffle test ordering
    cmd_args = ["--gtest_output=xml:" + xml_out,
                "--gtest_color=yes"]

    cmd_args += extra

    if args.wait_for_debugger:
        # user has requested to attach a debugger when running
        cmd_args += ["--wait-for-debugger"]

    ret = 0

    if type_ == ModuleType.LIBRARY:
        if args.integ:
            # user wants to run integration tests
            export_symbol = __module_integ_export_symbol__
            cmd_args += ["--integ"]
        else:
            # just run unit tests
            export_symbol = __module_unit_export_symbol__

        # run with bootstrapper
        ran_with_bootstrapper = False
        if bootstrap_config:
            module_name = os.path.split(file_name)[1]
            bootstrapper = bootstrap_config.get_bootstrapper(module_name)
            if bootstrapper:
                ran_with_bootstrapper = True
                try:
                    working_dir = args.dir
                    app = os.path.join(args.dir, bootstrapper.command_line[0])
                    if not os.path.isfile(app):
                        logger.error("bootstrap executable not found {}".format(app))

                    full_command_line = bootstrapper.command_line + tuple(cmd_args)
                    ret = scanner.bootstrap(working_dir, full_command_line)

                except:
                    ret = RunnerReturnCodes.UNEXPECTED_EXCEPTION
                    logger.exception("bootstrap failed")

        # run with "runner_<platform>" as the implicit bootstrapper (no need to specify this
        # in the bootstrapper config file)
        if not ran_with_bootstrapper:
            if scanner.exports_symbol(file_name, export_symbol):
                try:
                    ret = scanner.call(file_name, export_symbol, runner_path, args=cmd_args)

                except KeyboardInterrupt:
                    raise

                except:
                    ret = RunnerReturnCodes.UNEXPECTED_EXCEPTION
                    logger.exception("module call failed")

            else:
                ret = RunnerReturnCodes.SYMBOL_NOT_FOUND

    elif type_ == ModuleType.EXECUTABLE:
        if scanner.exports_symbol(file_name, __executable_export_symbol__):
            try:
                cmd_args = ["--unittest"] + cmd_args
                ret = scanner.run(file_name, args=cmd_args)

            except KeyboardInterrupt:
                raise

            except:
                ret = RunnerReturnCodes.UNEXPECTED_EXCEPTION
                logger.exception("executable run failed")

        else:
            logger.error("Executable does not export correct symbol.")
            ret = RunnerReturnCodes.SYMBOL_NOT_FOUND

    else:
        raise NotImplementedError("module type not supported: " + str(type_))

    err = RunnerReturnCodes.to_string(ret)
    return ScanResult(path=file_name, return_code=ret, xml_path=xml_out, error_msg=err)


def scan(args, extra, output_dir):
    scanner = Scanner()

    if not args.runner_path:
        runner_path = os.path.abspath(os.path.join(args.dir, scanner.__runner_exe__))
    else:
        runner_path = os.path.abspath(args.runner_path)
    if not os.path.exists(runner_path):
        logger.exception("Invalid test runner path: {}".format(runner_path))
        return

    bootstrap_config = None
    if args.bootstrap_config:
        with open(args.bootstrap_config) as json_file:
            bootstrap_config = BootstrapConfig(flatten=True)
            bootstrap_config.load(json.load(json_file))

    add_dirs_to_path(args.add_path)
    scan_results = []  # list of ScanResult()

    # Create a FileApprover to determine if scanned files can be tested
    file_approver = FileApprover(args.whitelist, args.whitelist_file, args.blacklist_file, args.include_gems,
                                 args.include_projects)

    module_failures = 0

    # Dynamic Libraries / Modules
    if not __no_dll__:
        logger.info("Scanning for dynamic libraries")
        for file_name in scanner.enumerate_modules(args.dir):
            try:
                if args.limit and len(scan_results) >= args.limit:
                    continue  # reached scanning limit

                if args.only and not FileApprover.is_in_list(file_name, args.only.split(',')):
                    continue  # filename does not match any expected pattern

                if not file_approver.is_approved(file_name):
                    continue

                result = scan_one(args, extra, ModuleType.LIBRARY, scanner, runner_path, bootstrap_config,
                                  file_name, output_dir)
                if result:
                    scan_results += [result]
                    if result.return_code != RunnerReturnCodes.TESTS_SUCCEEDED:
                        module_failures += 1

            except KeyboardInterrupt:
                logger.exception("Process interrupted by user.")
                break
            except:
                logger.exception("Module scan failed.")

    # Executables
    if args.exe:
        logger.info("Scanning for executables")
        for file_name in scanner.enumerate_executables(args.dir):

            if args.limit and len(scan_results) >= args.limit:
                continue  # reached scanning limit

            if args.only and not FileApprover.is_in_list(file_name, args.only.split(',')):
                continue  # filename does not match any expected pattern

            if not file_approver.is_approved(file_name):
                continue

            result = scan_one(args, extra, ModuleType.EXECUTABLE, scanner, runner_path,
                              bootstrap_config, file_name, output_dir)
            if result:
                scan_results += [result]
                if result.return_code != RunnerReturnCodes.TESTS_SUCCEEDED:
                    module_failures += 1

    # Convert the set of XML files into an HTML report
    HTMLReporter.create_html_report(scan_results, output_dir)
    HTMLReporter.create_html_failure_report(scan_results, output_dir)

    return 1 if module_failures > 0 else 0