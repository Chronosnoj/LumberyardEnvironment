#!/usr/bin/env python
# encoding: utf-8

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

from waflib.Configure import conf
from waflib.TaskGen import feature, after_method, before_method
from waflib.Tools import c_preproc
from waflib.Context import BOTH
from waflib import Node, Task, Utils, Logs, Errors
from binascii import hexlify
from collections import defaultdict
import os
import json
import waflib.Build
import threading

#  Code Generator Settings
code_generator_ignore_includes = False
code_generator_suppress_errors_as_warnings = False
code_generator_verbose = False
code_generator_invoke_command_line_directly = False
command_length_when_windows_fails_to_execute = 8192

# Module for our utility
module = "AZCodeGenerator"

# Add our task data to be saved between builds
waflib.Build.SAVED_ATTRS.append('azcg')

# Create a lock object to be used for az_code_gen task node access
# This must be used before any calls to make_node, find_node or similar methods during task execution
# These methods create nodes and simultaneous task invocation that attempts to create the same node will error
task_node_access_lock = threading.Lock()


def get_input_dir_node(tg):
    """
    Gets the input dir from a task generator.

    If 'az_code_gen_input_dir' is set, use that. Otherwise, use the project's root directory.
    """
    input_dir = getattr(tg, 'az_code_gen_input_dir', None)
    if input_dir:
        input_dir = tg.bld.srcnode.make_node(input_dir)
    else:
        input_dir = tg.path
    return input_dir


def get_output_dir_node(tg):
    """
    Gets the output dir from a task generator.

    If 'az_code_gen_out_dir' is set, use that. Otherwise, use the project's build directory.
    """
    output_dir = getattr(tg, 'az_code_gen_out_dir', None)
    if output_dir:
        output_dir = tg.bld.srcnode.make_node(output_dir)
    else:
        output_dir = tg.path.get_bld()
        output_dir = tg.bld.bldnode.make_node('{}/{}'.format(output_dir.relpath(), tg.idx))
    return output_dir


@feature('az_code_gen')
@before_method('process_use')
def add_codegen_includes(self):
    for attr in ('includes', 'export_includes'):
        if not hasattr(self, attr):
            setattr(self, attr, list())
    include_path_to_inject = get_output_dir_node(self)
    self.includes.append(include_path_to_inject)
    self.export_includes.append(include_path_to_inject)


@feature('az_code_gen')
@after_method('process_use')
def create_code_generator_tasks(self):
    # Skip during project generation
    if self.bld.env['PLATFORM'] == 'project_generator':
        return

    # promote raw entries to list
    if isinstance(getattr(self, 'az_code_gen', []), dict):
        self.az_code_gen = [self.az_code_gen]

    for az_code_gen_pass in getattr(self, 'az_code_gen', []):
        # See if we have any scripts
        code_generator_scripts = az_code_gen_pass.get('scripts', [])
        if not code_generator_scripts:
            Logs.warn(
                'az_code_gen feature enabled but no scripts were specified. '
                'No code generation performed for target {}'.format(self.target))
            return

        code_gen_arguments = az_code_gen_pass.get('arguments', [])
        if isinstance(code_gen_arguments, str):
            code_gen_arguments = [code_gen_arguments]

        code_gen_options = az_code_gen_pass.get('options', [])
        if isinstance(code_gen_options, str):
            code_gen_options = [code_gen_options]

        code_gen_input = az_code_gen_pass.get('files', [])
        if not code_gen_input:
            Logs.warn(
                'az_code_gen feature enabled but no files were specified. '
                'No code generation performed for target {}'.format(self.target))
            return

        # Create one task per input file/list
        for input_item in code_gen_input:
            # Auto promote non-lists to lists
            if not isinstance(input_item, list):
                input_file_list = [input_item]
            else:
                input_file_list = input_item

            create_az_code_generator_task(self, input_file_list, code_generator_scripts, code_gen_arguments, code_gen_options)


def create_az_code_generator_task(self, input_file_list, code_generator_scripts, code_gen_arguments, code_gen_options):
    input_dir_node = get_input_dir_node(self)
    input_nodes = [in_file if isinstance(in_file, waflib.Node.Node) else input_dir_node.make_node(in_file) for in_file in input_file_list]

    # Create a code gen task.
    # We would simply do "new_task = self.create_task('az_code_gen', input_nodes)" had we no need
    # to worry about build ordering.  Instead, we add to az_code_gen_group (tbd: a better name?)
    new_task = az_code_gen(env=self.env.derive(), generator=self)
    new_task.set_inputs(input_nodes)
    self.bld.add_to_group(new_task, 'az_code_gen_group')

    new_task.path = self.path
    new_task.input_dir = input_dir_node
    new_task.output_dir = get_output_dir_node(self)
    new_task.scripts = code_generator_scripts
    new_task.includes = self.to_incnodes(self.includes)
    new_task.defines = self.defines
    for code_gen_argument in code_gen_arguments:
        new_task.add_argument(code_gen_argument)

    if 'PrintOutputRedirectionFile' in code_gen_options:
        new_task.capture_and_print_error_output()

    if self.bld.is_option_true('use_debug_code_generator'):
        Logs.warn("Using DEBUG AZCodeGenerator!")
        path = new_task.env.CODE_GENERATOR_PATH[0]
        if path.endswith('profile'):
            new_task.env.CODE_GENERATOR_PATH = [path[:-len('profile')] + 'debug']
        else:
            Logs.error('CODE_GENERATOR_PATH had unexpected end: {}'.format(path))

    # Pre-compute arguments for task since doing this during run() causes multi-thread issues in Node
    new_task.prepare_task()


def clean_path(path):
    return path.replace('\\', '/')


def hash_node_list(list, up):
    for item in list:
        if isinstance(item, waflib.Node.Node):
            up(item.abspath().encode())
        else:
            up(item)

g_uid_counts = defaultdict(int)


class az_code_gen(Task.Task):
    color = 'CYAN'

    def __init__(self, *k, **kw):
        super(az_code_gen, self).__init__(*k, **kw)

        self.more_tasks = []
        self.input_dir = None
        self.output_dir = None
        self.scripts = []
        self.includes = []
        self.defines = []
        self.argument_list = []
        self.error_output_file_node = None
        self.script_deps = []
        # Ensure we have a 'azcg' attribute to cache task info for future builds
        if not isinstance(getattr(self.generator.bld, 'azcg', None), dict):
            self.generator.bld.azcg = defaultdict(dict)

    def uid(self):
        try:
            return self.uid_
        except AttributeError:
            m = Utils.md5()
            up = m.update

            # Be sure to add any items here that change how code gen runs, this needs to be unique!
            # Ensure anything here will not change over the life of the task
            up(self.path.abspath().encode())
            up(self.input_dir.abspath().encode())
            up(self.output_dir.abspath().encode())
            hash_node_list(self.inputs, up)
            hash_node_list(self.scripts, up)
            hash_node_list(self.includes, up)
            hash_node_list(self.defines, up)
            hash_node_list(self.argument_list, up)

            self.uid_ = m.digest()
            g_uid_counts[self.uid_] += 1
            assert(g_uid_counts[self.uid_] == 1)
            return self.uid_

    def azcg_get(self, key, default_value=None):
        assert isinstance(key, str)
        return self.generator.bld.azcg[self.uid()].get(key, default_value)

    def azcg_set(self, key, val):
        assert isinstance(key, str)
        self.generator.bld.azcg[self.uid()][key] = val

    def azcg_append_unique(self, key, val):
        assert isinstance(key, str)
        vals = self.azcg_get(key, [])
        assert isinstance(vals, list)
        if val not in vals:
            vals.append(val)
        self.azcg_set(key, vals)

    def add_argument(self, argument):
        """
        Add an argument to the argument list
        """
        self.argument_list.append(argument)

    def capture_and_print_error_output(self):
        error_output_file_path = 'CodeGenErrorOutput/error_output_{}.log'.format(hexlify(self.uid()))
        self.error_output_file_node = self.generator.bld.bldnode.find_or_declare(error_output_file_path)

    def register_output_file(self, output_path, should_add_to_build):
        # Nodes won't work with absolute paths, so we have to remove the build path from
        # the given output file path. Given path is unicode, make it str to match Node.
        output_path = os.path.relpath(str(output_path), start=self.generator.bld.bldnode.abspath())

        with task_node_access_lock:
            output_node = self.generator.bld.bldnode.find_node(output_path)

        if output_node is None:
            Logs.error('az_code_gen: Unable to find generated file as node {}'.format(output_path))

        self.outputs.append(output_node)

        # Add local folder to include path of the task_gen
        self.generator.env.append_unique('INCPATHS', output_node.parent.abspath())

        if should_add_to_build:
            # Add to persistent link list
            self.azcg_append_unique('link_inputs', output_node)
            # Perform actual add to link task
            self.add_link_task(output_node)

    def register_dependency_file(self, dep_path):
        dep_path = os.path.relpath(str(dep_path), start=self.generator.bld.srcnode.abspath())

        with task_node_access_lock:
            dep_node = self.generator.bld.srcnode.find_node(dep_path)

        if dep_node:
            self.azcg_append_unique('script_deps', dep_node)
            self.generator.bld.node_deps[self.uid()].append(dep_node)
        else:
            Logs.error('az_code_gen: Unable to find dependency file as node: {}'.format(dep_path))

    def add_link_task(self, node_to_link):
        # Using modified version of example here:
        # https://github.com/waf-project/waf/blob/7b7531b0c6d0598033bea608ffc3c8e335434a6d/docs/book/examples/scenarios_unknown/mytool.py
        try:
            task_hook = self.generator.get_hook(node_to_link)
        except Errors.WafError:
            Logs.error(
                'az_code_gen: Created file {} marked for "should add to build" '
                'is not buildable.'.format(node_to_link.path_from(self.generator.bldnode)))
            return

        created_task = task_hook(self.generator, node_to_link)
        link_task = getattr(self.generator, 'link_task', None)
        if link_task:
            link_task.set_run_after(created_task)  # Compile our .cpp before we link.
            link_task.inputs.append(created_task.outputs[0])  # list.append is atomic (GIL).
        else:
            # If we ever have a use case where link_task is inappropriate (non-C-family lang?),
            # then we should do "self.more_tasks.append(created_task)" in those cases.
            Logs.error('az_code_gen: Created file {} marked for "should add to build" '
                       'was not added to a link task.'.format(
                           node_to_link.path_from(self.generator.bldnode)))

    def write_argument_list_to_file(self):
        """
        Writes argument_list to a file
        :return: (True, <argument file>) on success; (False, '') on failure
        """
        argument_file_path = 'CodeGenArguments/arguments_file_{}.args'.format(hexlify(self.uid()))
        argument_file_node = self.generator.bld.bldnode.find_or_declare(argument_file_path)
        try:
            argument_file_node.write('\n'.join(self.argument_list))
        except:
            Logs.error(
                'az_code_gen: Failed to write argument file {}'.format(argument_file_node.abspath()))
            return False, ''
        return True, argument_file_node.abspath()

    def handle_code_generator_output(self, code_gen_output):
        """
        Decode json object and process return from generator
        :param code_gen_output: json string
        :return True on success, False on failure
        """
        try:
            json_object = json.loads(code_gen_output)
            for output_object in json_object:
                if output_object['type'] == 'info':
                    Logs.debug(output_object['info'])
                elif output_object['type'] == 'error':
                    Logs.error(output_object['error'])
                elif output_object['type'] == 'generated_file':
                    self.register_output_file(output_object['file_name'],
                                              output_object['should_be_added_to_build'])
                elif output_object['type'] == 'dependency_file':
                    self.register_dependency_file(output_object['file_name'])
                else:
                    Logs.error('az_code_gen: Unknown output json type returned from Code Generator. Type is: {} - Raw output: {}'.format(output_object['type'], code_gen_output))
                    return False
            return True
        except ValueError as value_error:
            # If we get output that isn't JSON, it means Clang errored before
            # the code generator gained control. Likely invalid commandline arguments.
            Logs.error('az_code_gen: Failed to json.loads output with error "{}" - output string was:\n{}'.format(str(value_error), code_gen_output))
            import traceback
            import sys
            tb_list = traceback.extract_tb(sys.exc_traceback)
            for filename, lineno, name, line in tb_list:
                Logs.error(
                    '{}({}): error {}: in {}: {}'.format(filename, lineno,
                                                         value_error.__class__.__name__,
                                                         name, line))
            filename, lineno, _, _ = tb_list[-1]
            Logs.error('{}({}): error {}: {}'.format(filename, lineno,
                                                     value_error.__class__.__name__,
                                                     str(value_error)))
            return False

    def print_error_output(self):
        Logs.error('Error output stored in {}:'.format(self.error_output_file_node.abspath()))
        Logs.error(self.error_output_file_node.read())

    def exec_code_generator(self, argument_string):
        """
        Execute the code generator with argument string
        :return: True on success, False on failure
        """
        command_string = '\"' + os.path.join(self.env['CODE_GENERATOR_PATH'][0],
                                             self.env['CODE_GENERATOR_EXECUTABLE']) + '\" ' + argument_string

        Logs.debug('az_code_gen: Invoking code generator with command: {}'.format(command_string))

        # Ensure not too long to execute on the current host
        host = Utils.unversioned_sys_platform()
        if (host == 'win_x64') or (host == 'win32'):
            if len(command_string) >= command_length_when_windows_fails_to_execute:
                Logs.warn(
                    "az_code_gen: Unable to execute code generator due to command length being "
                    "too long")
                return False

        try:
            (code_gen_output, code_gen_error_output) = self.generator.bld.cmd_and_log(
                command_string, output=BOTH, quiet=BOTH)
            if code_gen_error_output and not code_gen_error_output.isspace():
                Logs.error('az_code_gen: Code generator output to stderr even though it indicated success. Output:\n{}'.format(code_gen_error_output))
                return False
        except Errors.WafError as e:
            Logs.error('az_code_gen: Code generator execution failed! Output:\n{}'.format(e.stderr))
            self.handle_code_generator_output(e.stdout)
            if self.error_output_file_node:
                self.print_error_output()
            return False

        if not self.handle_code_generator_output(code_gen_output):
            return False

        # force waf to recompute a full signature for this task (we may have new/deleted dependencies we need it to account for)
        try:
            del self.cache_sig
        except:
            pass

        # Add outputs as dependencies of this.
        # raw_deps is a list of [self.signature(), ...outputs]
        self.generator.bld.raw_deps[self.uid()] = [self.signature()] + self.outputs

        return True

    def run_code_generator_at_syntax(self):
        """
        Run the code generator using at syntax for all accumulated arguments
        :return: True on success, False on failure
        """
        status, argument_file = self.write_argument_list_to_file()
        if status:
            return self.exec_code_generator(' \"@' + argument_file + '\"')
        return False

    def run_code_generator_directly(self):
        """
        Run the code generator using a direct command line (unless too big for host support,
        then fallback to @ syntax)
        :return: True on success, False on failure
        """
        argument_string = " ".join(self.argument_list)
        if not self.exec_code_generator(argument_string):
            Logs.warn(
                "az_code_gen: Unable to run code generator directly. "
                "Falling back to @ syntax invocation")
            return self.run_code_generator_at_syntax()
        return True

    def prepare_task(self):
        # Create the directory if it doesn't already exist
        self.output_dir.mkdir()
        # We expect json output for friendlier parsing
        self.add_argument("-output-using-json")

        self.add_argument('-input-path "{}"'.format(clean_path(self.input_dir.abspath())))
        self.add_argument('-output-path "{}"'.format(clean_path(self.output_dir.abspath())))

        # Write input files to a file (command line version is too long)
        for input_file in self.inputs:
            self.add_argument('-input-file "{}"'.format(clean_path(input_file.path_from(self.input_dir))))
            input_file.parent.get_bld().mkdir()

        def pypath(python_path):
            # Absolute paths are good to go as-is
            # Relative paths are assumed relative to src
            if not os.path.isabs(python_path):
                # Toss it in a node to figure out an absolute path
                python_path_node = self.generator.bld.srcnode.make_node(python_path)
                python_path = python_path_node.abspath()

            if not os.path.exists(python_path):
                Logs.warn('az_code_gen: Path given as python path does not exist: {}'.format(python_path))
            return clean_path(python_path)

        # Python paths
        self.add_argument('-python-home "{}"'.format(pypath(self.env['CODE_GENERATOR_PYTHON_HOME'])))
        for python_path in self.env['CODE_GENERATOR_PYTHON_PATHS']:
            self.add_argument('-python-path "{}"'.format(pypath(python_path)))

        # Debug python paths
        self.add_argument('-python-home-debug "{}"'.format(pypath(self.env['CODE_GENERATOR_PYTHON_HOME_DEBUG'])))
        for python_debug_path in self.env['CODE_GENERATOR_PYTHON_DEBUG_PATHS']:
            self.add_argument('-python-debug-path "{}"'.format(pypath(python_debug_path)))

        if code_generator_ignore_includes:
            self.add_argument('-ignore-includes')

        if code_generator_suppress_errors_as_warnings:
            self.add_argument('-suppress-errors-as-warnings')

        if code_generator_verbose:
            self.add_argument('-v')

        if Utils.unversioned_sys_platform().startswith('linux'):
            self.add_argument('-include-path /usr/include/c++/v1')

        for include in self.includes:
            self.add_argument('-include-path "{}"'.format(clean_path(include.abspath())))

        for define in self.defines:
            self.add_argument('-define "{}"'.format(define))

        for code_generator_script in self.scripts:
            self.add_argument('-codegen-script "{}"'.format(clean_path(self.path.find_or_declare(code_generator_script).get_src().abspath())))

        # Include file that contains code generation tag definitions
        codegen_tags = self.env['CODE_GENERATOR_TAGS']
        if not codegen_tags:
            codegen_tags = 'Code/Framework/AZCore/AZCore/Preprocessor/CodeGen.h'
        self.add_argument('-force-include "{}"'.format(clean_path(self.generator.bld.CreateRootRelativePath(codegen_tags))))

        if self.error_output_file_node:
            self.add_argument('-redirect-output-file "{}"'.format(clean_path(self.error_output_file_node.abspath())))
        
        if 'CLANG_SEARCH_PATHS' in self.env:
            self.add_argument('-resource-dir "{}"'.format(self.env['CLANG_SEARCH_PATHS']['libraries'][0]))


    def run(self):
        # clear script/link dependencies
        self.azcg_set('script_deps', [])
        self.azcg_set('link_inputs', [])

        if code_generator_invoke_command_line_directly:
            success = self.run_code_generator_directly()
        else:
            success = self.run_code_generator_at_syntax()

        if success:
            return 0
        return 1

    def scan(self):
        """
        Re-use the C/C++ scanner, but only on the given header file
        """
        # Does this even work with headers?
        dependencies = c_preproc.scan(self)

        # Add everything in the azcg folder as a dependency
        azcg_dir = self.env['CODE_GENERATOR_PATH'][0]
        azcg_dir_node = self.generator.bld.root.find_node(azcg_dir)
        if azcg_dir_node:
            azcg_dep_nodes = azcg_dir_node.ant_glob('**/*')
            dependencies[0].extend(azcg_dep_nodes)
        else:
            Logs.warn('az_code_gen: Unable to find azcg directory. Code Generator tasks will not have the utility/scripts as dependencies')

        # this script is a dependency
        script_relpath = os.path.relpath(__file__, self.generator.bld.root.abspath())
        script_node = self.generator.bld.root.find_node(script_relpath)
        dependencies[0].append(script_node)

        # all codegen scripts are deps
        for script in self.scripts:
            script_node = self.path.find_or_declare(script)
            dependencies[0].append(script_node)

        # any dependencies from last run that were stored
        script_deps = self.azcg_get('script_deps', [])
        dependencies[0].extend(script_deps)

        return dependencies

    def runnable_status(self):
        """
        Ensure that all outputs exist before skipping execution.
        """
        ret = super(az_code_gen, self).runnable_status()
        if ret == Task.SKIP_ME:
            # raw_deps is a list of [self.signature(), ...outputs]
            raw_deps = self.generator.bld.raw_deps[self.uid()]
            outputs = raw_deps[1:]

            if raw_deps and raw_deps[0] != self.signature():
                Logs.debug('az_code_gen: Running task for file {}, signature different.'.format(
                    self.inputs[0].abspath()))
                return Task.RUN_ME

            for output_node in outputs:
                # If you can't find the file, running is required
                if not os.path.isfile(output_node.abspath()):
                    Logs.debug(
                        'az_code_gen: Running task for file {}, output file {} not found.'.format(
                            self.inputs[0].abspath(), output_node.abspath()))
                    return Task.RUN_ME

            # At this point, we know we aren't being re-run, inject all output parent folders into include paths
            for output_node in outputs:
                self.generator.env.append_unique('INCPATHS', output_node.parent.abspath())

            # Also add the raw output path
            self.generator.env.append_unique('INCPATHS', get_output_dir_node(self.generator).abspath())

            # link_inputs is a list of nodes that need to be added to the link each time
            for link_node in self.azcg_get('link_inputs', []):
                self.add_link_task(link_node)

            self.outputs = outputs
            self.generator.source += outputs
        return ret

    def post_run(self):
        Task.Task.post_run(self)

        # Due to #includes of code generator header files, we can have an output node which is also an input node.
        # In addition, we are taking nodes that are not originally build nodes (e.g. header files) and building them, which alters the signature flow in Node.get_bld_sig().
        # Task.post_run() default behavior is to set the Node.sig to the task signature which will change our computed task signature because our outputs are our inputs in same cases.
        # To mitigate this, we must restore the original signature for any file that had a non-build signature previously.
        # However, we do not want to alter the signature for files that will be consumed by later tasks.
        # Therefore, we should restore signatures on any node that is not being added to the build (any output nodes not in link_task).
        for output in self.outputs:
            if not output in self.azcg_get('link_inputs', []):
                output.sig = output.cache_sig = Utils.h_file(output.abspath())
