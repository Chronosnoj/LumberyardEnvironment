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
from waflib import Context
from waflib import Logs
import re
import os

PROJECT_SETTINGS_FILE = 'project.json'
PROJECT_NAME_FIELD = 'project_name'

def _load_project_settings(ctx):
    """
    Util function to load the <engine root>/<project name>/project.json file and cache it within the build context.
    """

    if hasattr(ctx, 'projects_settings'):
        return

    engine_root = ctx.root.make_node(Context.launch_dir)

    if os.path.exists(engine_root.make_node('_WAF_').make_node('projects.json').abspath()):
        Logs.warn('projects.json file is deprecated.  Please follow the migration step listed in the release notes.')

    projects_settings = {}
    for project_settings_node in engine_root.ant_glob('*/' + PROJECT_SETTINGS_FILE):
        project_json = ctx.parse_json_file(project_settings_node)
        Logs.debug('lumberyard:  _load_project_settings examining project file %s...' % (project_settings_node.abspath()))
        project_name = project_json.get(PROJECT_NAME_FIELD, '').encode('ASCII', 'ignore')
        if not project_name:
            project_name = project_settings_node.parent.name
            ctx.fatal('"%s" attribute was not found in %s' % (PROJECT_NAME_FIELD, project_settings_node.abspath()))

        if projects_settings.has_key(project_name):
            ctx.fatal('Another project named "%s" has been detected:\n%s\n%s' % (
                    project_name,
                    project_settings_node.parent.abspath(),
                    projects_settings[project_name]['project_node'].abspath()
                ))

        project_json['project_node'] = project_settings_node.parent
        projects_settings[project_name] = project_json
        Logs.debug('lumberyard: project file %s is for project %s' % (project_settings_node.abspath(), project_name))

    ctx.projects_settings = projects_settings

    _validate_project_settings(ctx)

def _validate_project_settings(ctx):
    enabled_game_projects = list(ctx.get_enabled_game_project_list())
        
    # also ensure that the bootstrap.cfg points at an enabled game project:
    if ctx.get_bootstrap_game() not in ctx.projects_settings:
        ctx.fatal('bootstrap.cfg has the enabled game project folder "%s", but this game project was not found in the list of enabled game projects in your waf user_settings.options in the _WAF_ folder.  Please ensure that the project folder configured in your bootstrap.cfg actually exists (check for typos), and is also enabled for compilation in your WAF user settings file in the enabled_game_projects key.\n' % (ctx.get_bootstrap_game()))    
    for project_name in ctx.projects_settings:
        if project_name in enabled_game_projects:
            enabled_game_projects.remove(project_name)

    # the only game projects remaining in enabled_game_projects will be ones which don't have project settings files.
    if enabled_game_projects:
        error_message = ''
        for project in enabled_game_projects:
            error_message += 'Enabled project "%s" was not found. Check if "%s" file exists for the project.\n' % (project, PROJECT_SETTINGS_FILE)
        ctx.fatal(error_message)

def _project_setting_entry(ctx, project, entry, required=True):
    """
    Util function to load an entry from the projects.json file
    """
    _load_project_settings(ctx)

    if not project in ctx.projects_settings:
        ctx.cry_file_error('Cannot find project entry for "%s"' % project, get_project_settings_node(ctx, project).abspath() )

    if not entry in ctx.projects_settings[project]:
        if required:
            ctx.cry_file_error('Cannot find entry "%s" for project "%s"' % (entry, project), get_project_settings_node(ctx, project).abspath())
        else:
            return None

    return ctx.projects_settings[project][entry]


def _project_durango_setting_entry(ctx, project, entry):
    """
    Util function to load an entry from the projects.json file
    """

    durango_settings = _project_setting_entry(ctx,project, 'durango_settings')

    if not entry in durango_settings:
        ctx.cry_file_error('Cannot find entry "%s" for project "%s"' % (entry, project), get_project_settings_node(ctx, project).abspath())

    return durango_settings[entry]

def _project_orbis_setting_entry(ctx, project, entry):
    """
    Util function to load an entry from the projects.json file
    """

    orbis_settings = _project_setting_entry(ctx,project, 'orbis_settings')

    if not entry in orbis_settings:
        ctx.cry_file_error('Cannot find entry "%s" for project "%s"' % (entry, project), get_project_settings_node(ctx, project).abspath())

    return orbis_settings[entry]

#############################################################################
#############################################################################
@conf
def game_projects(self):
    _load_project_settings(self)

    # Build list of game code folders
    projects = []
    for key, value in self.projects_settings.items():
            projects += [ key ]

    # do not filter this list down to only enabled projects!
    return projects

@conf
def project_idx (self, project):
    _load_project_settings(self)
    enabled_projects = self.get_enabled_game_project_list()
    nIdx = 0
    for key, value in self.projects_settings.items():
        if key == project:
            return nIdx
        nIdx += 1
    return nIdx;


#############################################################################

@conf
def get_project_node(self, project):
    return _project_setting_entry(self, project, 'project_node')

@conf
def get_project_settings_node(ctx, project):
    return ctx.get_project_node(project).make_node(PROJECT_SETTINGS_FILE)

@conf
def game_code_folder(self, project):
    return _project_setting_entry(self, project, 'code_folder')

@conf
def get_executable_name(self, project):
    return _project_setting_entry(self, project, 'executable_name')

@conf
def get_dedicated_server_executable_name(self, project):
    return _project_setting_entry(self, project, 'executable_name') + '_Server'

@conf
def project_output_folder(self, project):
    return _project_setting_entry(self, project, 'project_output_folder')

#############################################################################
@conf
def get_launcher_product_name(self, game_project):
    return _project_setting_entry(self, game_project, 'product_name')

#############################################################################
def _get_android_settings_option(ctx, project, option, default = None):
    settings = ctx.get_android_settings(project)
    if settings == None:
        return None;

    return settings.get(option, default)

@conf
def get_android_settings(self, project):
    try:
        return _project_setting_entry(self, project, 'android_settings')
    except:
        pass
    return None

@conf
def get_android_package_name(self, project):
    return _get_android_settings_option(self, project, 'package_name', default = 'com.lumberyard.sdk')

@conf
def get_android_version_number(self, project):
    return str(_get_android_settings_option(self, project, 'version_number', default = 1))

@conf
def get_android_version_name(self, project):
    return _get_android_settings_option(self, project, 'version_name', default = '1.0.0.0')

@conf
def get_android_orientation(self, project):
    return _get_android_settings_option(self, project, 'orientation', default = 'landscape')

@conf
def get_android_app_icons(self, project):
    return _get_android_settings_option(self, project, 'icons', default = None)

@conf
def get_android_app_splash_screens(self, project):
    return _get_android_settings_option(self, project, 'splash_screen', default = None)

@conf
def get_android_place_assets_in_apk_setting(self, project):
    return (_get_android_settings_option(self, project, 'place_assets_in_apk', default = 0) == 1)

@conf
def get_android_app_public_key(self, project):
    return _get_android_settings_option(self, project, 'app_public_key', default = "NoKey")


#############################################################################
@conf
def get_ios_settings(self, project):
    try:
        return _project_setting_entry(self, project, 'ios_settings')
    except:
        pass
    return None

@conf
def get_ios_app_icon(self, project):
    settings = self.get_ios_settings(project)
    if settings == None:
        return 'AppIcon'
    return settings.get('app_icon', None)

@conf
def get_ios_launch_image(self, project):
    settings = self.get_ios_settings(project)
    if settings == None:
        return 'LaunchImage'
    return settings.get('launch_image', None)

#############################################################################
@conf
def get_appletv_settings(self, project):
    try:
        return _project_setting_entry(self, project, 'appletv_settings')
    except:
        pass
    return None

@conf
def get_appletv_app_icon(self, project):
    settings = self.get_appletv_settings(project)
    if settings == None:
        return 'AppIcon'
    return settings.get('app_icon', None)

@conf
def get_appletv_launch_image(self, project):
    settings = self.get_appletv_settings(project)
    if settings == None:
        return 'LaunchImage'
    return settings.get('launch_image', None)

#############################################################################
@conf
def get_mac_settings(self, project):
    try:
        return _project_setting_entry(self, project, 'mac_settings')
    except:
        pass
    return None

@conf
def get_mac_app_icon(self, project):
    settings = self.get_mac_settings(project)
    if settings == None:
        return 'AppIcon'
    return settings.get('app_icon', None)

@conf
def get_mac_launch_image(self, project):
    settings = self.get_mac_settings(project)
    if settings == None:
        return 'LaunchImage'
    return settings.get('launch_image', None)

#############################################################################
@conf
def get_dedicated_server_product_name(self, game_project):
    return _project_setting_entry(self,  game_project, 'product_name') + ' Dedicated Server'

#############################################################################

BUILD_SPECS = ['gems', 'all', 'game_and_engine', 'dedicated_server', 'pipeline', 'resource_compiler', 'shadercachegen']
GAMEPROJECT_DIRECTORIES_LIST = []

@conf
def project_modules(self, project):
    # Using None as a hint to the method that we want the method to get the
    # platfom list itself
    return self.project_and_platform_modules(project, None)

@conf
def project_and_platform_modules(self, project, platforms):
    '''Get the modules for the project that also include modules for the
       specified platforms. The platforms parameter can be either a list of
       platforms to check for, a single platform name, or None to indicate that
       the current supported platforms should be used.'''

    base_modules = _project_setting_entry(self, project, 'modules')

    platform_modules = []
    supported_platforms = [];

    if platforms is None:
        (current_platform, configuration) = self.get_platform_and_configuration()
        supported_platforms = self.get_platform_list(current_platform)
    elif not isinstance(platforms, list):
        supported_platforms.append(platforms)
    else:
        supported_platforms = platforms

    for platform in supported_platforms:
        platform_modules_string = platform + '_modules'
    if platform_modules_string in self.projects_settings[project]:
            platform_modules = platform_modules + _project_setting_entry(self, project, platform_modules_string)

    return base_modules + platform_modules
    
FLAVORS_DEFAULT = {
    "Game": {
        "modules": [
            "LmbrCentral"
        ]
    },
    "Editor": {
        "modules": [
            "LmbrCentralEditor"
        ]
    }
}

@conf
def project_flavor_modules(self, project, flavor):
    """
    Return modules used by the named flavor (ex: 'Game', 'Editor').
    If these entries are not defined, then default values are used.
    """
    flavors_entry = _project_setting_entry(self, project, 'flavors', False)
    if not flavors_entry:
        flavors_entry = FLAVORS_DEFAULT
        
    if flavor in flavors_entry:
        flavor_entry = flavors_entry[flavor]
    elif flavor in FLAVORS_DEFAULT:
        flavor_entry = FLAVORS_DEFAULT[flavor]
    else:
        self.cry_file_error('Cannot find entry "flavors.%s" for project "%s"' % (flavor, project), get_project_settings_node(self, project).abspath())
    
    if 'modules' in flavor_entry:
        return list(flavor_entry['modules'])
    else:
        return []

@conf
def add_game_projects_to_specs(self):
    # Force specs to load
    if not hasattr(self, 'loaded_specs_dict' ):
        self.loaded_specs()

    available_launchers = self.get_available_launchers()
    enabled_projects = self.get_enabled_game_project_list()

    # gems are added to all specs that involve game projects.
    specs_to_include = [spec for spec in self.loaded_specs() if not self.spec_disable_games(spec)]

    Logs.debug('lumberyard: Game Projects are only enabled in the following specs: %s' % specs_to_include)

    for spec_name in specs_to_include:
        spec_dict = self.loaded_specs_dict[spec_name]
        if 'modules' not in spec_dict:
            spec_dict['modules'] = []
        spec_list = spec_dict['modules']

        for project in enabled_projects:
            for module in project_modules(self, project):
                if not module in spec_list:
                    spec_list.append(module)
                    Logs.debug("lumberyard: Added module to spec list: %s for %s" % (module, spec_name))

            # if we have game projects, also allow the building of the launcher from templates:
            for available_launcher_spec in available_launchers:
                if available_launcher_spec not in spec_dict:
                    spec_dict[available_launcher_spec] = []
                spec_list_to_append_to = spec_dict[available_launcher_spec]
                available_spec_list = available_launchers[available_launcher_spec]
                for module in available_spec_list:
                    launcher_name = project + module
                    Logs.debug("lumberyard: Added launcher %s for %s (to %s spec in in %s sub_spec)" % (launcher_name, project, spec_name, available_launcher_spec))
                    spec_list_to_append_to.append(launcher_name)


#############################################################################
@conf
def get_product_name(self, target, game_project):
    if target == 'PCLauncher' or target == 'OrbisLauncher' or target == 'DurangoLauncher':
        return self.get_launcher_product_name(game_project)
    elif target == 'DedicatedLauncher':
        return self.get_dedicated_server_product_name(game_project)
    else:
        return target

#############################################################################
@conf
def project_durango_app_id(self,project):
        return _project_durango_setting_entry(self, project, 'app_id')

@conf
def project_durango_package_name(self,project):
        return _project_durango_setting_entry(self, project, 'package_name')

@conf
def project_durango_version(self,project):
        return _project_durango_setting_entry(self, project, 'version')

@conf
def project_durango_display_name(self,project):
        return _project_durango_setting_entry(self, project, 'display_name')

@conf
def project_durango_publisher_name(self,project):
        return _project_durango_setting_entry(self, project, 'publisher')

@conf
def project_durango_description(self,project):
        return _project_durango_setting_entry(self, project, 'description')

@conf
def project_durango_foreground_text(self,project):
        return _project_durango_setting_entry(self, project, 'foreground_text')

@conf
def project_durango_background_color(self,project):
        return _project_durango_setting_entry(self, project, 'background_color')

@conf
def project_durango_titleid(self,project):
        return _project_durango_setting_entry(self, project, 'titleid')

@conf
def project_durango_scid(self,project):
        return _project_durango_setting_entry(self, project, 'scid')

@conf
def project_durango_appxmanifest(self,project):
        return _project_durango_setting_entry(self, project, 'appxmanifest')

@conf
def project_durango_logo(self,project):
        return _project_durango_setting_entry(self, project, 'logo')

@conf
def project_durango_small_logo(self,project):
        return _project_durango_setting_entry(self, project, 'small_logo')

@conf
def project_durango_splash_screen(self,project):
        return _project_durango_setting_entry(self, project, 'splash_screen')

@conf
def project_durango_store_logo(self,project):
        return _project_durango_setting_entry(self, project, 'store_logo')

#############################################################################
@conf
def project_orbis_data_folder(self,project):
        return _project_orbis_setting_entry     (self, project, 'data_folder')

@conf
def project_orbis_nptitle_dat(self,project):
        return _project_orbis_setting_entry     (self, project, 'nptitle_dat')

@conf
def project_orbis_param_sfo(self,project):
        return _project_orbis_setting_entry     (self, project, 'param_sfo')

@conf
def project_orbis_trophy_trp(self,project):
        return _project_orbis_setting_entry     (self, project, 'trophy_trp')

@conf
def get_enabled_game_project_list(self):
    """Utility function which returns the current game projects enabled in the user prefernces as a list."""
    retVal = self.options.enabled_game_projects
    if (not retVal) or (len(retVal) == 0):
        return []

    if (len(retVal) == 1) and (retVal[0] == ''):
        return []

    retVal = retVal.strip().replace(' ', '').split(',')

    duplicateTracking = set()
    orderedUniqueList = list()

    # remove duplicates, but ensure that the order is maintained
    for x in retVal:
        if not (x in duplicateTracking):
            orderedUniqueList.append(x)
            duplicateTracking.add(x)

    return orderedUniqueList

@conf
def get_bootstrap_game(self):
    """
    :param self:
    :return: Name of the game enabled in bootstrap.cfg
    """
   
    game = 'SamplesProject'
    try:
        bootstrap_cfg = self.path.make_node('bootstrap.cfg')
        bootstrap_contents = bootstrap_cfg.read()
        game = re.search('^\s*sys_game_folder\s*=\s*(\w+)', bootstrap_contents, re.MULTILINE).group(1)
    except:
        pass
    return game
    
@conf
def get_layout_bootstrap_game(self, layout_node):
    """
    :param self:
    :return: Name of the game enabled in layouts bootstrap.cfg
    """
    
    game = 'SamplesProject'
    try:
        bootstrap_cfg = layout_node.make_node('bootstrap.cfg')
        bootstrap_contents = bootstrap_cfg.read()
        game = re.search('^\s*sys_game_folder\s*=\s*(\w+)', bootstrap_contents, re.MULTILINE).group(1)
    except:
        pass
    return game
    
@conf
def get_bootstrap_vfs(self):
    """
    :param self:
    :return: Name of the game enabled in bootstrap.cfg
    """
    bootstrap_cfg = self.path.make_node('bootstrap.cfg')
    bootstrap_contents = bootstrap_cfg.read()
    vfs = '0'
    try:
        vfs = re.search('^\s*remote_filesystem\s*=\s*(\w+)', bootstrap_contents, re.MULTILINE).group(1)
    except:
        pass
    return vfs

GAME_PLATFORM_MAP = {
    'durango': 'xbone',
    'orbis': 'ps4',
}

@conf
def get_bootstrap_assets(self, platform=None):
    """
    :param self:
    :param platform: optional, defaults to current build's platform
    :return: Asset type requested for the supplied platform in bootstrap.cfg
    """
    if platform is None:
        platform = self.env['PLATFORM']
    bootstrap_cfg = self.path.make_node('bootstrap.cfg')
    bootstrap_contents = bootstrap_cfg.read()
    assets = 'pc'
    game_platform = GAME_PLATFORM_MAP.get(platform, platform)
    try:
        assets = re.search('^\s*assets\s*=\s*(\w+)', bootstrap_contents, re.MULTILINE).group(1)
        assets = re.search('^\s*%s_assets\s*=\s*(\w+)' % (game_platform), bootstrap_contents, re.MULTILINE).group(1)
    except:
        pass

    return assets
