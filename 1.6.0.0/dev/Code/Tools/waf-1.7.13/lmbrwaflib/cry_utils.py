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
# Original file Copyright Crytek GMBH or its affiliates, used under license.
#
import sys
from waflib import Configure, Logs, Utils, Options, ConfigSet
from waflib.Configure import conf
from waflib import Logs, Node, Errors
from waflib.Scripting import _is_option_true
from waflib.TaskGen import after_method, before_method, feature, extension, taskgen_method
from waflib.Task import Task, RUN_ME, SKIP_ME
from waflib.Errors import BuildError, WafError

import json, os
import shutil
import stat
import glob
import time
import re

try:
    import _winreg
except ImportError:
    pass

def fast_copyfile(src, dst, buffer_size=1024*1024):
    """
    Copy data from src to dst - reimplemented with a buffer size
    Note that this function is simply a copy of the function from the official python shutils.py file, but
    with an increased (configurable) buffer size fed into copyfileobj instead of the original 16kb one
    """
    if shutil._samefile(src, dst):
        raise shutil.Error("`%s` and `%s` are the same file" % (src, dst))

    for fn in [src, dst]:
        try:
            st = os.stat(fn)
        except OSError:
            # File most likely does not exist
            pass
        else:
            # XXX What about other special files? (sockets, devices...)
            if stat.S_ISFIFO(st.st_mode):
                raise shutil.SpecialFileError("`%s` is a named pipe" % fn)

    with open(src, 'rb') as fsrc:
        with open(dst, 'wb') as fdst:
            shutil.copyfileobj(fsrc, fdst, buffer_size)

def fast_copy2(src, dst, buffer_size=1024*1024):
    """Copy data and all stat info ("cp -p src dst").

    The destination may be a directory.
    
    Note that this is just a copy of the copy2 function from shutil.py that calls the above fast copyfile

    """
    if os.path.isdir(dst):
        dst = os.path.join(dst, os.path.basename(src))
    fast_copyfile(src, dst, buffer_size)
    shutil.copystat(src, dst)
    

            
###############################################################################
WAF_EXECUTABLE = 'lmbr_waf.bat'

#############################################################################
# Helper method to add a value to a keyword and create the keyword if it is not there
# This method will also make sure that a list of items is appended to the kw key
@conf
def append_kw_entry(kw,key,value):
    if not key in kw:
        kw[key] = []
    if isinstance(value,list):
        kw[key] += value
    else:
        kw[key] += [value]

#############################################################################
# Helper method to add a value to a keyword and create the keyword if it is not there
# This method will also make sure that a list of items is appended to the kw key
@conf
def prepend_kw_entry(kw,key,value):
    if not key in kw:
        kw[key] = []
    if isinstance(value,list):
        kw[key] = value + kw[key]
    else:
        kw[key] = [value] + kw[key]

#############################################################################
#  Utility function to add an item to a list uniquely.  This will not reorder the list, rather it will discard items
#  that already exist in the list.  This will also flatten the input into a single list if there are any embedded
#  lists in the input
@conf
def append_to_unique_list(unique_list,x):

    if isinstance(x,list):
        for y in x:
            append_to_unique_list(unique_list, y)
    else:
        if not x in unique_list:
            unique_list.append(x)


#############################################################################
# Utility method to perform remove duplicates from a list while preserving the order (ie not using sets)
@conf
def clean_duplicates_in_list(list,debug_list_name):
    clean_list = []
    distinct_set = set()
    for item in list:
        if item not in distinct_set:
            clean_list.append(item)
            distinct_set.add(item)
        else:
            Logs.debug('lumberyard: Duplicate item {} detected in \'{}\''.format(item,debug_list_name))
    return clean_list


#############################################################################
# Utility function to make sure that kw inputs are 'lists'
@conf
def sanitize_kw_input_lists(list_keywords, kw):

    for key in list_keywords:
        if key in kw:
            if not isinstance(kw[key],list):
                kw[key] = [ kw[key] ]

@conf
def is_option_true(ctx, option_name):
    """ Util function to better intrepret all flavors of true/false """
    return _is_option_true(ctx.options, option_name)
    
#############################################################################
#############################################################################
# Helper functions to handle error and warning output
@conf
def cry_error(conf, msg):
    conf.fatal("error: %s" % msg) 
    
@conf
def cry_file_error(conf, msg, filePath, lineNum = 0 ):
    if isinstance(filePath, Node.Node):
        filePath = filePath.abspath()
    if not os.path.isabs(filePath):
        filePath = conf.path.make_node(filePath).abspath()
    conf.fatal('%s(%s): error: %s' % (filePath, lineNum, msg))
    
@conf
def cry_warning(conf, msg):
    Logs.warn("warning: %s" % msg) 
    
@conf
def cry_file_warning(conf, msg, filePath, lineNum = 0 ):
    Logs.warn('%s(%s): warning: %s.' % (filePath, lineNum, msg))
    
#############################################################################
#############################################################################   
# Helper functions to json file parsing and validation

def _decode_list(data):
    rv = []
    for item in data:
        if isinstance(item, unicode):
            item = item.encode('utf-8')
        elif isinstance(item, list):
            item = _decode_list(item)
        elif isinstance(item, dict):
            item = _decode_dict(item)
        rv.append(item)
    return rv
        
def _decode_dict(data):
    rv = {}
    for key, value in data.iteritems():
        if isinstance(key, unicode):
            key = key.encode('utf-8')
        if isinstance(value, unicode):
            value = value.encode('utf-8')
        elif isinstance(value, list):
            value = _decode_list(value)
        elif isinstance(value, dict):
            value = _decode_dict(value)
        rv[key] = value
    return rv

@conf
def parse_json_file(conf, file_node):   
    try:
        file_content_raw = file_node.read()
        file_content_parsed = json.loads(file_content_raw, object_hook=_decode_dict)
        return file_content_parsed
    except Exception as e:
        line_num = 0
        exception_str = str(e)
        
        # Handle invalid last entry in list error
        if "No JSON object could be decoded" in exception_str:
            cur_line = ""
            prev_line = ""
            file_content_by_line = file_content_raw.split('\n')
            for lineIndex, line in enumerate(file_content_by_line):
            
                # Sanitize string
                cur_line = ''.join(line.split())    
                
                # Handle empty line
                if not cur_line:
                    continue
                
                # Check for invalid JSON schema
                if any(substring in (prev_line + cur_line) for substring in [",]", ",}"]):
                    line_num = lineIndex
                    exception_str = 'Invalid JSON, last list/dictionary entry should not end with a ",". [Original exception: "%s"]' % exception_str
                    break;
                    
                prev_line = cur_line
      
        # If exception has not been handled yet
        if not line_num:
            # Search for 'line' in exception and output pure string
            exception_str_list = exception_str.split(" ")
            for index, elem in enumerate(exception_str_list):
                if elem == 'line':
                    line_num = exception_str_list[index+1]
                    break
                    
        # Raise fatal error
        conf.cry_file_error(exception_str, file_node.abspath(), line_num)

###############################################################################

###############################################################################
import sys
@taskgen_method
def copy_files(self, source_file, check_timestamp_and_size=True):

    # import libraries are only allowed to be copied if the copy import flag is set and we're a secondary copy (the primary copy goes to the appropriate Bin64 directory)
    _, extension = os.path.splitext(source_file.abspath())
    is_import_library = self._type == 'shlib' and extension == '.lib'
    is_secondary_copy_install = getattr(self, 'is_secondary_copy_install', False)

    # figure out what copies should be done
    skip_primary_copy = is_import_library
    skip_secondary_copy = is_import_library and not is_secondary_copy_install

    def _create_sub_folder_copy_task(output_sub_folder_copy):
        if output_sub_folder_copy is not None and output_sub_folder_copy != output_sub_folder:
            output_node_copy = node.make_node(output_sub_folder_copy)
            output_node_copy = output_node_copy.make_node( os.path.basename(source_file.abspath()) )
            self.create_copy_task(source_file, output_node_copy, False, check_timestamp_and_size)

    # If there is an attribute for 'output_folder', then this is an override of the output target folder
    # Note that subfolder if present will still be applied
    output_folder = getattr(self, 'output_folder', None)
    if output_folder:
        target_path = self.bld.root.make_node(output_folder)
        output_nodes = [target_path]
    else:
        output_nodes = self.bld.get_output_folders(self.bld.env['PLATFORM'], self.bld.env['CONFIGURATION'])

    for node in output_nodes:
        output_node = node
        output_sub_folder = getattr(self, 'output_sub_folder', None)
        if not skip_primary_copy:
            if output_sub_folder is not None:
                output_node = output_node.make_node(output_sub_folder)
            output_node = output_node.make_node( os.path.basename(source_file.abspath()) )
            self.create_copy_task(source_file, output_node, False, check_timestamp_and_size)

        # Special case to handle additional copies
        if not skip_secondary_copy:
            output_sub_folder_copy_attr = getattr(self, 'output_sub_folder_copy', None)
            if isinstance(output_sub_folder_copy_attr, str):
                _create_sub_folder_copy_task(output_sub_folder_copy_attr)
            elif isinstance(output_sub_folder_copy_attr, list):
                for output_sub_folder_copy_attr_item in output_sub_folder_copy_attr:
                    if isinstance(output_sub_folder_copy_attr_item, str):
                        _create_sub_folder_copy_task(output_sub_folder_copy_attr_item)
                    else:
                        Logs.warn("[WARN] attribute items in 'output_sub_folder_copy' must be a string.")



@taskgen_method
def create_copy_task(self, source_file, output_node, optional=False, check_timestamp_and_size=True):
    if not getattr(self.bld, 'existing_copy_tasks', None):
        self.bld.existing_copy_tasks = dict()
        
    if output_node in self.bld.existing_copy_tasks:
        Logs.debug('create_copy_task: skipping duplicate output node: (%s, %s)' % (output_node.abspath(), self))
    else:
        new_task = self.create_task('copy_outputs', source_file, output_node)
        new_task.optional = optional
        new_task.check_timestamp_and_size = check_timestamp_and_size
        self.bld.existing_copy_tasks[output_node] = new_task
    
    return self.bld.existing_copy_tasks[output_node]

###############################################################################
# Class to handle copying of the final outputs into the Bin folder
class copy_outputs(Task):
    color = 'YELLOW'
    optional = False
    """If True, build doesn't fail if copy fails."""

    def __init__(self, *k, **kw):
        super(copy_outputs, self).__init__(self, *k, **kw)
        self.check_timestamp_and_size = True

    def run(self):
        src = self.inputs[0].abspath()
        tgt = self.outputs[0].abspath()

        # Create output folder
        tries = 0
        dir = os.path.dirname(tgt)
        created = False
        while not created and tries < 10:
            try:
                os.makedirs(dir)
                created = True
            except OSError as ex:
                self.err_msg = "%s\nCould not mkdir for copy %s -> %s" % (str(ex), src, tgt)
                # ignore Directory already exists when this is called from multiple threads simultaneously
                if os.path.isdir(dir):
                    created = True
            tries += 1

        if not created:
            return -1

        tries = 0
        result = -1
        while result == -1 and tries < 10:
            try:
                fast_copy2(src, tgt)
                result = 0
                self.err_msg = None
            except Exception as why:
                self.err_msg = "Could not perform copy %s -> %s\n\t%s" % (src, tgt, str(why))
                result = -1
                time.sleep(tries)
            tries += 1

        if result == 0:
            try:
                os.chmod(tgt, 493) # 0755
            except:
                pass
        elif self.optional:
            result = 0

        return result

    def runnable_status(self):
        if super(copy_outputs, self).runnable_status() == -1:
            return -1

        # if we have no signature on our output node, it means the previous build was terminated or died
        # before we had a chance to write out our cache, we need to recompute it by redoing this task once.
        if not getattr(self.outputs[0], 'sig', None):
            return RUN_ME  

        src = self.inputs[0].abspath()
        tgt = self.outputs[0].abspath()

        # If there any target file is missing, we have to copy
        try:
            stat_tgt = os.stat(tgt)
        except OSError:
            return RUN_ME

        # Now compare both file stats
        try:
            stat_src = os.stat(src)
        except OSError:
            pass
        else:
            if self.check_timestamp_and_size:
                # same size and identical timestamps -> make no copy
                if stat_src.st_mtime >= stat_tgt.st_mtime + 2 or stat_src.st_size != stat_tgt.st_size:
                    return RUN_ME
            else:
                return super(copy_outputs, self).runnable_status()

        # Everything fine, we can skip this task
        return SKIP_ME

###############################################################################
# Function to generate the copy tasks for build outputs
@after_method('set_pdb_flags')
@after_method('add_import_lib_for_secondary_install')
@after_method('apply_incpaths')
@after_method('apply_map_file')
@feature('c', 'cxx')
def add_install_copy(self):
    if self.bld.env['PLATFORM'] == 'project_generator':
        return

    if not getattr(self, 'link_task', None):
        return

    if self._type == 'stlib' and not getattr(self, 'copy_static_library', False): # Do not copy static libs unless they specifically request
        return

    for src in self.link_task.outputs:
        self.copy_files(src)

###############################################################################
# Function to generate the EXE_VERSION_INFO defines
@after_method('apply_incpaths')
@feature('c', 'cxx')
def apply_version_info(self):

    if getattr(self, 'is_launcher', False) or getattr(self, 'is_dedicated_server', False):
        version = self.bld.get_game_project_version()
    else:
        version = self.bld.get_lumberyard_version()

    # At this point the version number format should be vetted so no need to check again, assume we have numbers

    parsed_version_parts = version.split('.')
    if len(parsed_version_parts) <= 1:
        Logs.warn('Invalid build version (%s), falling back to (1.0.0.1)' % version )
        version_parts = ['1', '0', '0', '1']

    version_parts = [
        parsed_version_parts[0] if len(parsed_version_parts) > 0 else '1',
        parsed_version_parts[1] if len(parsed_version_parts) > 1 else '0',
        parsed_version_parts[2] if len(parsed_version_parts) > 2 else '0',
        parsed_version_parts[3] if len(parsed_version_parts) > 3 else '0'
    ]

    for t in getattr(self, 'compiled_tasks', []):
        t.env.append_value('DEFINES', 'EXE_VERSION_INFO_0=' + version_parts[0])
        t.env.append_value('DEFINES', 'EXE_VERSION_INFO_1=' + version_parts[1])
        t.env.append_value('DEFINES', 'EXE_VERSION_INFO_2=' + version_parts[2])
        t.env.append_value('DEFINES', 'EXE_VERSION_INFO_3=' + version_parts[3])


###############################################################################
def get_output_folder_name(self, platform, configuration):

    def _optional_folder_ext(extension_value):
        if len(extension_value)>0:
            return '.' + extension_value
        else:
            return ''

    # Find the path for the current platform based on build options
    if platform == 'win_x86':
        path = self.options.out_folder_win32
    elif platform == 'win_x64':
        path = self.options.out_folder_win64
    elif platform == 'win_x64_vs2010':
        path = self.options.out_folder_win64
    elif platform == 'win_x64_vs2012':
        path = self.options.out_folder_win64
    elif platform == 'durango':
        path = self.options.out_folder_durango
    elif platform == 'orbis':
        path = self.options.out_folder_orbis
    elif platform == 'linux_x64':
        path = self.options.out_folder_linux64
    elif platform == 'darwin_x64':
        path = self.options.out_folder_mac64
    elif platform == 'ios':
        path = self.options.out_folder_ios
    elif platform == 'appletv':
        path = self.options.out_folder_appletv
    elif platform == 'android_armv7_gcc':
        path = self.options.out_folder_android_armv7_gcc
    elif platform == 'android_armv7_clang':
        path = self.options.out_folder_android_armv7_clang
    else:
        path = 'BinUnknown'
        Logs.warn('[WARNING] No output folder for platform (%s), defaulting to (%s)' % (platform, path))

    # Add any custom folder name extensions based on the configuration
    if 'debug' in configuration.lower():
        path += _optional_folder_ext(self.options.output_folder_ext_debug)
    elif 'profile' in configuration.lower():
        path += _optional_folder_ext(self.options.output_folder_ext_profile)
    elif 'performance' in configuration.lower():
        path += _optional_folder_ext(self.options.output_folder_ext_performance)
    elif 'release' in configuration.lower():
        path += _optional_folder_ext(self.options.output_folder_ext_release)

    if 'test' in configuration.lower():
        path += '.Test'
    if 'dedicated' in configuration.lower():
        path += '.Dedicated'

    return path


###############################################################################
@conf
def get_binfolder_defines(self):
    platform, configuration = self.get_platform_and_configuration()
    if self.env['PLATFORM'] == 'project_generator':
        # Right now, project_generator is building the solution for visual studio, so we will set the BINFOLDER value
        # based on the win64 output folder.  In the future, if we are generating project files for other platforms
        # we need to select the appropriate out_folder_xxxx for that platform
        bin_folder_name = 'BINFOLDER_NAME="{}"'.format(self.options.out_folder_win64)
    else:
        bin_folder_name = 'BINFOLDER_NAME="{}"'.format(get_output_folder_name(self, platform, configuration))

    return [bin_folder_name]

###############################################################################
@conf
def get_output_folders(self, platform, configuration):

    path = get_output_folder_name(self, platform, configuration)

    # Correct handling for absolute paths
    if os.path.isabs(path):
        output_folder_nodes = [self.root.make_node(path)]
    else:
        # For relative path, prefix binary output folder with game project folder
        output_folder_nodes = []
        output_folder_nodes += [self.path.make_node(path)]
    return output_folder_nodes


@conf
def read_file_list(bld, file):
    file_node = bld.path.make_node(file)

    return bld.parse_json_file(file_node)

@conf
def get_platform_and_configuration(bld):
    # Assume that the configuration is at the end:
    # Keep 'dedicated' as part of the configuration.
    # extreme example: 'win_x64_vs2012_debug_dedicated'

    elements = bld.variant.split('_')
    idx = -1
    if 'dedicated' in elements:
        idx -= 1
    if 'test' in elements:
        idx -= 1

    platform = '_'.join(elements[:idx])
    configuration = '_'.join(elements[idx:])

    return (platform, configuration)


###############################################################################
@conf
def target_clean(self):

    tmp_targets = self.options.targets[:]
    to_delete = []
    # Sort of recursive algorithm, find all outputs of targets
    # Repeat if new targets were added due to use directives
    while len(tmp_targets) > 0:
        new_targets = []

        for tgen in self.get_all_task_gen():
            tgen.post()
            if not tgen.target in tmp_targets:
                continue

            for t in tgen.tasks:
                # Collect outputs
                for n in t.outputs:
                    if n.is_child_of(self.bldnode):
                        to_delete.append(n)
            # Check for use flag
            if hasattr(tgen, 'use'):
                new_targets.append(tgen.use)
        # Copy new targets
        tmp_targets = new_targets[:]

    # Final File list to delete
    to_delete = list(set(to_delete))
    for file_to_delete in to_delete:
        file_to_delete.delete()

###############################################################################
@conf
def clean_output_targets(self):

    to_delete = []

    for base_output_folder_node in self.get_output_folders(self.env['PLATFORM'],self.env['CONFIGURATION']):

        # Go through the task generators
        for tgen in self.get_all_task_gen():

            is_msvc = tgen.env['CXX_NAME'] == 'msvc'

            # collect only shlibs and programs
            if not hasattr(tgen,'_type') or not hasattr(tgen,'env'):
                continue

            # determine the proper target extension pattern
            if tgen._type=='shlib' and tgen.env['cxxshlib_PATTERN']!='':
                target_ext_PATTERN=tgen.env['cxxshlib_PATTERN']
            elif tgen._type=='program' and tgen.env['cxxprogram_PATTERN']!='':
                target_ext_PATTERN=tgen.env['cxxprogram_PATTERN']
            else:
                continue

            target_output_folder_nodes = []

            # Determine if there is a sub folder
            if hasattr(tgen, 'output_sub_folder'):
                target_output_folder_nodes.append(base_output_folder_node.make_node(tgen.output_sub_folder))
            else:
                target_output_folder_nodes.append(base_output_folder_node)

            if hasattr(tgen, 'output_folder'):
                target_output_folder_nodes.append(tgen.bld.root.make_node(tgen.output_folder))

            # Determine if there are copy sub folders
            target_output_copy_folder_items = []
            target_output_copy_folder_attr = getattr(tgen, 'output_sub_folder_copy', None)
            if isinstance(target_output_copy_folder_attr, str):
                target_output_copy_folder_items.append(base_output_folder_node.make_node(target_output_copy_folder_attr))
            elif isinstance(target_output_copy_folder_attr, list):
                for target_output_copy_folder_attr_item in target_output_copy_folder_attr:
                    if isinstance(target_output_copy_folder_attr_item, str):
                        target_output_copy_folder_items.append(base_output_folder_node.make_node(target_output_copy_folder_attr_item))

            for target_output_folder_node in target_output_folder_nodes:

                target_name = getattr(tgen, 'output_file_name', tgen.get_name())
                delete_target = target_output_folder_node.make_node(target_ext_PATTERN % str(target_name))
                to_delete.append(delete_target)
                for target_output_copy_folder_item in target_output_copy_folder_items:
                    delete_target_copy = target_output_copy_folder_item.make_node(target_ext_PATTERN % str(target_name))
                    to_delete.append(delete_target_copy)

                # If this is an MSVC build, add pdb cleaning just in case
                if is_msvc:
                    delete_target_pdb = target_output_folder_node.make_node('%s.pdb' % str(target_name))
                    to_delete.append(delete_target_pdb)
                    for target_output_copy_folder_item in target_output_copy_folder_items:
                        delete_target_copy = target_output_copy_folder_item.make_node('%s.pdb' % str(target_name))
                        to_delete.append(delete_target_copy)

        # Go through GEMS and add possible gems components
        gems_output_names = self.GetGemsOutputModuleNames()

        gems_target_ext_PATTERN=self.env['cxxshlib_PATTERN']

        for gems_output_name in gems_output_names:
            gems_delete_target = base_output_folder_node.make_node(gems_target_ext_PATTERN % gems_output_name)
            to_delete.append(gems_delete_target)
            if is_msvc:
                gems_delete_target_pdb = base_output_folder_node.make_node('%s.pdb' % str(gems_output_name))
                to_delete.append(gems_delete_target_pdb)

    for file in to_delete:
        if os.path.exists(file.abspath()):
            try:
                if self.options.verbose >= 1:
                    Logs.info('Deleting {0}'.format(file.abspath()))
                file.delete()
            except:
                Logs.warn("Unable to delete {0}".format(file.abspath()))

###############################################################################
# Copy pasted from MSVS..
def convert_vs_configuration_to_waf_configuration(configuration):
    dedicated = ''
    test = ''
    if '_dedicated' in configuration:
        dedicated = '_dedicated'
    if '_test' in configuration:
        test = '_test'

    if 'Debug' in configuration:
        return 'debug' + test + dedicated
    if 'Profile' in configuration:
        return 'profile' + test + dedicated
    if 'Release' in configuration:
        return 'release' + test + dedicated
    if 'Performance' in configuration:
        return 'performance' + test + dedicated

    return ''


@feature('link_to_output_folder')
@after_method('process_source')
def link_to_output_folder(self):
    """
    Task Generator for tasks which generate symbolic links from the source to the dest folder
    """
    return # Disabled for now

    if self.bld.env['PLATFORM'] == 'project_generator':
        return # Dont create links during project generation

    if sys.getwindowsversion()[0] < 6:
        self.bld.fatal('not supported')

    # Compute base relative path (from <Root> to taskgen wscript
    relative_base_path = self.path.path_from(self.bld.path)

    # TODO: Need to handle absolute path here correctly
    spec_name = self.bld.options.project_spec
    for project in self.bld.active_projects(spec_name):
        project_folder = self.bld.project_output_folder(project)
        for file in self.source:
            # Build output folder
            relativ_file_path = file.path_from(self.path)

            output_node = self.bld.path.make_node(project_folder)
            output_node = output_node.make_node(relative_base_path)
            output_node = output_node.make_node(relativ_file_path)

            path = os.path.dirname( output_node.abspath() )
            if not os.path.exists( path ):
                os.makedirs( path )

            self.create_task('create_symbol_link', file, output_node)

import ctypes
###############################################################################
# Class to handle copying of the final outputs into the Bin folder
class create_symbol_link(Task):
    color = 'YELLOW'

    def run(self):
        src = self.inputs[0].abspath()
        tgt = self.outputs[0].abspath()

        # Make output writeable
        try:
            os.chmod(tgt, 493) # 0755
        except:
            pass

        try:
            kdll = ctypes.windll.LoadLibrary("kernel32.dll")
            res = kdll.CreateSymbolicLinkA(tgt, src, 0)
        except:
            self.generator.bld.fatal("File Link Error (%s -> %s( (%s)" % (src, tgt, sys.exc_info()[0]))

        return 0

    def runnable_status(self):
        if super(create_symbol_link, self).runnable_status() == -1:
            return -1

        src = self.inputs[0].abspath()
        tgt = self.outputs[0].abspath()

        # If there any target file is missing, we have to copy
        try:
            stat_tgt = os.stat(tgt)
        except OSError:
            return RUN_ME

        # Now compare both file stats
        try:
            stat_src = os.stat(src)
        except OSError:
            pass
        else:
            # same size and identical timestamps -> make no copy
            if stat_src.st_mtime >= stat_tgt.st_mtime + 2:
                return RUN_ME

        # Everything fine, we can skip this task
        return SKIP_ME


###############################################################################
@feature('cxx', 'c', 'cprogram', 'cxxprogram', 'cshlib', 'cxxshlib', 'cstlib', 'cxxstlib')
@after_method('apply_link')
def add_compiler_dependency(self):
    """ Helper function to ensure each compile task depends on the compiler """
    if self.env['PLATFORM'] == 'project_generator':
        return

    # Create nodes for compiler and linker
    c_compiler_node = self.bld.root.make_node( self.env['CC'] )
    cxx_compiler_node = self.bld.root.make_node( self.env['CXX'] )
    linker_node = self.bld.root.make_node( self.env['LINK'] )

    # Let all compile tasks depend on the compiler
    for t in getattr(self, 'compiled_tasks', []):
        if os.path.isabs( self.env['CC'] ):
            t.dep_nodes.append(c_compiler_node)
        if os.path.isabs( self.env['CXX'] ):
            t.dep_nodes.append(cxx_compiler_node)

    # Let all link tasks depend on the linker
    if getattr(self, 'link_task', None):
        if os.path.isabs(  self.env['LINK'] ):
            self.link_task.dep_nodes.append(linker_node)


###############################################################################
@taskgen_method
def copy_dependent_objects(self, source_file,source_node,target_node,source_exclusion_list,build_folder_tree_only = False,flatten_target = False):

    flatten_subfolder = ''
    if isinstance(source_file, str):
        source_file_name = source_file
    elif isinstance(source_file, tuple):
        source_file_name = source_file[0]
        flatten_subfolder = source_file[1]+'/'

    if not source_node.abspath() == target_node.abspath():

        source_file_node = source_node.make_node(source_file_name)
        if flatten_target:
            source_path_path, source_path_filename = os.path.split(source_file_name)
            target_file_node = target_node.make_node(flatten_subfolder + source_path_filename)
        else:
            target_file_node = target_node.make_node(source_file_name)
        target_node_root_path = target_node.abspath()
        source_file_path = source_file_node.abspath()

        # Check if this is a file pattern, if so, glob the list of files
        if '*' in source_file_path:
            # Collect the results of the glob
            glob_items = glob.glob(source_file_path)
            for glob_item in glob_items:

                # For each result of the glob item, determine the subpath, which is the path of the source minus the root source node
                # e.g. if the source node root is c:/source and the glob result file is c:/source/path_a/file_b.xml, then the subpath would be
                # path_a
                glob_sub_path = os.path.dirname(glob_item[len(source_node.abspath())+1:])

                if not flatten_target:
                    # Calculate the glob target directory by taking the glob result (source) and replacing the source root node with the target root
                    # node's path.  e.g. c:/source/path_a -> c:/target/path_a and then create the folder if its missing.
                    glob_target_dir = os.path.dirname(os.path.join(target_node_root_path,glob_item[:len(source_file_path)]))
                    if not os.path.exists( glob_target_dir ):
                        os.makedirs( glob_target_dir )

                # Get the raw GLOB'ed filename
                glob_item_file_name = os.path.basename(glob_item)

                # Construct the source and target node based on the actual result glob'd file and recursively call this method again
                glob_source_node = source_node.make_node(glob_sub_path)
                if not flatten_target:
                    glob_target_node = target_node.make_node(glob_sub_path)
                else:
                    glob_target_node = target_node

                self.copy_dependent_objects(glob_item_file_name,glob_source_node,glob_target_node,source_exclusion_list,build_folder_tree_only,flatten_target)

        # Check if this is a file, perform the file copy task if needed
        elif os.path.isfile(source_file_path):
            # Make sure that the base folder for the file exists
            file_item_target_path = os.path.dirname(target_file_node.abspath())
            if not os.path.exists( file_item_target_path ):
                os.makedirs( file_item_target_path )

            if not build_folder_tree_only:
                self.create_copy_task(source_file_node, target_file_node)
        # Check if this is a folder, make sure the path exists and recursively act on the folder
        elif os.path.isdir(source_file_path):
            folder_items = os.listdir(source_file_path)

            # Make sure the target path will exist so we can recursively copy into it
            target_sub_node = target_file_node
            target_sub_node_path = target_sub_node.abspath()

            if not os.path.exists( target_sub_node_path ):
                os.makedirs( target_sub_node_path )

            for sub_item in folder_items:
                # if flatten_target:
                #     self.copy_dependent_objects(sub_item,source_file_node,target_node,source_exclusion_list,build_folder_tree_only,flatten_target)
                # else:
                self.copy_dependent_objects(sub_item,source_file_node,target_sub_node,source_exclusion_list,build_folder_tree_only,flatten_target)



###############################################################################
# Function to generate the copy tasks to the target Bin64(.XXX) folder.  This will
# take any collection of source artifacts and copy them flattened into the Bin64(.XXX) target folder
@after_method('add_install_copy')
@feature('c', 'cxx', 'copy_artifacts')
def add_copy_artifacts(self):
    if self.bld.env['PLATFORM'] == 'project_generator':
        return

    include_source_artifacts =  self.env['SOURCE_ARTIFACTS_INCLUDE']
    exclude_source_artifacts =  self.env['SOURCE_ARTIFACTS_EXCLUDE']
    current_platform = self.bld.env['PLATFORM']
    current_configuration = self.bld.env['CONFIGURATION']

    # Special case, if the platform is win_x64 and the configuration is debug, there are dependent files that need to be copied over
    if not self.env['SOURCE_ARTIFACTS_INCLUDE'] is None:

        if include_source_artifacts:

            # source node is Bin64 for win_x64 platform
            source_node = source_root_path = self.bld.path.make_node('')

            output_sub_folder = getattr(self, 'output_sub_folder', None)
            # target node
            for target_node in self.bld.get_output_folders(current_platform,current_configuration):
                if output_sub_folder:
                    target_node = target_node.make_node(output_sub_folder)

                # Skip if the source and target folders are the same
                if source_node.abspath() != target_node.abspath():
                    for dependent_files in include_source_artifacts:
                        self.copy_dependent_objects(dependent_files,source_node,target_node,exclude_source_artifacts,False,True)


###############################################################################
# Function to generate the copy tasks for mirroring artifacts from Bin64.  This will take files that
# are relative to the base Bin64 folder and mirror them (copying them along with their folder structure)
# to any target Target folder (such as Bin64.Debug)
@after_method('add_install_copy')
@feature('c', 'cxx')
def add_mirror_artifacts(self):
    if self.bld.env['PLATFORM'] == 'project_generator':
        return

    artifact_include_files =  self.env['MIRROR_ARTIFACTS_INCLUDE']
    artifact_exclude_files =  self.env['MIRROR_ARTIFACTS_EXCLUDE']
    current_platform = self.bld.env['PLATFORM']
    current_configuration = self.bld.env['CONFIGURATION']

    # if the platform is win_x64 and the output folder is not Bin64, copy over any configured artifact from the source
    # Bin64 folder to the destination output folder if it is not the same as the source Bin64 folder
    if (artifact_include_files is not None):

        if len(artifact_include_files)==0:
            return

        # source node is Bin64 for win_x64 platform
        source_node = self.bld.path.make_node('Bin64')

        output_folders = self.bld.get_output_folders(current_platform, current_configuration)
        for target_node in output_folders:

            # Skip if the output folder is Bin64 already (this is where the source is)
            if target_node.abspath() == source_node.abspath():
                continue

            # Build the file exclusion list based off of the source node
            exclusion_abs_path_list = set()
            for exclude_file in artifact_exclude_files:
                normalized_exclude = os.path.normpath(os.path.join(source_node.abspath(),exclude_file))
                exclusion_abs_path_list.add(normalized_exclude)

            # Copy each file/folder in the collection
            # (first pass create the tree structure)
            for dependent_files in artifact_include_files:
                self.copy_dependent_objects(dependent_files,source_node,target_node,exclusion_abs_path_list,True)
            # (second pass create the tree structure)
            for dependent_files in artifact_include_files:
                self.copy_dependent_objects(dependent_files,source_node,target_node,exclusion_abs_path_list,False)
