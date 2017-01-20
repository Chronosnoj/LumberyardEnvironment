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
from waflib.Configure import conf
from waflib.TaskGen import feature, before_method, after_method
from waflib import Utils, Logs, Errors

import os
from cry_utils import append_kw_entry, append_to_unique_list, sanitize_kw_input_lists, clean_duplicates_in_list, prepend_kw_entry
import re
from collections import defaultdict
from waflib import Context
from waf_branch_spec import PLATFORMS, CONFIGURATIONS, CONFIGURATION_SHORTCUT_ALIASES
import json
from lumberyard_sdks import add_aws_native_sdk_platform_defines

def get_common_inputs():
    inputs = [
        'additional_settings',
        'export_definitions',
        'meta_includes',
        'file_list',
        'use',              # module dependency
        'defines',
        'export_defines',
        'includes',
        'export_includes',
        'cxxflags',
        'cflags',
        'lib',              # shared
        'libpath',          # shared
        'stlib',            # static
        'stlibpath',        # static
        'linkflags',
        'framework',
        'frameworkpath',
        'rpath',
        'features',
        'enable_rtti',
        'remove_release_define',
        'uselib',
        'mirror_artifacts_to_include',
        'mirror_artifacts_to_exclude'
    ]
    return inputs

def get_all_supported_platforms():
    all_supported_platforms = [
        'android',
        'android_armv7_clang',
        'android_armv7_gcc',
        'appletv',
        'darwin',
        'darwin_x64',
        'darwin_x86',
        'durango',
        'ios',
        'linux',
        'linux_x64',
        'orbis',
        'win',
        'win_x64'
    ]
    return all_supported_platforms

def GetConfiguration(ctx, target):
    if target in ctx.env['CONFIG_OVERWRITES']:
        return ctx.env['CONFIG_OVERWRITES'][target]
    return ctx.env['CONFIGURATION']

@conf
def get_platform_list(self, platform ):
    """
    Util-function to find valid platform aliases for the current platform
    """
    if platform in ['win_x86', 'win_x64', 'win_x64_vs2012', 'win_x64_vs2010']:
        return [platform, 'win']
    if platform in ['linux_x64']:
        return [platform, 'linux_x64', 'linux']
    if platform in ['darwin_x86', 'darwin_x64']:
        return [platform, 'darwin']
    if platform in ['durango']:
        return ['durango']
    if platform in ['ios']:
        return ['ios']
    if platform in ['appletv']:
        return ['appletv']
    if platform in ['android_armv7_gcc', 'android_armv7_clang']:
        return [platform, 'android']
    if platform in ('project_generator', []):
        # check the build context current target
        if hasattr(self, 'get_target_platforms'):
            return self.get_target_platforms()
        # else the project generator doesn't care about the environment
        else:
            return get_all_supported_platforms()

    # Always return a list, even if there is no alias
    if platform == []:
        return []
    return [ platform ]


@conf
def is_windows_platform(self, platform):
    '''Checks to see if the platform is a windows platform. Returns true if it is, otherwise false'''
    platform_names = get_platform_list(self, platform)
    for name in platform_names:
        if 'win' == name:
            return True

    return False


#############################################################################
def SanitizeInput(ctx, kw):

    # Sanitize the inputs kws that should be list inputs
    inputs = get_common_inputs()
    inputs += [
        'output_file_name',
        'files',
        'winres_includes',
        'winres_defines'
    ]
    sanitize_kw_input_lists(inputs, kw)

    # Recurse for additional settings
    if 'additional_settings' in kw:
        for setting in kw['additional_settings']:
            SanitizeInput(ctx, setting)


def RegisterVisualStudioFilter(ctx, kw):
    """
    Util-function to register each provided visual studio filter parameter in a central lookup map
    """
    if not 'vs_filter' in kw:
        ctx.fatal('Mandatory "vs_filter" task generater parameter missing in %s/wscript' % ctx.path.abspath())

    if not hasattr(ctx, 'vs_project_filters'):
        ctx.vs_project_filters = {}

    ctx.vs_project_filters[ kw['target' ] ] = kw['vs_filter']

def AssignTaskGeneratorIdx(ctx, kw):
    """
    Util-function to assing a unique idx to prevent concurrency issues when two task generator output the same file.
    """
    if not hasattr(ctx, 'index_counter'):
        ctx.index_counter = 0
    if not hasattr(ctx, 'index_map'):
        ctx.index_map = {}

    # Use a path to the wscript and the actual taskgenerator target as a unqiue key
    key = ctx.path.abspath() + '___' + kw['target']

    if key in ctx.index_map:
        kw['idx'] = ctx.index_map.get(key)
    else:
        ctx.index_counter += 1
        kw['idx'] = ctx.index_map[key] = ctx.index_counter

    append_kw_entry(kw,'features','parse_vcxproj')

def SetupRunTimeLibraries(ctx, kw, overwrite_settings = None):
    """
    Util-function to set the correct flags and defines for the runtime CRT (and to keep non windows defines in sync with windows defines)
    By default CryEngine uses the "Multithreaded, dynamic link" variant (/MD)
    """
    runtime_crt = 'dynamic'                     # Global Setting
    if overwrite_settings:                      # Setting per Task Generator Type
        runtime_crt = overwrite_settings
    if kw.get('force_static_crt', False):       # Setting per Task Generator
        runtime_crt = 'static'
    if kw.get('force_dynamic_crt', False):      # Setting per Task Generator
        runtime_crt = 'dynamic'

    if runtime_crt != 'static' and runtime_crt != 'dynamic':
        ctx.fatal('Invalid Settings: "%s" for runtime_crt' % runtime_crt )

    crt_flag = []
    config = GetConfiguration(ctx, kw['target'])

    if runtime_crt == 'static':
        append_kw_entry(kw,'defines',['_MT'])
        if ctx.env['CC_NAME'] == 'msvc':
            if 'debug' in config:
                crt_flag = [ '/MTd' ]
            else:
                crt_flag    = [ '/MT' ]
    else: # runtime_crt == 'dynamic':
        append_kw_entry(kw,'defines',[ '_MT', '_DLL' ])
        if ctx.env['CC_NAME'] == 'msvc':
            if 'debug' in config:
                crt_flag = [ '/MDd' ]
            else:
                crt_flag    = [ '/MD' ]

    append_kw_entry(kw,'cflags',crt_flag)
    append_kw_entry(kw,'cxxflags',crt_flag)


def TrackFileListChanges(ctx, kw):
    """
    Util function to ensure file lists are correctly tracked regardless of current target platform
    """
    def _to_list( value ):
        """ Helper function to ensure a value is always a list """
        if isinstance(value,list):
            return value
        return [ value ]

    files_to_track = []
    kw['waf_file_entries'] = []

    # Collect all file list entries
    for (key,value) in kw.items():
        if 'file_list' in key:
            files_to_track += _to_list(value)
        # Collect potential file lists from addional options
        if 'additional_settings' in key:
            for settings_container in kw['additional_settings']:
                for (key2,value2) in settings_container.items():
                    if 'file_list' in key2:
                        files_to_track += _to_list(value2)

    # Remove duplicates
    files_to_track = list(set(files_to_track))

    # Add results to global lists
    for file in files_to_track:
        file_node = ctx.path.make_node(file)
        if not hasattr(ctx, 'addional_files_to_track'):
            ctx.addional_files_to_track = []
        ctx.addional_files_to_track += [ file_node ]
        append_kw_entry(kw,'waf_file_entries',[ file_node ])

def LoadFileLists(ctx, kw, file_lists):
    """
    Util function to extract a list of needed source files, based on uber files and current command
    It expects that kw['file_list'] points to a valid file, containing a JSON file with the following mapping:
    Dict[ <UberFile> -> Dict[ <Project Filter> -> List[Files] ] ]
    """
    def _MergeFileList(in_0, in_1):
        """ Merge two file lists """
        result = dict(in_0)

        for (uber_file,project_filter) in in_1.items():
            for (filter_name,file_list) in project_filter.items():
                for file in file_list:
                    if not uber_file in result:
                        result[uber_file] = {}
                    if not filter_name in result[uber_file]:
                        result[uber_file][filter_name] = []
                    result[uber_file][filter_name].append(file)
        return result

    def _DisableUberFile(ctx, project_filter_list, files_marked_for_exclusion):
        for (filter_name, file_list) in project_filter_list.items():
            if any(ctx.path.make_node(file).abspath().lower() in files_marked_for_exclusion for file in file_list): # if file in exclusion list
                return True
        return False

    task_generator_files        = set() # set of all files in this task generator (store as abspath to be case insenstive)

    file_to_project_filter  = {}
    uber_file_to_file_list  = {}
    file_list_to_source     = {}
    file_list_content       = {}

    source_files            = set()
    no_uber_file_files      = set()
    header_files            = set()
    objc_source_files       = set()
    qt_source_files         = set()
    resource_files          = set()
    plist_files             = set()
    uber_files              = set()
    other_files             = set()
    uber_file_relative_list = set()

    target                  = kw['target']
    found_pch               = False
    pch_file                = kw.get('pch', '')
    platform                = ctx.env['PLATFORM']
    uber_file_folder        = ctx.get_bintemp_folder_node().make_node('uber_files/{}'.format(target))

    # Keep track of files per waf_file spec and be ready to identify duplicate ones per file
    file_list_to_file_collection = dict()
    file_list_to_duplicate_file_collection = dict()
    has_duplicate_files = False

    # Apply project override
    disable_uber_files_for_project = ctx.get_project_overrides(target).get('exclude_from_uber_file', False)

    files_marked_for_uber_file_exclusion = []
    if not disable_uber_files_for_project:
        for key, value in ctx.get_file_overrides(target).iteritems():
            if value.get('exclude_from_uber_file', False):
                files_marked_for_uber_file_exclusion.append(key)

    # Load file lists   and build all needed lookup lists
    for file_list_file in file_lists:

        # Prevent processing the same file twice
        if file_list_file in file_list_to_file_collection:
            continue
        # Prepare to collect the files per waf_file spec
        file_list_to_file_collection[file_list_file] = set()
        file_list_to_duplicate_file_collection[file_list_file] = set()

        # Read *.waf_file from disc
        file_list                   = ctx.read_file_list( file_list_file )

        # configure uber files
        if not disable_uber_files_for_project:
            # if there's a wart on this filename, use it as the token for generating uber file names
            # e.g. AzFramework_win.waf_files -> _win
            file_list_token = re.sub(target, '', file_list_file, flags=re.IGNORECASE).replace('.waf_files', '')
            file_list = ctx.map_uber_files(file_list, file_list_token, target)

        file_list_content = _MergeFileList(file_list_content, file_list)

        # Build various mappings/lists based in file just
        for (uber_file, project_filter_list) in file_list.items():

            # Disable uber file usage if defined by override parameter
            disable_uber_file = disable_uber_files_for_project or _DisableUberFile(ctx, project_filter_list, files_marked_for_uber_file_exclusion)

            if disable_uber_file:
                Logs.debug('[Option Override] - %s - Disabled uber file "%s"' %(target, uber_file))

            generate_uber_file = uber_file != 'none' and uber_file != 'NoUberFile' and not disable_uber_file # TODO: Deprecate 'NoUberfile'
            if generate_uber_file:
                # Collect Uber file related informations
                uber_file_node = uber_file_folder.make_node(uber_file)
                uber_file_relative = uber_file_node.path_from(ctx.path)

                if uber_file in uber_files:
                    ctx.cry_file_error('[%s] UberFile "%s" was specifed twice. Please choose a different name' % (kw['target'], uber_file), file_list_file)

                task_generator_files.add(uber_file_node.abspath().lower())
                uber_files.add(uber_file)
                uber_file_relative_list.add(uber_file_relative)
                file_to_project_filter[uber_file_node.abspath()] = 'Uber Files'

            for (filter_name, file_entries) in project_filter_list.items():
                for file in file_entries:
                    file_node = ctx.path.make_node(file)

                    filenode_abs_path = file_node.abspath().lower()

                    # Keep track of files for file_list file and track which ones are duplicate
                    if (filenode_abs_path in file_list_to_file_collection[file_list_file]):
                        file_list_to_duplicate_file_collection[file_list_file].add(filenode_abs_path)
                        has_duplicate_files = True
                    else:
                        file_list_to_file_collection[file_list_file].add(filenode_abs_path)

                    task_generator_files.add(filenode_abs_path)

                    # Collect per file information
                    if file == pch_file:
                        # PCHs are not compiled with the normal compilation, hence don't collect them
                        found_pch = True

                    elif file.endswith(('.c', '.C', '.cpp', '.CPP')):
                        source_files.add(file)
                        if not generate_uber_file:
                            no_uber_file_files.add(file)
                    elif file.endswith(('.mm', '.m')):
                        objc_source_files.add(file)
                    elif file.endswith(('.ui', '.qrc', '.ts')):
                        qt_source_files.add(file)
                    elif file.endswith(('.h', '.H', '.hpp', '.HPP', '.hxx', '.HXX')):
                        header_files.add(file)
                    elif file.endswith(('.rc', '.r')):
                        resource_files.add(file)
                    elif file.endswith('.plist'):
                        plist_files.add(file)
                    else:
                        other_files.add(file)

                    # Build file name -> Visual Studio Filter mapping
                    file_to_project_filter[file_node.abspath()] = filter_name

                    # Build list of uber files to files
                    if generate_uber_file:
                        uber_file_abspath = uber_file_node.abspath()
                        if not uber_file_abspath in uber_file_to_file_list:
                            uber_file_to_file_list[uber_file_abspath]   = []
                        uber_file_to_file_list[uber_file_abspath]       += [ file_node ]

            # Remember which sources come from which file list (for later lookup)
            file_list_to_source[file_list_file] = list(source_files | qt_source_files)

    # Report any files that were duplicated within a file_list spec
    if has_duplicate_files:
        for (error_file_list,error_file_set) in file_list_to_duplicate_file_collection.items():
            if len(error_file_set) > 0:
                for error_file in error_file_set:
                    Logs.error('[ERROR] file "%s" was specifed more than once in file spec %s' % (str(error_file), error_file_list))
        ctx.fatal('[ERROR] One or more files errors detected for target %s.' % (kw['target']))

    # Compute final source list based on platform
    if platform == 'project_generator' or ctx.options.file_filter != "":
        # Collect all files plus uber files for project generators and when doing a single file compilation
        kw['source'] = uber_file_relative_list | source_files | qt_source_files | objc_source_files | header_files | resource_files | other_files
        kw['mac_plist'] = list(plist_files)
        if len(plist_files) != 0:
            if 'darwin' in ctx.cmd or 'mac' in ctx.cmd:
                kw['mac_app'] = True
            elif 'appletv' in ctx.cmd:
                kw['appletv_app'] = True
            else:
                kw['ios_app'] = True

        if platform == 'project_generator' and pch_file != '':
            kw['source'].add(pch_file) # Also collect PCH for project generators

    else:
        # Regular compilation path
        if ctx.is_option_true('use_uber_files'):
            # Only take uber files when uber files are enabled and files not using uber files
            kw['source'] = uber_file_relative_list | no_uber_file_files | qt_source_files
        else:
            # Fall back to pure list of source files
            kw['source'] = source_files | qt_source_files

        # Append platform specific files
        if 'darwin' in ctx.get_platform_list( platform ) or 'ios' in ctx.get_platform_list( platform ) or 'appletv' in ctx.get_platform_list( platform ):
            kw['source'] |= objc_source_files
            kw['mac_plist'] = list(plist_files)
            if len(plist_files) != 0:
                if 'darwin' in ctx.cmd or 'mac' in ctx.cmd:
                    kw['mac_app'] = True
                elif 'appletv' in ctx.cmd:
                    kw['appletv_app'] = True
                else:
                    kw['ios_app'] = True

        if ctx.is_windows_platform(ctx.env['PLATFORM']):
            kw['source'] |= resource_files

    # Handle PCH files
    if pch_file != '' and found_pch == False:
        # PCH specified but not found
        ctx.cry_file_error('[%s] Could not find PCH file "%s" in provided file list (%s).\nPlease verify that the name of the pch is the same as provided in a WAF file and that the PCH is not stored in an UberFile.' % (kw['target'], pch_file, ', '.join(file_lists)), 'wscript' )

    # Try some heuristic when to use PCH files
    #if ctx.is_option_true('use_uber_files') and found_pch and len(uber_file_relative_list) > 0 and ctx.options.file_filter == "" and ctx.cmd != 'generate_uber_files':
        # Disable PCH files when having UberFiles as they  bring the same benefit in this case
        #kw['pch_name'] = kw['pch']
        #del kw['pch']

    # Store global lists in context
    kw['task_generator_files']  = task_generator_files
    kw['file_list_content']     = file_list_content
    kw['project_filter']        = file_to_project_filter
    kw['uber_file_lookup']      = uber_file_to_file_list
    kw['file_list_to_source']   = file_list_to_source
    kw['header_files']          = header_files

    # Note: Always sort the file list so that its stable between recompilation.  WAF uses the order of input files to generate the UID of the task, 
    # and if the order changes, it will cause recompilation!
    if (platform != 'project_generator'):
        kw['source'] = sorted(kw['source'])
        kw['header_files'] = sorted(kw['header_files'])

    # save sources and header_files as lists.  WAF appears to work just fine as sets, but historically these were lists, so I'm minimizing impact.
    kw['source'] = list(kw['source'])
    kw['header_files'] = list(kw['header_files'])

def VerifyInput(ctx, kw):
    """
    Helper function to verify passed input values
    """

    # 'target' is required
    target_name = kw['target']

    wscript_file = ctx.path.make_node('wscript').abspath()
    if kw['file_list'] == []:
        ctx.cry_file_error('TaskGenerator "%s" is missing mandatory parameter "file_list"' % target_name, wscript_file )

    if 'source' in kw:
        ctx.cry_file_error('TaskGenerator "%s" is using unsupported parameter "source", please use "file_list"' % target_name, wscript_file )

    # Loop through and check the follow type of keys that represent paths and validate that they exist on the system.
    # If they do not exist, this does not mean its an error, but we want to be able to display a warning instead as
    # having unnecessary paths in the include paths affects build performance
    path_check_key_values = ['includes','libpath']

    # Validate the paths only during the build command execution and exist in the spec
    if ctx.cmd.startswith('build'):

        if ctx.is_target_enabled(target_name):

            # Validate the include paths and show warnings for ones that dont exist (only for the currently building platform
            # and only during the build command execution)
            current_platform, current_configuration = ctx.get_platform_and_configuration()

            # Special case: If the platform is win_x64, reduce it to win
            if current_platform == 'win_x64':
                current_platform = 'win'

            # Search for the keywords in 'path_check_key_values'
            for kw_check in kw.keys():
                for path_check_key in path_check_key_values:
                    if kw_check == path_check_key or (kw_check.endswith('_' + path_check_key) and kw_check.startswith(current_platform)):
                        path_check_values = kw[kw_check]
                        if path_check_values is not None:
                            # Make sure we are working with a list of strings, not a string
                            if isinstance(path_check_values,str):
                                path_check_values = [path_check_values]
                            for path_check in path_check_values:
                                if isinstance(path_check,str):
                                    # If the path input is a string, derive the absolute path for the input path
                                    path_to_validate = os.path.join(ctx.path.abspath(),path_check)
                                else:
                                    # If the path is a Node object, get its absolute path
                                    path_to_validate = path_check.abspath()

                                if not os.path.exists(path_to_validate):
                                    Logs.warn('[WARNING] \'{}\' value \'{}\' defined in TaskGenerator "{}" does not exist'.format(kw_check,path_to_validate,target_name))


def InitializeTaskGenerator(ctx, kw):
    """
    Helper function to call all initialization routines requiered for a task generator
    """
    SanitizeInput(ctx, kw)
    VerifyInput(ctx, kw)
    AssignTaskGeneratorIdx(ctx, kw)
    RegisterVisualStudioFilter(ctx, kw)
    TrackFileListChanges(ctx, kw)


def apply_cryengine_module_defines(ctx, kw):

    additional_defines = ctx.get_binfolder_defines()
    ctx.add_aws_native_sdk_platform_defines(additional_defines)
    additional_defines.append('LY_BUILD={}'.format(ctx.get_lumberyard_build()))
    append_kw_entry(kw, 'defines', additional_defines)


# Append any common static modules to the configuration
def AppendCommonModules(ctx,kw):

    common_modules_dependencies = []

    if not 'use' in kw:
        kw['use'] = []

    # Append common module's dependencies
    if any(p == ctx.env['PLATFORM'] for p in ('win_x86', 'win_x64', 'durango')):
        common_modules_dependencies = ['bcrypt']

    if 'test' in ctx.env['CONFIGURATION'] or 'project_generator' == ctx.env['PLATFORM']:
        append_to_unique_list(kw['use'], 'AzTest')

    append_kw_entry(kw,'lib',common_modules_dependencies)

def LoadAddionalFileSettings(ctx, kw):
    """
    Load all settings from the addional_settings parameter, and store them in a lookup map
    """
    append_kw_entry(kw,'features',[ 'apply_additional_settings' ])
    kw['file_specifc_settings'] = {}

    for setting in kw['additional_settings']:

        setting['target'] = kw['target'] # reuse target name

        file_list = []

        if 'file_list' in setting:
            # Option A: The files are specifed as a *.waf_files (which is loaded already)
            for list in setting['file_list']:
                file_list += kw['file_list_to_source'][list]

        if 'files' in setting:
            # Option B: The files are already specified as an list
            file_list += setting['files']

        if 'regex' in setting:
            # Option C: A regex is specifed to match the files
            p = re.compile(setting['regex'])

            for file in kw['source']:
                if p.match(file):
                    file_list += [file]

        # insert files into lookup dictonary, but make sure no uber file and no file within an uber file is specified
        uber_file_folder = ctx.bldnode.make_node('..')
        uber_file_folder = uber_file_folder.make_node('uber_files')
        uber_file_folder = uber_file_folder.make_node(kw['target'])

        for file in file_list:
            file_abspath = ctx.path.make_node(file).abspath()
            uber_file_abspath = uber_file_folder.make_node(file).abspath()

            if 'uber_file_lookup' in kw:
                for uber_file in kw['uber_file_lookup']:

                    # Uber files are not allowed for additional settings
                    if uber_file_abspath == uber_file:
                        ctx.cry_file_error("Additional File Settings are not supported for UberFiles (%s) to ensure a consistent behavior without UberFiles, please adjust your setup" % file, ctx.path.make_node('wscript').abspath())

                    for entry in kw['uber_file_lookup'][uber_file]:
                        if file_abspath == entry.abspath():
                            ctx.cry_file_error("Additional File Settings are not supported for file using UberFiles (%s) to ensure a consistent behavior without UberFiles, please adjust your setup" % file, ctx.path.make_node('wscript').abspath())

            # All fine, add file name to dictonary
            kw['file_specifc_settings'][file_abspath] = setting
            setting['source'] = []

def ConfigureTaskGenerator(ctx, kw):

    """
    Helper function to apply default configurations and to set platform/configuration dependent settings
    """

    target = kw['target']

    # Ensure we have a name for lookup purposes
    if 'name' not in kw:
        kw['name'] = target

    # Apply all settings, based on current platform and configuration
    ApplyConfigOverwrite(ctx, kw)

    ApplyPlatformSpecificSettings(ctx, kw, target)
    ApplyBuildOptionSettings(ctx, kw)

    # Load all file lists (including addional settings)
    file_list = kw['file_list']
    for setting in kw['additional_settings']:
        file_list += setting.get('file_list', [])
        file_list += ctx.GetPlatformSpecificSettings(setting, 'file_list', ctx.env['PLATFORM'], GetConfiguration(ctx, kw['target']) )
    # Load all configuration specific files when generating projects
    if ctx.env['PLATFORM'] == 'project_generator':
        for configuration in CONFIGURATIONS:
            file_list += ctx.GetPlatformSpecificSettings(kw, 'file_list', ctx.env['PLATFORM'], configuration)
        for alias in CONFIGURATION_SHORTCUT_ALIASES.keys():
            file_list += ctx.GetPlatformSpecificSettings(kw, 'file_list', ctx.env['PLATFORM'], alias)

    LoadFileLists(ctx, kw, file_list)
    LoadAddionalFileSettings(ctx, kw)

    # Make sure we have a 'use' list
    if not kw.get('use', None):
        kw['use'] = []

    if ctx.is_windows_platform(ctx.env['PLATFORM']):
        # Handle meta includes for WinRT
        for meta_include in kw.get('meta_includes', []):
            append_kw_entry(kw,'cxxflags',[ '/AI' + meta_include ])

    # Handle export definitions file
    append_kw_entry(kw,'linkflags',[ '/DEF:' + ctx.path.make_node( export_file ).abspath() for export_file in kw['export_definitions']])

    # Handle Spec unique defines (if one is provided)
    if ctx.is_project_spec_specified():
        append_kw_entry(kw, 'defines', ctx.get_current_spec_defines())

    # Generate output file name (without file ending), use target as an default if nothing is specified
    if kw['output_file_name'] == []:
        kw['output_file_name'] = kw['target']
    elif isinstance(kw['output_file_name'],list):
        kw['output_file_name'] = kw['output_file_name'][0] # Change list into a single string

    # Handle force_disable_mfc (Small Hack for Perforce Plugin (no MFC, needs to be better defined))
    if kw.get('force_disable_mfc', False) and '_AFXDLL' in kw['defines']:
        kw['defines'].remove('_AFXDLL')

    # Clean out some duplicate kw values to reduce the size for the hash calculation
    kw['defines'] = clean_duplicates_in_list(kw['defines'],'{} : defines'.format(target))

def MonolithicBuildModule(ctx, *k, **kw):
    """
    Util function to collect all libs and linker settings for monolithic builds
    (Which apply all of those only to the final link as no DLLs or libs are produced)
    """
    # Set up member for monolithic build settings
    if not hasattr(ctx, 'monolithic_build_settings'):
        ctx.monolithic_build_settings = defaultdict(lambda: [])

    # For game specific modules, store with a game unique prefix
    prefix = ''
    if kw.get('game_project', False):
        prefix = kw['game_project'] + '_'

    # Collect libs for later linking
    def _append(key, values):
        if not ctx.monolithic_build_settings.get(key):
            ctx.monolithic_build_settings[key] = []
        ctx.monolithic_build_settings[key] += values

    # If this is a cryengine module, then it is marked to be included in all monolithic applications implicitly
    is_cryengine_module = kw.get('is_cryengine_module', False)
    if is_cryengine_module:
        _append(prefix + 'use',         [ kw['target'] ] )
        _append(prefix + 'lib',           kw['lib'] )
        _append(prefix + 'libpath',       kw['libpath'] )
        _append(prefix + 'linkflags',     kw['linkflags'] )
        _append(prefix + 'framework',     kw['framework'] )


    # Adjust own task gen settings
    append_kw_entry(kw, 'defines',[ '_LIB', 'AZ_MONOLITHIC_BUILD' ])

    # Remove rc files from the sources for monolithic builds (only the rc of
    # the launcher will be used) and remove any duplicate files that may have
    # sneaked in as well (using the python idiom: list(set(...)) to do so
    kw['source'] = [file for file in list(set(kw['source'])) if not file.endswith('.rc')]

    return ctx.objects(*k, **kw)


def does_module_use_qt(ctx, kw):

    # First check the features
    if 'qt5' in kw.get('features',[]):
        return True

    # Next check the uselib
    uselibs = kw.get('uselib',[])
    for uselib in uselibs:
        if uselib.startswith('QT5'):
            return True

    return False


###############################################################################
def BuildTaskGenerator(ctx, kw):

    """
    Check if this task generator should be build at all in the current configuration
    """
    target = kw['target']
    current_platform = ctx.env['PLATFORM']
    current_configuration = ctx.env['CONFIGURATION']
    if ctx.cmd == 'configure':
        return False        # Dont build during configure

    if ctx.cmd == 'generate_uber_files':
        ctx(features='generate_uber_file', uber_file_list=kw['file_list_content'], target=target, pch=os.path.basename( kw.get('pch', '') ))
        return False        # Dont do the normal build when generating uber files

    if ctx.cmd == 'generate_module_def_files':
        ctx(features='generate_module_def_files',
            use_module_list=kw['use'],
            platform_list=kw.get('platforms', []),
            configuration_list=kw.get('configurations', []),
            target=target)
        return False

    if not ctx.env.QMAKE and does_module_use_qt(ctx, kw):
        Logs.warn('[WARNING] Skipping module {} because it requires the lumberyard engine to be setup to compile the engine and/or editor and tools'.format(target))
        return False

    if current_platform == 'project_generator':
        return True         # Always include all projects when generating project for IDEs

    # if we're restricting to a platform, only build it if appropriate:
    if 'platforms' in kw:
        platforms_allowed = ctx.preprocess_platform_list(kw['platforms'], True)   # this will be a list like [ 'android_armv7_gcc', 'android_armv7_clang' ]
        if len(platforms_allowed) > 0:
            if current_platform not in platforms_allowed:
                Logs.debug('lumberyard: disabled module %s because it is only for platforms %s, we are not currently building that platform.'
                           % (kw['target'], platforms_allowed) )
                return False
        else:
            return False

    # If we're restricting to a configuration, only build if appropriate
    if 'configurations' in kw:
        configurations_allowed = ctx.preprocess_configuration_list(target, current_platform, kw['configurations'], True)
        if len(configurations_allowed) > 0:
            if current_configuration not in configurations_allowed:
                Logs.debug('lumberyard: disabled module {} because it is only for configurations {} on platform'.format(target,','.join(configurations_allowed),current_platform))
                return False
        else:
            return False

    build_in_dedicated = kw.get('build_in_dedicated')
    if (build_in_dedicated == False) and (ctx.is_building_dedicated_server()):
        Logs.debug('lumberyard: module %s disabled because we are building dedicated server and build_in_dedicated is set to False' % target)
        return False

    if ctx.is_target_enabled(target):
        Logs.debug('lumberyard: module {} enabled for platform {}.'.format(target,current_platform))
        return True     # Skip project is it is not part of the currecnt spec

    Logs.debug('lumberyard: disabled module %s because it is not in the current list of spec modules' % target)
    return False



@feature('apply_additional_settings')
@before_method('extract_vcxproj_overrides')
def tg_apply_additional_settings(self):
    """
    Apply all settings found in the addional_settings parameter after all compile tasks are generated
    """
    if len(self.file_specifc_settings) == 0:
        return # no file specifc settings found

    for t in getattr(self, 'compiled_tasks', []):
        input_file = t.inputs[0].abspath()

        file_specific_settings = self.file_specifc_settings.get(input_file, None)

        if not file_specific_settings:
            continue

        t.env['CFLAGS']     += file_specific_settings.get('cflags', [])
        t.env['CXXFLAGS'] += file_specific_settings.get('cxxflags', [])
        t.env['DEFINES']    += file_specific_settings.get('defines', [])

        for inc in file_specific_settings.get('defines', []):
            if os.path.isabs(inc):
                t.env['INCPATHS'] += [ inc ]
            else:
                t.env['INCPATHS'] += [ self.path.make_node(inc).abspath() ]

###############################################################################
def set_cryengine_flags(ctx, kw):

    prepend_kw_entry(kw,'includes',['.',
                                    ctx.CreateRootRelativePath('Code/SDKs/boost'),
                                    ctx.CreateRootRelativePath('Code/CryEngine/CryCommon')])


###############################################################################
def find_file_in_content_dict(content_dict, file_name):
    """
    Check if a file exists in the content dictionary
    """
    file_name_search_key = file_name.upper()
    for uber_file_name in iter(content_dict):
        vs_filter_dict = content_dict[uber_file_name]
        for vs_filter_name in iter(vs_filter_dict):
            source_files = vs_filter_dict[vs_filter_name]
            for source_file in source_files:
                if source_file.upper() == file_name_search_key:
                    return True
                # Handle the (posix) case if file_name is in a different folder than the context root
                if source_file.upper().endswith('/'+file_name_search_key):
                    return True
                # Handle the (dos) case if file_name is in a different folder than the context root
                if source_file.upper().endswith('\\'+file_name_search_key):
                    return True



    return False

###############################################################################
@conf
def CryEngineModule(ctx, *k, **kw):
    """
    CryEngine Modules are mostly compiled as dynamic libraries
    Except the build configuration requires a monolithic build
    """

    # Initialize the Task Generator
    InitializeTaskGenerator(ctx, kw)

    # Append common modules
    AppendCommonModules(ctx,kw)

    # Setup TaskGenerator specific settings
    set_cryengine_flags(ctx, kw)
    apply_cryengine_module_defines(ctx, kw)
    SetupRunTimeLibraries(ctx, kw)

    if hasattr(ctx, 'game_project'):

        if ctx.env['PLATFORM'] in ('android_armv7_gcc', 'android_armv7_clang') and ctx.game_project is not None:
            if ctx.get_android_settings(ctx.game_project) == None:
                Logs.warn('[WARN] Game project - %s - not configured for Android.  Skipping...' % ctx.game_project)
                return

        kw['game_project'] = ctx.game_project
        if kw.get('use_gems', False):
            # if this is defined it means we need to add all the defines, includes and such that the gem provides
            # to this project.
            ctx.apply_gems_to_context(ctx.game_project, k, kw)

    ConfigureTaskGenerator(ctx, kw)

    if not BuildTaskGenerator(ctx, kw):
        return

    if is_monolithic_build(ctx): # For monolithic builds, simply collect all build settings
        kw['is_cryengine_module'] = True
        MonolithicBuildModule(ctx, getattr(ctx, 'game_project', None), *k, **kw)
        return

    # Determine if we need to generate an rc file (for versioning) based on this being a windows platform and
    # there exists a resource.h file in the file list content.
    if ctx.env['PLATFORM'].startswith('win') and 'file_list_content' in kw:
        has_resource_h = find_file_in_content_dict(kw['file_list_content'],'resource.h')
        if has_resource_h:
            append_kw_entry(kw,'features',['generate_rc_file'])     # Always Generate RC files for Engine DLLs

    if ctx.env['PLATFORM'] == 'darwin_x64' or ctx.env['PLATFORM'] == 'ios' or ctx.env['PLATFORM'] == 'appletv':
        kw['mac_bundle']        = True                                      # Always create a Mac Bundle on darwin

    ctx.shlib(*k, **kw)


###############################################################################
@conf
def CryEngineSharedLibrary(ctx, *k, **kw):
    """
    Definition for shared libraries.  This is not considered a module, so it will not be implicitly included
    in project dependencies.
    """
    # Initialize the Task Generator
    InitializeTaskGenerator(ctx, kw)

    # Append common modules
    AppendCommonModules(ctx,kw)

    # Setup TaskGenerator specific settings
    set_cryengine_flags(ctx, kw)
    apply_cryengine_module_defines(ctx, kw)
    SetupRunTimeLibraries(ctx,kw)

    ConfigureTaskGenerator(ctx, kw)

    if not BuildTaskGenerator(ctx, kw):
        return

    if is_monolithic_build(ctx): # For monolithic builds, simply collect all build settings
        kw['is_cryengine_module'] = False
        MonolithicBuildModule(ctx, getattr(ctx, 'game_project', None), *k, **kw)
        return

    ctx.shlib(*k, **kw)

###############################################################################
@conf
def CryEngineStaticLibrary(ctx, *k, **kw):
    """
    CryEngine Static Libraries are static libraries
    Except the build configuration requires a monolithic build
    """
    # Initialize the Task Generator
    InitializeTaskGenerator(ctx, kw)
    set_cryengine_flags(ctx, kw)
    apply_cryengine_module_defines(ctx, kw)

    SetupRunTimeLibraries(ctx, kw)
    ConfigureTaskGenerator(ctx, kw)

    kw['stlib'] = True

    if not BuildTaskGenerator(ctx, kw):
        return

    if ctx.cmd == 'generate_uber_files':
        ctx(features='generate_uber_file', uber_file_list=kw['file_list_content'], target=kw['target'], pch=os.path.basename( kw.get('pch', '') ))
        return

    append_kw_entry(kw,'features',['c', 'cxx', 'cstlib', 'cxxstlib', 'use'])

    ctx.stlib(*k, **kw)

###############################################################################
@conf
def CryLauncher(ctx, *k, **kw):

    """
    Wrapper for CryEngine Executables
    """
    # Copy kw dict and some internal values to prevent overwriting settings in one launcher from another
    apply_cryengine_module_defines(ctx, kw)

    if ctx.env['PLATFORM'] != 'project_generator':  # if we're making project files for an IDE, then don't quit early
        if ctx.is_building_dedicated_server():
            return # regular launchers do not build in dedicated server mode.

    active_projects = ctx.get_enabled_game_project_list()
    for project in active_projects:
        kw_per_launcher = dict(kw)
        kw_per_launcher['target'] = project + kw['target'] # rename the target!

        # If there are multiple active projects, the 'use' kw needs to be its own instance
        if 'use' in kw:
            kw_per_launcher['use'] = []
            kw_per_launcher['use'] += kw['use']

        CryLauncher_Impl(ctx, project, *k, **kw_per_launcher)

def codegen_static_modules_cpp_for_launcher(ctx, project, k, kw):
    """
    Use codegen to create StaticModules.cpp and compile it into the launcher.
    StaticModules.cpp contains code that bootstraps each module used in a monolithic build.
    """

    if not is_monolithic_build(ctx):
        return
        
    # Gather modules used by this project
    static_modules = ctx.project_flavor_modules(project, 'Game')
    for gem in ctx.get_game_gems(project):
        if gem.link_type != 'NoCode' and not gem.is_legacy_igem:
            static_modules.append('{}_{}'.format(gem.name, gem.id.hex))

    # Write out json file listing modules. This will be fed into codegen.
    static_modules_json = {'modules' : static_modules}
    static_modules_json_node = ctx.path.find_or_declare(kw['target'] + 'StaticModules.json')
    static_modules_json_node.write(json.dumps(static_modules_json))
    
    # LMBR-30070: We should be generating this file with a waf task. Until then,
    # we need to manually set the cached signature.
    static_modules_json_node.sig = static_modules_json_node.cache_sig = Utils.h_file(static_modules_json_node.abspath())

    # Set up codegen for launcher.
    kw['features'] += ['az_code_gen']
    kw['az_code_gen'] = [
        {
            'files' : [static_modules_json_node],
            'scripts' : ['../CodeGen/StaticModules.py'],
            'arguments' : ['-JSON'],
        }
    ]

def CryLauncher_Impl(ctx, project, *k, **kw_per_launcher):

    # Initialize the Task Generator
    InitializeTaskGenerator(ctx, kw_per_launcher)

    # Append common modules
    AppendCommonModules(ctx, kw_per_launcher)

    # Setup TaskGenerator specific settings
    set_cryengine_flags(ctx, kw_per_launcher)
    SetupRunTimeLibraries(ctx, kw_per_launcher)

    ConfigureTaskGenerator(ctx, kw_per_launcher)

    if not BuildTaskGenerator(ctx, kw_per_launcher):
        return

    if ctx.env['PLATFORM'] in ('android_armv7_gcc', 'android_armv7_clang'):
        if ctx.get_android_settings(project) == None:
            Logs.warn('[WARN] Game project - %s - not configured for Android.  Skipping...' % ctx.game_project)
            return

    kw_per_launcher['idx']              = kw_per_launcher['idx'] + (1000 * (ctx.project_idx(project) + 1));
    # Setup values for Launcher Projects
    append_kw_entry(kw_per_launcher,'features',[ 'generate_rc_file' ])
    kw_per_launcher['is_launcher']          = True
    kw_per_launcher['resource_path']        = ctx.launch_node().make_node(ctx.game_code_folder(project) + '/Resources')
    kw_per_launcher['project_name']         = project
    kw_per_launcher['output_file_name']     = ctx.get_executable_name( project )

    Logs.debug("lumberyard: Generating launcher %s from %s" % (kw_per_launcher['output_file_name'], kw_per_launcher['target']))

    # For some odd reason applying the gems to the Android launcher causes a build order issue where
    # the launcher is built/linked prior to the required gems being built/linked resulting in a missing
    # node signature error.  I suspect it has to do with being compiled into a library instead of a program.
    if ctx.env['PLATFORM'] not in ('android_armv7_gcc', 'android_armv7_clang'):
        ctx.apply_gems_to_context(project, k, kw_per_launcher)

    codegen_static_modules_cpp_for_launcher(ctx, project, k, kw_per_launcher)

    if 'mac_launcher' in kw_per_launcher:
        kw_per_launcher['output_sub_folder'] = ctx.get_executable_name( project ) + ".app/Contents/MacOS"
    elif 'appletv_launcher' in kw_per_launcher:
        kw_per_launcher['output_sub_folder'] = ctx.get_executable_name( project ) + ".app"
    elif 'ios_launcher' in kw_per_launcher:
        kw_per_launcher['output_sub_folder'] = ctx.get_executable_name( project ) + ".app"

    if is_monolithic_build(ctx):
        append_kw_entry(kw_per_launcher,'defines',[ '_LIB', 'AZ_MONOLITHIC_BUILD' ])
        append_kw_entry(kw_per_launcher,'features',[ 'apply_monolithic_build_settings' ])

    if not is_monolithic_build(ctx) and 'mac_launcher' in kw_per_launcher:
    	append_kw_entry(kw_per_launcher, 'features', ['apply_non_monolithic_launcher_settings'])

    # android doesn't have the concept of native executables so we need to build it as a lib
    if ctx.env['PLATFORM'] in ('android_armv7_gcc', 'android_armv7_clang'):
        ctx.shlib(*k, **kw_per_launcher)
    else:
        ctx.program(*k, **kw_per_launcher)

###############################################################################
@conf
def CryDedicatedServer(ctx, *k, **kw):
    """
    Wrapper for CryEngine Dedicated Servers
    """

    apply_cryengine_module_defines(ctx, kw)

    active_projects = ctx.get_enabled_game_project_list()

    for project in active_projects:
        kw_per_launcher = dict(kw)
        kw_per_launcher['target'] = project + kw['target'] # rename the target!

        # If there are multiple active projects, the 'use' kw needs to be its own instance
        if 'use' in kw:
            kw_per_launcher['use'] = []
            kw_per_launcher['use'] += kw['use']

        CryDedicatedserver_Impl(ctx, project, *k, **kw_per_launcher)

def CryDedicatedserver_Impl(ctx, project, *k, **kw_per_launcher):

    if ctx.env['PLATFORM'] != 'project_generator': # if we're making project files for an IDE, then don't quit early
        if not ctx.is_building_dedicated_server():
            return # only build this launcher in dedicated mode.

    # Initialize the Task Generator
    InitializeTaskGenerator(ctx, kw_per_launcher)

    # Append common modules
    AppendCommonModules(ctx,kw_per_launcher)

    # Setup TaskGenerator specific settings
    set_cryengine_flags(ctx, kw_per_launcher)
    SetupRunTimeLibraries(ctx,kw_per_launcher)

    append_kw_entry(kw_per_launcher,'win_linkflags',[ '/SUBSYSTEM:WINDOWS' ])

    ConfigureTaskGenerator(ctx, kw_per_launcher)

    if not BuildTaskGenerator(ctx, kw_per_launcher):
        return

    kw_per_launcher['idx']          = kw_per_launcher['idx'] + (1000 * (ctx.project_idx(project) + 1));

    append_kw_entry(kw_per_launcher,'features',[ 'generate_rc_file' ])
    kw_per_launcher['is_dedicated_server']          = True
    kw_per_launcher['resource_path']                = ctx.launch_node().make_node(ctx.game_code_folder(project) + '/Resources')
    kw_per_launcher['project_name']                 = project
    kw_per_launcher['output_file_name']             = ctx.get_dedicated_server_executable_name(project)

    Logs.debug("lumberyard: Generating dedicated server %s from %s" % (kw_per_launcher['output_file_name'], kw_per_launcher['target']))

    ctx.apply_gems_to_context(project, k, kw_per_launcher)

    codegen_static_modules_cpp_for_launcher(ctx, project, k, kw_per_launcher)

    if is_monolithic_build(ctx):
        Logs.debug("lumberyard: Dedicated server monolithic build %s ... " % kw_per_launcher['target'])
        append_kw_entry(kw_per_launcher,'defines',[ '_LIB', 'AZ_MONOLITHIC_BUILD' ])
        append_kw_entry(kw_per_launcher,'features',[ 'apply_monolithic_build_settings' ])


    ctx.program(*k, **kw_per_launcher)


###############################################################################
@conf
def CryConsoleApplication(ctx, *k, **kw):
    """
    Wrapper for CryEngine Executables
    """
    # Initialize the Task Generator
    InitializeTaskGenerator(ctx, kw)

    # Append common modules
    AppendCommonModules(ctx,kw)

    # Setup TaskGenerator specific settings
    set_cryengine_flags(ctx, kw)
    apply_cryengine_module_defines(ctx, kw)
    SetupRunTimeLibraries(ctx, kw)

    if ctx.is_windows_platform(ctx.env['PLATFORM']):
        append_kw_entry(kw,'win_linkflags',[ '/SUBSYSTEM:CONSOLE' ])
        
    # Default clang behavior is to disable exceptions. For console apps we want to enable them
    if 'CXXFLAGS' in ctx.env.keys() and 'darwin' in ctx.get_platform_list(ctx.env['PLATFORM']):
        if '-fno-exceptions' in ctx.env['CXXFLAGS']:
            ctx.env['CXXFLAGS'].remove("-fno-exceptions")

    ConfigureTaskGenerator(ctx, kw)

    if not BuildTaskGenerator(ctx, kw):
        return

    ctx.program(*k, **kw)

###############################################################################
@conf
def CryBuildUtility(ctx, *k, **kw):
    """
    Wrapper for Build Utilities
    """

    # Initialize the Task Generator
    InitializeTaskGenerator(ctx, kw)

    # Setup TaskGenerator specific settings
    SetupRunTimeLibraries(ctx,kw)

    if ctx.is_windows_platform(ctx.env['PLATFORM']):
        append_kw_entry(kw,'win_linkflags',[ '/SUBSYSTEM:CONSOLE' ])

    ConfigureTaskGenerator(ctx, kw)

    if not BuildTaskGenerator(ctx, kw):
        return

    ctx.program(*k, **kw)

###############################################################################
@conf
def CryFileContainer(ctx, *k, **kw):
    """
    Function to create a header only library
    """
    # Initialize the Task Generator
    InitializeTaskGenerator(ctx, kw)

    # Setup TaskGenerator specific settings
    ConfigureTaskGenerator(ctx, kw)

    if not BuildTaskGenerator(ctx, kw):
        return

    if ctx.env['PLATFORM'] == 'project_generator':
        ctx(*k, **kw)


###############################################################################
@conf
def CryEditor(ctx, *k, **kw):
    """
    Wrapper for CryEngine Editor Executables
    """
    # Initialize the Task Generator
    InitializeTaskGenerator(ctx, kw)

    # Append common modules
    AppendCommonModules(ctx,kw)

    # Setup TaskGenerator specific settings
    ctx.set_editor_flags(kw)
    apply_cryengine_module_defines(ctx, kw)
    SetupRunTimeLibraries(ctx, kw)

    # Additional Editor-specific settings
    append_kw_entry(kw,'features',[ 'generate_rc_file' ])
    append_kw_entry(kw,'defines',[ 'SANDBOX_EXPORTS' ])

    if ctx.is_windows_platform(ctx.env['PLATFORM']):
        append_kw_entry(kw,'win_linkflags',[ '/SUBSYSTEM:WINDOWS' ])

    ConfigureTaskGenerator(ctx, kw)

    if not BuildTaskGenerator(ctx, kw):
        return

    ctx.program(*k, **kw)

###############################################################################
@conf
def LumberyardApp(ctx, *k, **kw):
    """
    Wrapper to make lmbr_waf happy.  We shouldn't tack on any settings here,
    so we can make the waf transition easier later on.
    """

    InitializeTaskGenerator(ctx, kw)

    apply_cryengine_module_defines(ctx, kw)

    SetupRunTimeLibraries(ctx,kw)

    ConfigureTaskGenerator(ctx, kw)

    if BuildTaskGenerator(ctx, kw):
        ctx.program(*k, **kw)

###############################################################################
@conf
def CryEditorCore(ctx, *k, **kw):
    """
    Wrapper for CryEngine Editor Core component
    """
    # Initialize the Task Generator
    InitializeTaskGenerator(ctx, kw)

    # Append common modules
    AppendCommonModules(ctx,kw)

    # Setup TaskGenerator specific settings
    ctx.set_editor_flags(kw)
    apply_cryengine_module_defines(ctx, kw)

    SetupRunTimeLibraries(ctx,kw)
    append_kw_entry(kw,'cxxflags',['/EHsc'])
    append_kw_entry(kw,'cflags', ['/EHsc'])
    append_kw_entry(kw,'defines',['EDITOR_CORE', 'USE_MEM_ALLOCATOR', 'EDITOR', 'DONT_BAN_STD_STRING', 'FBXSDK_NEW_API=1' ])

    ConfigureTaskGenerator(ctx, kw)

    if not BuildTaskGenerator(ctx, kw):
        return

    ctx.shlib(*k, **kw)


###############################################################################
@conf
def CryEditorUiQt(ctx, *k, **kw):
    """
    Wrapper for CryEngine Editor Core component
    """
    # Initialize the Task Generator
    InitializeTaskGenerator(ctx, kw)

    # Append common modules
    AppendCommonModules(ctx,kw)

    # Setup TaskGenerator specific settings
    ctx.set_editor_flags(kw)
    apply_cryengine_module_defines(ctx, kw)

    SetupRunTimeLibraries(ctx,kw)
    append_kw_entry(kw,'cxxflags',['/EHsc'])
    append_kw_entry(kw,'cflags',['/EHsc'])
    append_kw_entry(kw,'defines',[  'NOMINMAX',
                                    'EDITOR_UI_UX_CHANGE',
                                    'EDITOR_QT_UI_EXPORTS',
                                    'IGNORE_CRY_COMMON_STATIC_VAR',
                                    'CRY_ENABLE_RC_HELPER',
                                    'PLUGIN_EXPORTS',
                                    'EDITOR_COMMON_IMPORTS',
                                    'NOT_USE_CRY_MEMORY_MANAGER'])
    
    ConfigureTaskGenerator(ctx, kw)

    if not BuildTaskGenerator(ctx, kw):
        return

    ctx.shlib(*k, **kw)


###############################################################################
@conf
def CryPlugin(ctx, *k, **kw):
    """
    Wrapper for CryEngine Editor Plugins
    """
    # Initialize the Task Generator
    InitializeTaskGenerator(ctx, kw)

    # Append common modules
    AppendCommonModules(ctx,kw)

    # Setup TaskGenerator specific settings
    ctx.set_editor_flags(kw)
    apply_cryengine_module_defines(ctx, kw)

    SetupRunTimeLibraries(ctx,kw)
    append_kw_entry(kw,'cxxflags',['/EHsc'])
    append_kw_entry(kw,'cflags',['/EHsc'])
    append_kw_entry(kw,'defines',[ 'SANDBOX_IMPORTS', 'PLUGIN_EXPORTS', 'EDITOR_COMMON_IMPORTS', 'NOT_USE_CRY_MEMORY_MANAGER' ])
    kw['output_sub_folder']     = 'EditorPlugins'
    kw['features'] += ['qt5']#added QT to all plugins

    ConfigureTaskGenerator(ctx, kw)

    if not BuildTaskGenerator(ctx, kw):
        return

    ctx.shlib(*k, **kw)
    
    ###############################################################################
@conf
def BuilderPlugin(ctx, *k, **kw):
    """
    Wrapper for CryEngine Editor Plugins
    """
    append_kw_entry(kw, 'output_sub_folder', ['Builders'])
    append_kw_entry(kw, 'cxxflags', ['/EHsc'])
    append_kw_entry(kw, 'cflags', ['/EHsc'])
    append_kw_entry(kw, 'defines', ['UNICODE'])
    append_kw_entry(kw, 'debug_defines', ['AZ_DEBUG_BUILD'])
    append_kw_entry(kw, 'use', ['AzToolsFramework', 'AssetBuilderSDK'])
    
    # Initialize the Task Generator
    InitializeTaskGenerator(ctx, kw)

    # Append common modules
    AppendCommonModules(ctx,kw)

    # Setup TaskGenerator specific settings
    ctx.set_editor_flags(kw)
    apply_cryengine_module_defines(ctx, kw)

    SetupRunTimeLibraries(ctx,kw)

    ConfigureTaskGenerator(ctx, kw)

    if not BuildTaskGenerator(ctx, kw):
        return

    ctx.shlib(*k, **kw)

###############################################################################
@conf
def CryStandAlonePlugin(ctx, *k, **kw):
    """
    Wrapper for CryEngine Editor Plugins
    """
    # Initialize the Task Generator
    InitializeTaskGenerator(ctx, kw)

    # Append common modules
    AppendCommonModules(ctx,kw)
    apply_cryengine_module_defines(ctx, kw)

    # Setup TaskGenerator specific settings
    ctx.set_editor_flags(kw)
    SetupRunTimeLibraries(ctx,kw)
    append_kw_entry(kw,'cxxflags',['/EHsc'])
    append_kw_entry(kw,'cflags',['/EHsc'])
    append_kw_entry(kw,'defines',[ 'PLUGIN_EXPORTS', 'NOT_USE_CRY_MEMORY_MANAGER' ])
    append_kw_entry(kw,'debug_linkflags',['/NODEFAULTLIB:libcmtd.lib', '/NODEFAULTLIB:libcd.lib'])
    append_kw_entry(kw,'profile_linkflags',['/NODEFAULTLIB:libcmt.lib', '/NODEFAULTLIB:libc.lib'])
    append_kw_entry(kw,'release_linkflags',['/NODEFAULTLIB:libcmt.lib', '/NODEFAULTLIB:libc.lib'])

    if not 'output_sub_folder' in kw:
        kw['output_sub_folder'] = 'EditorPlugins'
    kw['features'] += ['qt5'] #added QT to all plugins

    ConfigureTaskGenerator(ctx, kw)

    if not BuildTaskGenerator(ctx, kw):
        return

    kw['enable_rtti'] = [ True ]
    kw['remove_release_define'] = [ True ]

    ctx.shlib(*k, **kw)

###############################################################################
@conf
def CryPluginModule(ctx, *k, **kw):
    """
    Wrapper for CryEngine Editor Plugins Util dlls, those used by multiple plugins
    """
    # Initialize the Task Generator
    InitializeTaskGenerator(ctx, kw)

    # Append common modules
    AppendCommonModules(ctx,kw)

    apply_cryengine_module_defines(ctx, kw)

    # Setup TaskGenerator specific settings
    ctx.set_editor_flags(kw)
    SetupRunTimeLibraries(ctx,kw)
    append_kw_entry(kw,'cxxflags',['/EHsc'])
    append_kw_entry(kw,'cflags',['/EHsc'])
    append_kw_entry(kw,'defines',[ 'PLUGIN_EXPORTS', 'EDITOR_COMMON_EXPORTS', 'NOT_USE_CRY_MEMORY_MANAGER' ])
    if not 'output_sub_folder' in kw:
        kw['output_sub_folder'] = 'EditorPlugins'

    kw['features'] += ['qt5']#added QT to all plugins
    ConfigureTaskGenerator(ctx, kw)

    if not BuildTaskGenerator(ctx, kw):
        return

    kw['remove_release_define'] = [ True ]

    ctx.shlib(*k, **kw)

###############################################################################
@conf
def CryEditorCommon(ctx, *k, **kw):
    """
    Wrapper for CryEditorCommon
    """
    # Initialize the Task Generator
    InitializeTaskGenerator(ctx, kw)

    # Append common modules
    AppendCommonModules(ctx,kw)
    apply_cryengine_module_defines(ctx, kw)

    # Setup TaskGenerator specific settings
    ctx.set_editor_flags(kw)
    SetupRunTimeLibraries(ctx,kw)
    append_kw_entry(kw,'cxxflags',['/EHsc'])
    append_kw_entry(kw,'cflags',['/EHsc'])
    append_kw_entry(kw,'defines',[ 'PLUGIN_EXPORTS', 'EDITOR_COMMON_EXPORTS', 'NOT_USE_CRY_MEMORY_MANAGER' ])

    ConfigureTaskGenerator(ctx, kw)

    if not BuildTaskGenerator(ctx, kw):
        return

    kw['remove_release_define'] = [ True ]

    ctx.shlib(*k, **kw)


# Dependent files/folders from Bin64 to copy to any other target folder
INCLUDE_MIRROR_RC_FILES = [
    'rc/Qt5Core*.dll',
    'rc/Qt5Gui*.dll',
    'rc/Qt5Svg*.dll',
    'rc/libGLESv2*.dll',
    'rc/icu*.dll',
    'rc/rc.ini'
]

###############################################################################
@conf
def CryResourceCompiler(ctx, *k, **kw):
    """
    Wrapper for RC
    """
    # Initialize the Task Generator
    InitializeTaskGenerator(ctx, kw)

    # Append common modules
    AppendCommonModules(ctx,kw)
    apply_cryengine_module_defines(ctx, kw)

    # Setup TaskGenerator specific settings
    SetupRunTimeLibraries(ctx,kw, 'dynamic')

    ctx.set_rc_flags(kw)

    # Setup mirroring of files
    kw['mirror_artifacts_to_include'] = INCLUDE_MIRROR_RC_FILES
    append_kw_entry(kw,'features',[ 'mirror_artifacts' ])

    kw['output_file_name']  = 'rc'
    kw['output_sub_folder'] = 'rc'

    Logs.debug('lumberyard: creating RC, with mirror_artifacts')

    if ctx.is_windows_platform(ctx.env['PLATFORM']):
        append_kw_entry(kw,'win_debug_linkflags',['/NODEFAULTLIB:libcmtd.lib', '/NODEFAULTLIB:libcd.lib'])
        append_kw_entry(kw,'win_profile_linkflags',['/NODEFAULTLIB:libcmt.lib', '/NODEFAULTLIB:libc.lib'])
        append_kw_entry(kw,'win_release_linkflags',['/NODEFAULTLIB:libcmt.lib', '/NODEFAULTLIB:libc.lib'])
        append_kw_entry(kw,'win_linkflags',[ '/SUBSYSTEM:CONSOLE' ])
        
    ConfigureTaskGenerator(ctx, kw)

    if not BuildTaskGenerator(ctx, kw):
        return

    ctx.program(*k, **kw)

###############################################################################
@conf
def CryResourceCompilerModule(ctx, *k, **kw):
    """
    Wrapper for RC modules
    """
    # Initialize the Task Generator
    InitializeTaskGenerator(ctx, kw)

    # Append common modules
    AppendCommonModules(ctx,kw)
    apply_cryengine_module_defines(ctx, kw)

    # Setup TaskGenerator specific settings
    SetupRunTimeLibraries(ctx,kw, 'dynamic')

    ctx.set_rc_flags(kw)
    kw['output_sub_folder'] = 'rc'

    if ctx.is_windows_platform(ctx.env['PLATFORM']):
        append_kw_entry(kw,'win_debug_linkflags',['/NODEFAULTLIB:libcmtd.lib', '/NODEFAULTLIB:libcd.lib'])
        append_kw_entry(kw,'win_profile_linkflags',['/NODEFAULTLIB:libcmt.lib', '/NODEFAULTLIB:libc.lib'])
        append_kw_entry(kw,'win_release_linkflags',['/NODEFAULTLIB:libcmt.lib', '/NODEFAULTLIB:libc.lib'])

    ConfigureTaskGenerator(ctx, kw)

    if not BuildTaskGenerator(ctx, kw):
        return
    if ctx.is_windows_platform(ctx.env['PLATFORM']):
        append_kw_entry(kw,'win_linkflags',[ '/SUBSYSTEM:CONSOLE' ])

    ctx.shlib(*k, **kw)

###############################################################################
@conf
def CryPipelineModule(ctx, *k, **kw):
    """
    Wrapper for Pipleine modules (mostly DCC exporters)
    """
    # Initialize the Task Generator
    InitializeTaskGenerator(ctx, kw)

    # Setup TaskGenerator specific settings
    SetupRunTimeLibraries(ctx, kw, 'dynamic')

    apply_cryengine_module_defines(ctx, kw)

    ctx.set_pipeline_flags(kw)

    # LUMBERYARD
    if ctx.is_windows_platform(ctx.env['PLATFORM']):
        append_kw_entry(kw,'win_linkflags',[ '/SUBSYSTEM:CONSOLE' ])
        append_kw_entry(kw,'win_debug_linkflags',['/NODEFAULTLIB:libcmtd.lib', '/NODEFAULTLIB:libcd.lib'])
        append_kw_entry(kw,'win_profile_linkflags',['/NODEFAULTLIB:libcmt.lib', '/NODEFAULTLIB:libc.lib'])
        append_kw_entry(kw,'win_release_linkflags',['/NODEFAULTLIB:libcmt.lib', '/NODEFAULTLIB:libc.lib'])

    ConfigureTaskGenerator(ctx, kw)

    if not BuildTaskGenerator(ctx, kw):
        return

    ctx.shlib(*k, **kw)

###############################################################################
@conf
def CryQtApplication(ctx, *k, **kw):
    """
    Wrapper for Qt programs launched by the editor
    """
    # Initialize the Task Generator
    InitializeTaskGenerator(ctx, kw)

    # Append common modules
    AppendCommonModules(ctx,kw)
    apply_cryengine_module_defines(ctx, kw)

    # Setup TaskGenerator specific settings
    set_cryengine_flags(ctx, kw)
    SetupRunTimeLibraries(ctx,kw)

    if ctx.is_windows_platform(ctx.env['PLATFORM']):
        append_kw_entry(kw,'win_linkflags',[ '/SUBSYSTEM:WINDOWS' ])

    ConfigureTaskGenerator(ctx, kw)

    if not BuildTaskGenerator(ctx, kw):
        return

    ctx.program(*k, **kw)

###############################################################################
@conf
def CryQtConsoleApplication(ctx, *k, **kw):
    """
    Wrapper for Qt programs launched by the editor
    """
    # Initialize the Task Generator
    InitializeTaskGenerator(ctx, kw)

    # Append common modules
    AppendCommonModules(ctx,kw)
    apply_cryengine_module_defines(ctx, kw)

    # Setup TaskGenerator specific settings
    set_cryengine_flags(ctx, kw)
    SetupRunTimeLibraries(ctx,kw)

    if ctx.is_windows_platform(ctx.env['PLATFORM']):
        append_kw_entry(kw,'win_linkflags',[ '/SUBSYSTEM:CONSOLE' ])

    ConfigureTaskGenerator(ctx, kw)

    if not BuildTaskGenerator(ctx, kw):
        return

    ctx.program(*k, **kw)

###############################################################################
# Helper function to set Flags based on options
def ApplyBuildOptionSettings(self, kw):
    """
    Util function to apply flags based on waf options
    """
    # Add debug flags if requested
    if self.is_option_true('generate_debug_info'):
        kw['cflags'].extend(self.env['COMPILER_FLAGS_DebugSymbols'])
        kw['cxxflags'].extend(self.env['COMPILER_FLAGS_DebugSymbols'])
        kw['linkflags'].extend(self.env['LINKFLAGS_DebugSymbols'])

    # Add show include flags if requested
    if self.is_option_true('show_includes'):
        kw['cflags'].extend(self.env['SHOWINCLUDES_cflags'])
        kw['cxxflags'].extend(self.env['SHOWINCLUDES_cxxflags'])

    # Add preprocess to file flags if requested
    if self.is_option_true('show_preprocessed_file'):
        kw['cflags'].extend(self.env['PREPROCESS_cflags'])
        kw['cxxflags'].extend(self.env['PREPROCESS_cxxflags'])
        self.env['CC_TGT_F'] = self.env['PREPROCESS_cc_tgt_f']
        self.env['CXX_TGT_F'] = self.env['PREPROCESS_cxx_tgt_f']

    # Add disassemble to file flags if requested
    if self.is_option_true('show_disassembly'):
        kw['cflags'].extend(self.env['DISASSEMBLY_cflags'])
        kw['cxxflags'].extend(self.env['DISASSEMBLY_cxxflags'])
        self.env['CC_TGT_F'] = self.env['DISASSEMBLY_cc_tgt_f']
        self.env['CXX_TGT_F'] = self.env['DISASSEMBLY_cxx_tgt_f']

###############################################################################
# Helper function to extract platform specific flags
@conf
def GetPlatformSpecificSettings(ctx, dict, entry, _platform, configuration):
    """
    Util function to apply flags based on current platform
    """
    def _to_list( value ):
        if isinstance(value,list):
            return value
        return [ value ]

    returnValue = []
    platforms   = ctx.get_platform_list( _platform )

    # Check for entry in <platform>_<entry> style
    for platform in platforms:
        platform_entry = platform + '_' + entry
        if not platform_entry in dict:
            continue # No platfrom specific entry found

        returnValue += _to_list( dict[platform_entry] )

    if configuration == []:
        return [] # Dont try to check for configurations if we dont have any

    # Check for entry in <configuration>_<entry> style
    configuration_entry = configuration + '_' + entry
    if configuration_entry in dict:
        returnValue += _to_list( dict[configuration_entry] )

    # Check for entry in <platform>_<configuration>_<entry> style
    for platform in platforms:
        platform_configuration_entry =   platform + '_' + configuration + '_' + entry
        if not platform_configuration_entry in dict:
            continue # No platfrom /configuration specific entry found

        returnValue += _to_list( dict[platform_configuration_entry] )

    return returnValue

def process_kw_macros_expansion(target, kw, configuration):
    """
    Process the special kw expansion macros in the keyword dictionary based on the configuration
    Args:
        target:         Target name to report any warnings
        kw:             The keyword map to manipulate
        configuration:  The current build configuration
    """
    class KeywordMacroNDebug:
        """
        Keyword Macro handler to handle 'ndebug' macros.  These macros are a convenience macro to expand non-debug
        keywords to the appropriate non-debug configuration.  This is to reduce the need to repeat all of the non-debug
        configuration values in the keyword list
        """

        def process(self,keyword_name, keyword_value, current_configuration):

            is_config_dedicated = current_configuration.endswith('_dedicated')
            is_kw_dedicated = '_dedicated' in kw_entry
            if is_config_dedicated != is_kw_dedicated:
                return None, None

            # Only process this keyword non-debug mode, otherwise it will be ignored
            if current_configuration not in ('debug', 'debug_dedicated', 'debug_test', 'debug_test_dedicated'):
                if '_ndebug_dedicated_' in kw_entry and is_config_dedicated:
                    new_kw_entry_name = keyword_name.replace('_ndebug_dedicated_','_{}_'.format(configuration))
                    return (new_kw_entry_name,keyword_value), keyword_name
                elif '_ndebug_' in kw_entry:
                    new_kw_entry_name = keyword_name.replace('_ndebug_','_{}_'.format(configuration))
                    return (new_kw_entry_name,keyword_value), keyword_name
                elif kw_entry.startswith('ndebug_dedicated') and is_config_dedicated:
                    new_kw_entry_name = keyword_name.replace('ndebug_dedicated_','{}_'.format(configuration))
                    return (new_kw_entry_name,keyword_value), keyword_name
                elif kw_entry.startswith('ndebug'):
                    new_kw_entry_name = keyword_name.replace('ndebug_','{}_'.format(configuration))
                    return (new_kw_entry_name,keyword_value), keyword_name
            return None, None

    class KeywordMacroShortcutAlias:
        """
        Keyword macro handler to handle shortcut aliases. These aliases are defined in waf_branch_spec and are used to
        group multiple configurations under one using a dictionary. This will make it so that aliases can be used in
        keywords as they are already used as a value for the 'configuration' keyword. For example:

        debug_test_file_list = 'module_tests.waf_files',
        profile_test_file_list = 'module_tests.waf_files'

        becomes

        test_all_file_list = 'module_tests.waf_files'
        """
        def process(self, keyword_name, keyword_value, current_configuration):
            for alias, configs in CONFIGURATION_SHORTCUT_ALIASES.iteritems():
                if alias == 'all':
                    continue  # Do not use 'all' alias, it conflicts with other aliases
                if current_configuration not in configs:
                    continue
                if keyword_name.startswith(alias):
                    remove_entry_name = keyword_name
                    new_kw_entry_name = keyword_name.replace(alias, current_configuration)
                elif '_{}_'.format(alias) in keyword_name:
                    remove_entry_name = keyword_name
                    new_kw_entry_name = keyword_name.replace('_{}_'.format(alias), '_{}_'.format(current_configuration))
                else:
                    continue
                return (new_kw_entry_name, keyword_value), remove_entry_name
            return None, None

    class KeywordMacroAutoDebugLib:
        """
        Keyword macro handler to handle 'lib' and 'uselib' keywords to add a common 'D' character after the library name
        entry.  For example:

         autod_uselib = ['QTMAIN']

         will be in profile configuration:
            profile_uselib = ['QTMAIN']

        and will be in profile configuration:
            debug_uselib = ['QTMAIND']

        This is to eliminate the need to specify the debug and non-debug versions of the same library for every project setup
        """
        def process(self,keyword_name, keyword_value, current_configuration):

            if kw_entry.endswith('_autod_uselib'):
                remove_entry_name = keyword_name
                new_kw_entry_name = keyword_name.replace('_autod_uselib','_{}_uselib'.format(configuration))
            elif kw_entry == 'autod_uselib':
                remove_entry_name = keyword_name
                new_kw_entry_name = '{}_uselib'.format(configuration)
            elif kw_entry.endswith('_autod_lib'):
                remove_entry_name = keyword_name
                new_kw_entry_name = keyword_name.replace('_autod_lib','_{}_lib'.format(configuration))
            elif kw_entry == 'autod_lib':
                remove_entry_name = keyword_name
                new_kw_entry_name = '{}_lib'.format(configuration)
            else:
                return None, None

            if current_configuration in ('debug', 'debug_dedicated', 'debug_test', 'debug_test_dedicated'):
                if isinstance(keyword_value,str):
                    new_kw_value = keyword_value + 'D'
                elif isinstance(keyword_value,list):
                    debug_lib_list = []
                    for lib in keyword_value:
                        debug_lib = lib + 'D'
                        debug_lib_list.append(debug_lib)
                    new_kw_value = debug_lib_list
                else:
                    Logs.warn('[WARN] kw[{}] expected to be a string or list. '.format(keyword_name))
                    return None, None
                return (new_kw_entry_name,new_kw_value), remove_entry_name
            else:
                return (new_kw_entry_name,keyword_value), remove_entry_name


        # Perform special keyword macro replacement

    # Only valid and supported configurations
    if configuration is None or len(configuration)==0 or configuration not in CONFIGURATIONS:
        return

    macros = [KeywordMacroNDebug(), KeywordMacroAutoDebugLib(), KeywordMacroShortcutAlias()]

    if configuration != 'project_generator':
        kw_entries_to_add = []
        kw_entries_to_remove = []

        for kw_entry, kw_value in kw.iteritems():
            for macro in macros:
                kw_entry_to_add, kw_entry_to_remove = macro.process(kw_entry, kw_value,configuration)
                if kw_entry_to_add is not None or kw_entry_to_remove is not None:
                    if kw_entry_to_add is not None:
                        kw_entries_to_add.append(kw_entry_to_add)
                    if kw_entry_to_remove is not None:
                        kw_entries_to_remove.append(kw_entry_to_remove)

        if len(kw_entries_to_add)>0:
            for new_kw_key, new_kw_value in kw_entries_to_add:
                if new_kw_key in kw:
                    Logs.warn('[WARN] Macro Expanded keyword {} has a collision to an existing keyword for Target {}.  Skipping macro'.format(new_kw_key,target))
                else:
                    kw[new_kw_key] = new_kw_value
        if len(kw_entries_to_remove)>0:
            for kw_to_delete in kw_entries_to_remove:
                del kw[kw_to_delete]

###############################################################################
# Wrapper for ApplyPlatformSpecificFlags for all flags to apply
@conf
def ApplyPlatformSpecificSettings(ctx, kw, target):
    """
    Check each compiler/linker flag for platform specific additions
    """

    platform = ctx.env['PLATFORM']
    configuration = GetConfiguration( ctx, target )

    # Expand any special macros
    process_kw_macros_expansion(target, kw, configuration)

    # handle list entries
    for entry in get_common_inputs():
        append_kw_entry(kw,entry,GetPlatformSpecificSettings(ctx, kw, entry, platform, configuration))

    # Handle string entries
    for entry in 'output_file_name'.split():
        if not entry in kw or kw[entry] == []: # No general one set yet
            kw[entry] = GetPlatformSpecificSettings(ctx, kw, entry, platform, configuration)

    # Recurse for addional settings
    for setting in kw['additional_settings']:
        ApplyPlatformSpecificSettings(ctx, setting, target)


###############################################################################
# Set env in case a env overwrite is specified for this project
def ApplyConfigOverwrite(ctx, kw):

    target = kw['target']
    if not target in ctx.env['CONFIG_OVERWRITES']:
        return

    platform                    =  ctx.env['PLATFORM']
    overwrite_config    = ctx.env['CONFIG_OVERWRITES'][target]

    # Need to set crytek specific shortcuts if loading another enviorment
    ctx.all_envs[platform + '_' + overwrite_config]['PLATFORM']             = platform
    ctx.all_envs[platform + '_' + overwrite_config]['CONFIGURATION']  = overwrite_config

    # Create a deep copy of the env for overwritten task to prevent modifying other task generator envs
    kw['env'] = ctx.all_envs[platform + '_' + overwrite_config].derive()
    kw['env'] .detach()

###############################################################################
@conf
def is_monolithic_build(ctx):
    if ctx.env['PLATFORM'] == 'project_generator':
        return False

    return ctx.spec_monolithic_build()

###############################################################################
@conf
def get_current_spec_defines(ctx):
    if ctx.env['PLATFORM'] == 'project_generator' or ctx.env['PLATFORM'] == []:
        return []   # Return only an empty list when generating a project

    return ctx.spec_defines()


@feature('apply_non_monolithic_launcher_settings')
@before_method('process_source')
def apply_non_monolithic_launcher_settings(self):
    self.env['LINKFLAGS_MACBUNDLE'] =  []  # disable the '-dynamiclib' flag


@feature('apply_monolithic_build_settings')
@before_method('process_source')
def apply_monolithic_build_settings(self):
    # Add collected settings to link task
    # Don't do 'list(set(...))' on these values as duplicates will be removed
    # by waf later and some arguments need to be next to each other (such as
    # -force_load <lib>). The set will rearrange the order in a
    # non-deterministic way

    def _apply_monolithic_build_settings(monolithic_dict, prefix=''):
        append_to_unique_list(self.use, list(monolithic_dict[prefix + 'use']))
        self.lib        += list(monolithic_dict[prefix + 'lib'])
        append_to_unique_list(self.libpath, list(monolithic_dict[prefix + 'libpath']))
        self.linkflags  += list(monolithic_dict[prefix + 'linkflags'])
        append_to_unique_list(self.framework, list(monolithic_dict[prefix + 'framework']))
        
    Logs.debug("lumberyard: Applying monolithic build settings for %s ... " % self.name)

    if not hasattr(self.bld, 'monolithic_build_settings'):
        self.bld.monolithic_build_settings = defaultdict(lambda: [])

    # All CryEngineModules use AzCore
    _apply_monolithic_build_settings(self.bld.monolithic_build_settings)

    # if we're compiling a tool that isn't part of a project, then project_name will not be set.
    if getattr(self, 'project_name', None):
        # Add game specific files
        prefix = self.project_name + '_'
        _apply_monolithic_build_settings(self.bld.monolithic_build_settings,prefix)

@feature('apply_monolithic_build_settings')
@after_method('apply_link')
def apply_monolithic_pch_objs(self):
    """ You also need the output obj files from a MS PCH compile to be linked into any modules using it."""
    for tgen_name in self.use:
        other_tg = self.bld.get_tgen_by_name(tgen_name)
        other_pch_task = getattr(other_tg, 'pch_task', None)
        if other_pch_task:
            if other_pch_task.outputs[0] not in self.link_task.inputs:
                Logs.debug('Lumberyard: Monolithic build: Adding pch %s from %s to %s ' % (other_pch_task.outputs[0], tgen_name, self.target))
                self.link_task.inputs.append(other_pch_task.outputs[0])


###############################################################################
# Feature to copy artifacts to the output build folder
@feature('copy_dependent_libs')
@before_method('process_source')
def copy_artifacts(self):

    # Handle copying artifacts
    if getattr(self,'source_artifacts_include',None):
        self.env['SOURCE_ARTIFACTS_INCLUDE'] = getattr(self,'source_artifacts_include',None)
    if getattr(self,'source_artifacts_exclude',None):
        self.env['SOURCE_ARTIFACTS_EXCLUDE'] = getattr(self,'source_artifacts_exclude',None)


###############################################################################
# Feature to mirror collection of artifacts.  The artifacts contents are relative to
# the Bin64 folder.
@feature('mirror_artifacts')
@before_method('process_source')
def mirror_artifacts(self):

    # Handle the mirroring of files from Bin64

    if getattr(self,'mirror_artifacts_to_include',None):
        self.env['MIRROR_ARTIFACTS_INCLUDE'] = getattr(self,'mirror_artifacts_to_include',None)
    if getattr(self,'mirror_artifacts_to_exclude',None):
        self.env['MIRROR_ARTIFACTS_EXCLUDE'] = getattr(self,'mirror_artifacts_to_exclude',None)

@conf
def is_building_dedicated_server(ctx):
    if ctx.cmd == 'configure':
        return False
    if ctx.cmd == 'generate_uber_files':
        return False
    if ctx.cmd == 'generate_module_def_files':
        return False

    config = ctx.env['CONFIGURATION']
    if (config == '') or (config == []) or (config == 'project_generator'):
        return False

    return config.endswith('_dedicated')

@feature('cxx')
@after_method('apply_incpaths')
def apply_msvc_custom_flags(self):

    if not 'msvc' in (self.env.CC_NAME, self.env.CXX_NAME):
        return

    remove_release_define_option = getattr(self,'remove_release_define',False)

    rtti_include_option = getattr(self,'enable_rtti',False)
    rtti_flag = '/GR' if rtti_include_option else '/GR-'

    for t in getattr(self, 'compiled_tasks', []):
        t.env['CXXFLAGS'] = list(filter(lambda r:not r.startswith('/GR'), t.env['CXXFLAGS']))
        t.env['CXXFLAGS'] += [rtti_flag]
        if remove_release_define_option:
            t.env['DEFINES'] = list(filter(lambda r:r!='_RELEASE', t.env['DEFINES']))
