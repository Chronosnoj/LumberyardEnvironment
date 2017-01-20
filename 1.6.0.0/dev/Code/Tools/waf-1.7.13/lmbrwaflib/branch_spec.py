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

## Description: Central configuration file for a branch, should never be
##              integrated since it is unique for each branch

########### Below are various getter to expose the global values ############
from waflib.Configure import conf
from waflib import Context, Utils, Logs
from waflib import Errors

from waf_branch_spec import BINTEMP_FOLDER

from waf_branch_spec import COMPANY_NAME
from waf_branch_spec import COPYRIGHT_AMAZON
from waf_branch_spec import COPYRIGHT_CRYTEK
from waf_branch_spec import PLATFORMS
from waf_branch_spec import CONFIGURATIONS
from waf_branch_spec import MONOLITHIC_BUILDS
from waf_branch_spec import MSVS_PLATFORM_VERSION
from waf_branch_spec import AVAILABLE_LAUNCHERS
from waf_branch_spec import CRYTEK_ORIGINAL_PROJECTS
from waf_branch_spec import LUMBERYARD_VERSION
from waf_branch_spec import LUMBERYARD_BUILD
from waf_branch_spec import PLATFORM_SHORTCUT_ALIASES
from waf_branch_spec import CONFIGURATION_SHORTCUT_ALIASES
from waf_branch_spec import PLATFORM_CONFIGURATION_FILTER
import json
import copy

#############################################################################
#############################################################################
@conf
def get_bintemp_folder_node(self):
    return self.root.make_node(Context.launch_dir).make_node(BINTEMP_FOLDER)

#############################################################################
@conf
def get_dep_proj_folder_name(self):
    return self.options.visual_studio_solution_name + '.depproj'

#############################################################################
@conf
def get_project_output_folder(self):
    project_folder_node = self.root.make_node(Context.launch_dir).make_node(self.options.visual_studio_solution_folder).make_node(self.get_dep_proj_folder_name())
    project_folder_node.mkdir()
    return project_folder_node

#############################################################################
@conf
def get_solution_name(self):
    return self.options.visual_studio_solution_folder + '/' + self.options.visual_studio_solution_name + '.sln'

#############################################################################
@conf
def get_solution_dep_proj_folder_name(self):
    return self.options.visual_studio_solution_folder + '/' + self.get_dep_proj_folder_name()

#############################################################################
#############################################################################
@conf
def get_android_project_relative_path(self):
    return self.options.android_studio_project_folder + '/' + self.options.android_studio_project_name

@conf
def get_appletv_project_name(self):
    return self.options.appletv_project_folder + '/' + self.options.appletv_project_name

@conf
def get_ios_project_name(self):
    return self.options.ios_project_folder + '/' + self.options.ios_project_name

@conf
def get_mac_project_name(self):
    return self.options.mac_project_folder + '/' + self.options.mac_project_name

@conf
def get_platform_alias(self, platform_key):
    """
    Return the expanded value of a (target) platform alias if any.  If no alias, return None
    """
    if platform_key in PLATFORM_SHORTCUT_ALIASES:
        return PLATFORM_SHORTCUT_ALIASES[platform_key]
    else:
        return None

#############################################################################
#############################################################################
@conf
def get_company_name(self):
    return COMPANY_NAME

#############################################################################
@conf
def get_lumberyard_version(self):
    return LUMBERYARD_VERSION

#############################################################################
@conf
def get_lumberyard_build(self):
    return LUMBERYARD_BUILD

#############################################################################
@conf
def get_game_project_version(self):
    return self.options.version

#############################################################################
@conf
def get_copyright(self,project_name):
    if self.is_project_original_crytek(project_name):
        return COPYRIGHT_CRYTEK
    else:
        return COPYRIGHT_AMAZON

@conf
def _get_valid_platform_info_filename(self):
    return "valid_configuration_platforms.json"

#############################################################################
#############################################################################

@conf
def get_supported_platforms(self):
    host = Utils.unversioned_sys_platform()
    return PLATFORMS[host]

## Determine if a waf_platform is compatible with a version of MSVS
@conf
def check_msvs_compatibility(self, waf_platform, version):

    if not waf_platform in MSVS_PLATFORM_VERSION:
        return False
    if not version in MSVS_PLATFORM_VERSION[waf_platform]:
        return False
    else:
        return True

@conf
def get_validated_platforms(self):
    try:
        return self.validly_configured_platforms
    except:
        pass

    #try reading the platforms from a file
    fileNode = self.get_bintemp_folder_node().make_node(self._get_valid_platform_info_filename())
    platformList = []

    try:
        platformList = self.parse_json_file(fileNode)
        platformList = [x.encode('ascii') for x in platformList]  #Loaded data is unicode so convert it to ASCII

        self.validly_configured_platforms = platformList  #store this so we don't have to keep loading the file on subsequent calls
        return self.validly_configured_platforms
    except:
        Logs.info("Can't load validated platforms file - using platforms specified for host") ##Too spammy - left in for debugging
        pass

    #the ultimate fallback - return the values from waf_branch_spec
    host = Utils.unversioned_sys_platform()
    return PLATFORMS[host]


@conf
def clear_empty_platforms(self):
    host = Utils.unversioned_sys_platform()
    PLATFORMS[host] = [x for x in PLATFORMS[host] if x != '']

    #write out the list of platforms that were successfully configured
    jsonData = json.dumps(PLATFORMS[host])
    output_file = self.get_bintemp_folder_node().make_node(self._get_valid_platform_info_filename())
    output_file.write(jsonData)

#############################################################################
#############################################################################
@conf
def set_supported_platforms(self, platforms):
    host = Utils.unversioned_sys_platform()
    PLATFORMS[host] = platforms

@conf
def mark_supported_platform_for_removal(self, platform):
    host = Utils.unversioned_sys_platform()
    PLATFORMS[host] = [x if x != platform else '' for x in PLATFORMS[host]]


#############################################################################
@conf
def get_supported_configurations(self, platform=None):
    return PLATFORM_CONFIGURATION_FILTER.get(platform, CONFIGURATIONS)


#############################################################################
@conf
def get_available_launchers(self):
    return AVAILABLE_LAUNCHERS

#############################################################################
#############################################################################
@conf
def get_project_vs_filter(self, target):
    if not hasattr(self, 'vs_project_filters'):
        self.fatal('vs_project_filters not initialized')

    if not target in self.vs_project_filters:
        self.fatal('No visual studio filter entry found for %s' % target)

    return self.vs_project_filters[target]

#############################################################################
#############################################################################
def _load_android_settings(ctx):
    """ Helper function for loading the global android settings """

    if hasattr(ctx, 'android_settings'):
        return

    ctx.android_settings = {}
    settings_file = ctx.root.make_node(Context.launch_dir).make_node([ '_WAF_', 'android', 'android_settings.json' ])
    try:
        ctx.android_settings = ctx.parse_json_file(settings_file)
    except Exception as e:
        ctx.cry_file_error(str(e), settings_file.abspath())


def _get_android_setting(ctx, setting):
    """" Helper function for getting android settings """
    _load_android_settings(ctx)

    return ctx.android_settings[setting]

@conf
def get_android_dev_keystore_alias(conf):
    return _get_android_setting(conf, 'DEV_KEYSTORE_ALIAS')

@conf
def get_android_dev_keystore_path(conf):
    local_path = _get_android_setting(conf, 'DEV_KEYSTORE')
    debug_ks_node = conf.root.make_node(Context.launch_dir).make_node(local_path)
    return debug_ks_node.abspath()

@conf
def get_android_distro_keystore_alias(conf):
    return _get_android_setting(conf, 'DISTRO_KEYSTORE_ALIAS')

@conf
def get_android_distro_keystore_path(conf):
    local_path = _get_android_setting(conf, 'DISTRO_KEYSTORE')
    release_ks_node = conf.root.make_node(Context.launch_dir).make_node(local_path)
    return release_ks_node.abspath()

@conf
def get_android_build_environment(conf):
    env = _get_android_setting(conf, 'BUILD_ENVIRONMENT')
    if env == 'Development' or env == 'Distribution':
        return env
    else:
        Logs.fatal('[ERROR] Invalid android build environment, valid environments are: Development and Distribution')

@conf
def get_android_env_keystore_alias(conf):
    env = conf.get_android_build_environment()
    if env == 'Development':
        return conf.get_android_dev_keystore_alias()
    elif env == 'Distribution':
        return conf.get_android_distro_keystore_alias()

@conf
def get_android_env_keystore_path(conf):
    env = conf.get_android_build_environment()
    if env == 'Development':
        return conf.get_android_dev_keystore_path()
    elif env == 'Distribution':
        return conf.get_android_distro_keystore_path()

@conf
def get_android_build_tools_version(conf):
    return _get_android_setting(conf, 'BUILD_TOOLS_VER')

@conf
def get_android_sdk_version(conf):
    return _get_android_setting(conf, 'SDK_VERSION')

#############################################################################
#############################################################################
def _load_environment_file(ctx):
    """ Helper function for loading the environment file created by Setup Assistant """

    if hasattr(ctx, 'local_environment'):
        return

    ctx.local_environment = {}
    settings_file = ctx.root.make_node(Context.launch_dir).make_node([ '_WAF_', 'environment.json' ])
    try:
        ctx.local_environment = ctx.parse_json_file(settings_file)
    except Exception as e:
        ctx.cry_file_error(str(e), settings_file.abspath())

def _get_environment_file_var(ctx, env_var):
    _load_environment_file(ctx)
    return ctx.local_environment[env_var]

@conf
def get_env_file_var(conf, env_var, required = False, silent = False):

    try:
        return _get_environment_file_var(conf, env_var)
    except:
        if not silent:
            message = 'Failed to find %s in _WAF_/environment.json.' % env_var
            if required:
                Logs.error('[ERROR] %s' % message)
            else:
                Logs.warn('[WARN] %s' % message)
        pass

    return ''

#############################################################################
#############################################################################

def _load_specs(ctx):
    """ Helper function to find all specs stored in _WAF_/specs/*.json """
    if hasattr(ctx, 'loaded_specs_dict'):
        return

    ctx.loaded_specs_dict = {}
    spec_file_folder    = ctx.root.make_node(Context.launch_dir).make_node('/_WAF_/specs')
    spec_files              = spec_file_folder.ant_glob('**/*.json')

    for file in spec_files:
        try:
            spec = ctx.parse_json_file(file)
            spec_name = str(file).split('.')[0]
            ctx.loaded_specs_dict[spec_name] = spec
        except Exception as e:
            ctx.cry_file_error(str(e), file.abspath())


@conf
def loaded_specs(ctx):
    """ Get a list of the names of all specs """
    _load_specs(ctx)

    ret = []
    for (spec,entry) in ctx.loaded_specs_dict.items():
        ret.append(spec)

    return ret

def _spec_entry(ctx, entry, spec_name=None, platform=None, configuration=None):
    """ Util function to load a specifc spec entry (always returns a list) """
    _load_specs(ctx)

    if not spec_name: # Fall back to command line specified spec if none was given
        spec_name = ctx.options.project_spec

    spec = ctx.loaded_specs_dict.get(spec_name, None)
    if not spec:
        ctx.fatal('[ERROR] Unknown Spec "%s", valid settings are "%s"' % (spec_name, ', '.join(ctx.loaded_specs())))

    def _to_list( value ):
        if isinstance(value,list):
            return value
        return [ value ]

    # copy the list since its kept in memory to prevent the 'get platform specifics' call
    # from mutating the list with a bunch of duplicate entries
    res = _to_list( spec.get(entry, []) )[:]

    # The options context is missing an env attribute, hence don't try to find platform settings in this case
    if hasattr(ctx, 'env'):
        if not platform:
            platform = ctx.env['PLATFORM']
        if not configuration:
            configuration = ctx.env['CONFIGURATION']
        res += ctx.GetPlatformSpecificSettings(spec, entry, platform, configuration)

    return res

@conf
def spec_modules(ctx, spec_name = None, platform=None, configuration=None):
    """ Get all a list of all modules needed for this spec """

    module_list = _spec_entry(ctx, 'modules', spec_name)
    if platform is not None:
        platform_filtered_module_list = [module for module in module_list if platform in ctx.get_module_platforms(module)]
        module_list = platform_filtered_module_list
    if configuration is not None:
        configuration_filtered_module_list = [module for module in module_list if configuration in ctx.get_module_configurations(module, platform)]
        module_list = configuration_filtered_module_list

    return module_list


@conf
def is_project_spec_specified(ctx):
    return len(ctx.options.project_spec) > 0


@conf
def is_target_enabled(ctx, target):
    """
    Check if the target is enabled for the build based on whether a spec file was specified
    """
    if is_project_spec_specified(ctx):
        return (target in ctx.spec_modules()) or (target in ctx.get_all_module_uses())
    else:
        # No spec means all projects are enabled
        return True


@conf
def spec_defines(ctx, spec_name = None):
    """ Get all a list of all defines needed for this spec """
    """ This function is deprecated """
    return _spec_entry(ctx, 'defines', spec_name)

@conf
def spec_platforms(ctx, spec_name = None):
    """ Get all a list of all defines needed for this spec """
    """ This function is deprecated """
    return _spec_entry(ctx, 'platforms', spec_name)

@conf
def spec_configurations(ctx, spec_name = None):
    """ Get all a list of all defines needed for this spec """
    """ This function is deprecated """
    return _spec_entry(ctx, 'configurations', spec_name)

@conf
def spec_visual_studio_name(ctx, spec_name = None):
    """ Get all a the name of this spec for Visual Studio """
    visual_studio_name =  _spec_entry(ctx, 'visual_studio_name', spec_name)

    if not visual_studio_name:
        ctx.fatal('[ERROR] Mandatory field "visual_studio_name" missing from spec "%s"' % (spec_name or ctx.options.project_spec) )

    # _spec_entry always returns a list, but client code expects a single string
    assert( len(visual_studio_name) == 1 )
    return visual_studio_name[0]


@conf
def spec_description(ctx, spec_name = None):
    """ Get description for this spec """
    description = _spec_entry(ctx, 'description', spec_name)
    if description == []:
        ctx.fatal('[ERROR] Mandatory field "description" missing from spec "%s"' % (spec_name or ctx.options.project_spec) )

    # _spec_entry always returns a list, but client code expects a single string
    assert( len(description) == 1 )
    return description[0]

@conf
def spec_disable_games(ctx, spec_name = None):
    """ For a given spec, are game projects disabled?  For example, tool-only specs do this."""

    # If no spec is supplied, then no games are disabled
    if len(ctx.options.project_spec)==0:
        return False

    disable_games = _spec_entry(ctx, 'disable_game_projects', spec_name)
    if disable_games == []:
        return False

    return disable_games[0]


@conf
def spec_monolithic_build(ctx, spec_name = None):
    """ Return true if the current platform|configuration should be a monolithic build in this spec """
    if not spec_name: # Fall back to command line specified spec if none was given
        spec_name = ctx.options.project_spec

    current_confs = ctx.env['CONFIGURATION']
    current_plats = ctx.get_platform_list( ctx.env['PLATFORM'] )

    if isinstance(current_confs, str):
        current_confs = [current_confs] # it needs to be a list

    # Check for entry in <platform> style
    for platform in current_plats:
        platform = platform.strip();
        if platform in MONOLITHIC_BUILDS:
            Logs.debug("lumberyard: spec_monolithic_build(" + str(current_plats) + spec_name +") == True")
            return True

    # Check for entry in <configuration>
    for config in current_confs:
        if config in MONOLITHIC_BUILDS:
            Logs.debug("lumberyard: spec_monolithic_build(" + str(current_plats) + spec_name +") == True")
            return True

    # Check for entry in <platform>_<configuration> style
    for platform in current_plats:
        for config in current_confs:
            platform_configuration = platform + '_' + config
            if platform_configuration in MONOLITHIC_BUILDS:
                Logs.debug("lumberyard: spec_monolithic_build(" + str(current_plats) + spec_name +") == True")
                return True

    Logs.debug("lumberyard: spec_monolithic_build(" + str(current_plats) + spec_name +") == False")
    return False


@conf
def is_project_original_crytek(ctx, project_name):
    return project_name in CRYTEK_ORIGINAL_PROJECTS


@conf
def is_cmd_monolithic(ctx):
    '''
    Determine if a build command is tagged as a monolithic build
    '''
    for monolithic_key in MONOLITHIC_BUILDS:
        if monolithic_key in ctx.cmd:
            return True

    return False


# Set global output directory
setattr(Context.g_module, Context.OUT, BINTEMP_FOLDER)

# List of platform specific envs that will be filtered out in the env cache
ELIGIBLE_PLATFORM_SPECIFIC_ENV_KEYS = [ 'INCLUDES',
                                        'FRAMEWORKPATH',
                                        'DEFINES',
                                        'CPPFLAGS',
                                        'CCDEPS',
                                        'CFLAGS',
                                        'ARCH',
                                        'CXXDEPS',
                                        'CXXFLAGS',
                                        'DFLAGS',
                                        'LIB',
                                        'STLIB',
                                        'LIBPATH',
                                        'STLIBPATH',
                                        'LINKFLAGS',
                                        'RPATH',
                                        'LINKDEPS',
                                        'FRAMEWORK',
                                        'ARFLAGS',
                                        'ASFLAGS']

ELIGIBLE_PLATFORMS = copy.copy(PLATFORMS)

@conf
def derive_configset_for_platform(ctx, env_configset, platform):
    """
    This method will derive a new config set based on an source config set and will filter and process platform tagged
    entries in the table.  This method will only process keys that are specified in ELIGIBLE_PLATFORM_SPECIFIC_ENV_KEYS
    """

    host = Utils.unversioned_sys_platform()
    eligible_platforms = ELIGIBLE_PLATFORMS[host]

    # Derive from the source config set and detach
    filtered_configset = env_configset.derive()
    filtered_configset.detach()

    for env_key in env_configset.keys():

        # Only evaluate keys that have a '_' delimiter
        if '_' not in env_key:
            continue

        first_delim_index = env_key.find('_')
        last_delim_index = env_key.rfind('_')

        # If there is only one '_' delimiter, or the delimiters are next to each other, that means this is not a
        # platform tagged uselib.
        if (last_delim_index-first_delim_index)<2:
            continue

        # Parse out the root, platform, and key
        env_partial_root = env_key[:first_delim_index]
        env_partial_platform = env_key[first_delim_index+1:last_delim_index]
        env_partial_key = env_key[last_delim_index+1:]

        # If this matches the current platform, reduce the name by removing the platform key
        if env_partial_platform == platform:
            processed_env_key = '{}_{}'.format(env_partial_root,env_partial_key)
            filtered_configset[processed_env_key] = env_configset[env_key]
        # Remove any platform embedded eligible key
        del filtered_configset[env_key]
        filtered_configset.table.pop(env_key,None)

    return filtered_configset

@conf
def preprocess_platform_list(ctx, platforms, auto_populate_empty=False):
    """
    Preprocess a list of platforms from user input
    """
    host_platform = Utils.unversioned_sys_platform()
    processed_platforms = set()
    if (auto_populate_empty and len(platforms) == 0) or 'all' in platforms:
        for platform in PLATFORMS[host_platform]:
            processed_platforms.add(platform)
    else:
        for platform in platforms:
            if platform in PLATFORMS[host_platform]:
                processed_platforms.add(platform)
            else:
                aliased_platforms = ctx.get_platform_alias(platform)
                if aliased_platforms:
                    for aliased_platform in aliased_platforms:
                        processed_platforms.add(aliased_platform)
    return processed_platforms

@conf
def preprocess_configuration_list(ctx, target, target_platform, configurations, auto_populate_empty=False):
    """
    Preprocess a list of configurations based on an optional target_platform filter from user input
    """

    processed_configurations = set()
    if (auto_populate_empty and len(configurations) == 0) or 'all' in configurations:
        for configuration in CONFIGURATIONS:
            processed_configurations.add(configuration)
    else:
        for configuration in configurations:
            # Matches an existing configuration
            if configuration in CONFIGURATIONS:
                processed_configurations.add(configuration)
            # Matches an alias
            elif configuration in CONFIGURATION_SHORTCUT_ALIASES:
                for aliased_configuration in CONFIGURATION_SHORTCUT_ALIASES[configuration]:
                    processed_configurations.add(aliased_configuration)
            elif ':' in configuration and target_platform is not None:
                # Could be a platform-only configuration, split into components
                configuration_parts = configuration.split(':')
                split_platform = configuration_parts[0]
                split_configuration = configuration_parts[1]

                # Apply the configuration alias here as well
                if split_configuration in CONFIGURATION_SHORTCUT_ALIASES:
                    split_configurations = CONFIGURATION_SHORTCUT_ALIASES[split_configuration]
                elif split_configuration in CONFIGURATIONS:
                    split_configurations = [split_configuration]
                else:
                    split_configurations = []
                    if target:
                        Logs.warn('[WARN] configuration value {} in module {} is invalid.'.format(configuration, target))
                    else:
                        Logs.warn('[WARN] configuration value {}.'.format(configuration))

                # Apply the platform alias here as well
                split_platforms = preprocess_platform_list(ctx,[split_platform])

                # Iterate through the platform(s) and add the matched configurations
                for split_platform in split_platforms:
                    if split_platform == target_platform:
                        for split_configuration in split_configurations:
                            processed_configurations.add(split_configuration)
            else:
                if target:
                    Logs.warn('[WARN] configuration value {} in module {} is invalid.'.format(configuration, target))
                else:
                    Logs.warn('[WARN] configuration value {}.'.format(configuration))

    return processed_configurations
