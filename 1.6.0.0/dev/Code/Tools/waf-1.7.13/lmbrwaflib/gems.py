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
import os
import re
import uuid
from waflib.Configure import conf
from waflib import Logs
from cry_utils import append_to_unique_list

GEMS_UUID_KEY = "Uuid"
GEMS_VERSION_KEY = "Version"
GEMS_PATH_KEY = "Path"
GEMS_LIST_FILE = "gems.json"
GEMS_DEFINITION_FILE = "gem.json"
GEMS_CODE_FOLDER = 'Code'
GEMS_FOLDER = 'Gems'
GEMS_FORMAT_VERSION = 2

BUILD_SPECS = ['gems', 'all', 'game_and_engine', 'dedicated_server', 'pipeline', 'resource_compiler', 'shadercachegen']
"""Build specs to add Gems to"""

manager = None

def _create_field_reader(ctx, obj, parse_context):
    '''
    Creates a field reader for reading from Json
    Calls ctx.cry_error() if a field not found (except opt variants).

    Args:
        ctx: the Context object to call cry_error() on failure
        obj: the object to read from
        parse_context: string describing the current context, should work following the word 'in'
    Returns:
        A Reader object, used for reading from a JSON object
    '''
    class Reader(object):
        def field_opt(self, field_name, default=None):
            return obj.get(field_name, default)

        def field_req(self, field_name):
            val = self.field_opt(field_name)
            if val != None: # != None required because [] is valid, but triggers false
                return val
            else:
                ctx.cry_error('Failed to read field {} in {}.'.format(
                    field_name, parse_context))
                return None

        def field_int(self, field_name):
            val_str = self.field_req(field_name)
            val = None
            try:
                val = int(val_str)
            except ValueError as err:
                ctx.cry_error('Failed to parse {} field with value {} in {}: {}'.format(
                    field_name, val, parse_context, err.message))
            return val

        def uuid(self, field_name = GEMS_UUID_KEY):
            '''
            Attempt to parse the field into a UUID.
            '''
            id_str = self.field_req(field_name)
            id = None
            try:
                id = uuid.UUID(hex=id_str)
            except ValueError as err:
                ctx.cry_error('Failed to parse {} field with value {} in {}: {}.'.format(
                    field_name, id_str, parse_context, err.message))
            return id

        def version(self, field_name = GEMS_VERSION_KEY):
            ver_str = self.field_req(field_name)
            return Version(ctx, ver_str)

    return Reader()

@conf
def add_gems_to_specs(self):
    manager = GemManager.GetInstance(self)

    # Add Gems to the specs
    manager.add_to_specs()

@conf
def process_gems(self):
    manager = GemManager.GetInstance(self)

    # Recurse into all sub projects
    manager.recurse()

@conf
def GetEnabledGemUUIDs(ctx):
    return [gem.id for gem in GemManager.GetInstance(ctx).gems]

@conf
def GetGemsOutputModuleNames(ctx):
    return [gem.dll_file for gem in GemManager.GetInstance(ctx).gems]

@conf
def GetGemsModuleNames(ctx):
    return [gem.name for gem in GemManager.GetInstance(ctx).gems]

@conf
def DefineGem(ctx, *k, **kw):
    """
    Gems behave very similarly to engine modules, but with a few extra options
    """
    manager = GemManager.GetInstance(ctx)

    gem_path = ctx.path.parent.path_from(ctx.srcnode)
    gem = manager.get_gem_by_path(gem_path)
    if not gem:
        ctx.cry_error("DefineGem must be called by a wscript file in a Gem's 'Code' folder.")

    # Set default properties
    default_settings = {
        'target': gem.name,
        'output_file_name': gem.dll_file,
        'vs_filter': 'Gems',
        'file_list': ["{}.waf_files".format(gem.name.lower())],
        'platforms': ['all'],
        'configurations' : ['all'],
        'defines': [],
        'pch': 'Source/StdAfx.cpp',
        'includes': [],
        'lib': [],
        'libpath': [],
        'features': [],
        'use': [],
        'autod_uselib': []
    }

    for key, value in default_settings.iteritems():
        if key not in kw:
            kw[key] = value

    # Link the auto-registration symbols so that Flow Node registration will work
    append_to_unique_list(kw['use'], ['CryAction_AutoFlowNode', 'AzFramework'])

    # Setup includes
    include_paths = []
    local_includes = ['Include', 'Source']

    # If disable_tests=False or disable_tests isn't specified, enable Google test
    disable_test_settings = ctx.GetPlatformSpecificSettings(kw, 'disable_tests', ctx.env['PLATFORM'], ctx.env['CONFIGURATION'])
    disable_tests = kw.get('disable_tests', False) or any(disable_test_settings)
    # Disable tests when doing monolithic build, except when doing project generation (which is always monolithic)
    disable_tests = disable_tests or (ctx.env['PLATFORM'] != 'project_generator' and ctx.spec_monolithic_build())
    # Disable tests on non-test configurations, except when doing project generation
    disable_tests = disable_tests or (ctx.env['PLATFORM'] != 'project_generator' and 'test' not in ctx.env['CONFIGURATION'])
    if not disable_tests:
        append_to_unique_list(kw['use'], 'AzTest')
        test_waf_files = "{}_tests.waf_files".format(gem.name.lower())
        if ctx.path.find_node(test_waf_files):
            append_to_unique_list(kw['file_list'], test_waf_files)

    # Add local includes
    for local_path in local_includes:
        node = ctx.path.find_node(local_path)
        if node:
            include_paths.append(node)

    # if the gem includes aren't already in the list, ensure they are prepended in order
    gem_includes = []
    for include_path in include_paths:
        if not include_path in kw['includes']:
            gem_includes.append(include_path)
    kw['includes'] = gem_includes + kw['includes']

    # Add includes for dependencies
    for dep_id in gem.dependencies:
        dep = manager.get_gem_by_spec(dep_id)
        if not dep:
            ctx.cry_error('Gem {}({}) has an unmet dependency with ID {}.'.format(gem.id, gem.name, dep_id))
            continue
        append_to_unique_list(kw['includes'], dep.get_include_path())
        if Gem.LinkType.requires_linking(ctx, dep.link_type):
            append_to_unique_list(kw['use'], dep.name)

    ctx.CryEngineSharedLibrary(ctx, *k, **kw)

    # If Gem is marked for having editor module, add it
    if gem.editor_module:
        # If 'editor' object is added to the wscript, ensure it's settings are kept
        editor_kw = kw.get('editor', { })

        # Overridable settings, not to be combined with Game version
        editor_default_settings = {
            'target': gem.editor_module,
            'output_file_name': gem.editor_dll_file,
            'vs_filter': 'Gems',
            'platforms': ['win'],
            'configurations' : ['debug', 'debug_test', 'profile', 'profile_test'],
            'file_list': kw['file_list'] + ["{}_editor.waf_files".format(gem.name.lower())],
            'pch':       kw['pch'],
            'defines':   kw['defines'],
            'includes':  kw['includes'],
            'lib':       kw['lib'],
            'libpath':   kw['libpath'],
            'features':  kw['features'],
            'use':       kw['use'],
            'autod_uselib': kw['autod_uselib'] + ['QT5CORE', 'QT5QUICK', 'QT5GUI', 'QT5WIDGETS'],
        }

        for key, value in editor_default_settings.iteritems():
            if key not in editor_kw:
                editor_kw[key] = value

        # Include required settings
        append_to_unique_list(editor_kw['features'], 'qt5')
        append_to_unique_list(editor_kw['use'], 'AzToolsFramework')

        # Set up for testing
        if not disable_tests:
            append_to_unique_list(editor_kw['use'], 'AzTest')
            test_waf_files = "{}_editor_tests.waf_files".format(gem.name.lower())
            if ctx.path.find_node(test_waf_files):
                append_to_unique_list(editor_kw['file_list'], test_waf_files)

        ctx.CryEngineModule(ctx, *k, **editor_kw)

class Version(object):
    def __init__(self, ctx, ver_str = None):
        self.major = 0
        self.minor = 0
        self.patch = 0
        self.ctx = ctx

        if ver_str:
            self.parse(ver_str)

    def parse(self, string):
        match = re.match(r'(\d+)\.(\d+)\.(\d+)', string)
        if match:
            self.major = int(match.group(1))
            self.minor = int(match.group(2))
            self.patch = int(match.group(3))
        else:
            self.ctx.cry_error('Invalid version format {}. Should be [MAJOR].[MINOR].[PATCH].'.format(string))

    def __str__(self):
        return '{}.{}.{}'.format(self.major, self.minor, self.patch)

    def __eq__(self, other):
        return (isinstance(other, self.__class__)
            and self.major == other.major
            and self.minor == other.minor
            and self.patch == other.patch)

    def __ne__(self, other):
        return not self.__eq__(other)

class Gem(object):
    class LinkType(object):
        Dynamic         = 'Dynamic'
        DynamicStatic   = 'DynamicStatic'
        NoCode          = 'NoCode'

        Types           = [Dynamic, DynamicStatic, NoCode]

        @classmethod
        def requires_linking(cls, ctx, link_type):
            # If NoCode, never link for any reason
            if link_type == cls.NoCode:
                return False

            # When doing monolithic builds, always link (the module system will not do that for us)
            if ctx.spec_monolithic_build():
                return True

            return link_type in [cls.DynamicStatic]

    def __init__(self, ctx):
        self.name = ""
        self.id = None
        self.path = ""
        self.version = Version(ctx)
        self.dependencies = []
        self.dll_file = ""
        self.waf_files = ""
        self.link_type = Gem.LinkType.Dynamic
        self.ctx = ctx
        self.games_enabled_in = []
        self.editor_targets = []
        self.is_legacy_igem = False
        self.editor_module = None

    def load_from_json(self, gem_def):
        '''
        Parses a Gem description file from JSON.
        Requires that self.path already be set.
        '''
        self._upgrade_gem_json(gem_def)

        reader = _create_field_reader(self.ctx, gem_def, 'Gem in ' + self.path)

        self.name = reader.field_req('Name')
        self.version = reader.version()
        self.id = reader.uuid()
        self.dll_file = reader.field_opt('DllFile', self._get_output_name())
        self.editor_dll_file = self._get_editor_output_name()
        self.editor_targets = reader.field_opt('EditorTargets', [])
        self.is_legacy_igem = reader.field_opt('IsLegacyIGem', False)

        # If EditorModule specified, provide name of name + Editor
        if reader.field_opt('EditorModule', False):
            self.editor_module = self.name + '.Editor'
            self.editor_targets.append(self.editor_module)

        for dep_obj in reader.field_opt('Dependencies', list()):
            dep_reader = _create_field_reader(self.ctx, dep_obj, '"Dependencies" field in Gem in ' + self.path)
            self.dependencies.append(dep_reader.uuid())

        self.link_type = reader.field_req('LinkType')
        if self.link_type not in Gem.LinkType.Types:
            self.ctx.cry_error('Gem.json file\'s "LinkType" value "{}" is invalid, please supply one of:\n{}'.format(self.link_type, Gem.LinkType.Types))

    def _upgrade_gem_json(self, gem_def):
        """
        If gems file is in an old format, update the data within.
        """
        reader = _create_field_reader(self.ctx, gem_def, 'Gem in ' + self.path)

        latest_format_version = 3
        gem_format_version = reader.field_int('GemFormatVersion')

        # Can't upgrade ancient versions or future versions
        if (gem_format_version < 2 or gem_format_version > latest_format_version):
            reader.ctx.cry_error(
                'Field "GemsFormatVersion" is {}, not expected version {}. Please update your Gem file in {}.'.format(
                    gem_format_version, latest_format_version, self.path))

        # v2 -> v3
        if (gem_format_version < 3):
            gem_def['IsLegacyIGem'] = True

        # File is now up to date
        gem_def['GemFormatVersion'] = latest_format_version

    def _get_output_name(self):
        return 'Gem.{}.{}.v{}'.format(self.name, self.id.hex, str(self.version))

    def _get_editor_output_name(self):
        return 'Gem.{}.Editor.{}.v{}'.format(self.name, self.id.hex, str(self.version))

    def get_include_path(self):
        return self.ctx.srcnode.make_node(self.path).make_node([GEMS_CODE_FOLDER, 'Include'])

    def _get_spec(self):
        return (self.id, self.version, self.path)
    spec = property(_get_spec)

class GemManager(object):
    """This class loads all gems for all enabled game projects
    Once loaded:
       the dirs property contains all gem folders
       the gems property contains a list of Gem objects, for only the ACTIVE, ENABLED gems that cover all game projects
       the ctx property is the build context
       """

    def __init__(self, ctx):
        self.dirs = []
        self.gems = []
        self.ctx = ctx

    def get_gem_by_path(self, path):
        """
        Gets the Gem with the corresponding path
        :param path The path to the Gem to look up.
        :ptype path str
        :rtype : Gem
        """
        for gem in self.gems:
            if os.path.normpath(path) == os.path.normpath(gem.path):
                return gem

        return None

    def get_gem_by_spec(self, *spec):
        """
        Gets the Gem with the corresponding id
        :param spec The spec to the Gem to look up (id, [opt] version, [opt] path).
        :ptype path tuple
        :rtype : Gem
        """
        if spec.count < 1:
            return None

        check_funs = [
            lambda gem: gem.id == spec[0],
            lambda gem: check_funs[0](gem) and gem.version == spec[1],
            lambda gem: check_funs[1](gem) and gem.path == spec[2],
            ]

        for gem in self.gems:
            if check_funs[len(spec) - 1](gem):
                return gem

        return None

    def contains_gem(self, *gem_spec):
        return self.get_gem_by_spec(*gem_spec) != None

    def process(self):
        """Process current directory for gems
        Note that this has to check each game project to know which gems are enabled
        and build a list of all enabled gems so that those are built.
        To debug gems output during build, use --zones=gems in your command line
        """
        this_path = self.ctx.path
        game_projects = self.ctx.get_enabled_game_project_list()

        for game_project in game_projects:
            Logs.debug('gems: Game Project: %s' % game_project)

            gems_list_file = self.ctx.get_project_node(game_project).make_node(GEMS_LIST_FILE)

            if not os.path.isfile(gems_list_file.abspath()):
                if self.ctx.is_option_true('gems_optional'):
                    Logs.debug("gems: Game has no gems file, skipping [%s]" % gems_list_file)
                    continue # go to the next game
                else:
                    self.ctx.cry_error('Project {} is missing {} file.'.format(game_project, GEMS_LIST_FILE))

            Logs.debug('gems: reading gems file at %s' % gems_list_file)

            gem_info_list = self.ctx.parse_json_file(gems_list_file)
            list_reader = _create_field_reader(self.ctx, gem_info_list, 'Gems list for project ' + game_project)

            # Verify that the project file is an up-to-date format
            gem_format_version = list_reader.field_int('GemListFormatVersion')
            if gem_format_version != GEMS_FORMAT_VERSION:
                self.ctx.cry_error(
                    'Gems list file at {} is of version {}, not expected version {}. Please update your project file.'.format(
                        gems_list_file, gem_format_version, GEMS_FORMAT_VERSION))

            for idx, gem_info_obj in enumerate(list_reader.field_req('Gems')):
                # String for error reporting.
                reader = _create_field_reader(self.ctx, gem_info_obj, 'Gem {} in game project {}'.format(idx, game_project))

                gem_id = reader.uuid()
                version = reader.version()
                path = os.path.normpath(reader.field_req('Path'))

                gem = self.get_gem_by_spec(gem_id, version, path)
                if not gem:
                    Logs.debug('gems: Gem not found in cache, attempting to load from disk: ({}, {}, {})'.format(id, version, path))

                    def_file = this_path.make_node(path).make_node(GEMS_DEFINITION_FILE)
                    if not os.path.isfile(def_file.abspath()):
                        self.ctx.cry_error('Gem definition file not found: {}'.format(def_file.abspath()))

                    gem = Gem(self.ctx)
                    gem.path = path
                    gem.load_from_json(self.ctx.parse_json_file(def_file))

                    # Validate that the Gem loaded from the path specified actually matches the id and version.
                    if gem.id != gem_id:
                        self.ctx.cry_error("Gem at path {} has ID {}, instead of ID {} specified in {}'s {}.".format(
                            path, gem.id, gem_id, game_project, GEMS_LIST_FILE))

                    if gem.version != version:
                        self.ctx.cry_error("Gem at path {} has version {}, instead of version {} specified in {}'s {}.".format(
                            path, gem.version, version, game_project, GEMS_LIST_FILE))

                    self.add_gem(gem)

                gem.games_enabled_in.append(game_project)

        for gem in self.gems:
            Logs.debug("gems: gem %s is used by games: %s" % (gem.name, gem.games_enabled_in))

    def add_gem(self, gem):
        """Adds gem to the collection"""
        # Skip any disabled Gems
        # don't add duplicates!
        if (self.contains_gem(*gem.spec)):
            return
        self.gems.append(gem)
        code_path = self.ctx.path.make_node(gem.path).make_node(GEMS_CODE_FOLDER).abspath()
        if os.path.isdir(code_path):
            self.dirs.append(code_path)
        else:
            Logs.debug('gems: gem Code folder does not exist %s - this is okay if the gem contains only assets and no code' % code_path)

    def add_to_specs(self):
        # Force specs to load
        if not hasattr(self.ctx, 'loaded_specs_dict'):
            self.ctx.loaded_specs()

        # gems are added to all specs that involve game projects.
        specs_to_include = [spec for spec in self.ctx.loaded_specs() if not self.ctx.spec_disable_games(spec)] + ['gems']
        Logs.debug("gems: Game Projects are only enabled in the following specs: %s" % specs_to_include)

        # Create Gems spec
        if not 'gems' in self.ctx.loaded_specs_dict:
            self.ctx.loaded_specs_dict['gems'] = dict(description="Configuration to build all Gems.",
                                                      valid_configuration=['debug', 'profile', 'release'],
                                                      valid_platforms=['win_x64'],
                                                      visual_studio_name='Gems',
                                                      modules=['CryAction', 'CryAction_AutoFlowNode'])

        # Add to build specs
        for gem in self.gems:
            for spec_name in specs_to_include:
                Logs.debug('gems: adding gem %s to spec %s' % (gem.name, spec_name))
                spec_dict = self.ctx.loaded_specs_dict[spec_name]
                if 'modules' not in spec_dict:
                    spec_dict['modules'] = []
                spec_list = spec_dict['modules']
                if gem.name not in spec_list:
                    spec_list.append(gem.name)
                if gem.editor_targets:
                    for editor_target in gem.editor_targets:
                        if (spec_name == 'all'): # only when building the ALL do we do editor plugins.
                            Logs.debug('gems: adding editor target of gem %s - %s to spec %s' % (gem.name, editor_target, spec_name))
                            spec_list.append(editor_target)

    def recurse(self):
        """Tells WAF to build active Gems"""
        self.ctx.recurse(self.dirs)

    @staticmethod
    def GetInstance(ctx):
        """
        Initializes the Gem manager if it hasn't already
        :rtype : GemManager
        """
        if not hasattr(ctx, 'gem_manager'):
            ctx.gem_manager = GemManager(ctx)
            ctx.gem_manager.process()
        return ctx.gem_manager

@conf
def get_game_gems(ctx, game_project):
    return [gem for gem in GemManager.GetInstance(ctx).gems if game_project in gem.games_enabled_in]

@conf
def apply_gems_to_context(ctx, game_name, k, kw):
    """this function is called whenever its about to generate a target that should be aware of gems"""
    game_gems = ctx.get_game_gems(game_name)

    Logs.debug('gems: adding gems to %s (%s_%s) : %s' % (game_name, ctx.env['CONFIGURATION'], ctx.env['PLATFORM'], [gem.name for gem in game_gems]))

    kw['includes']   += [gem.get_include_path() for gem in game_gems]
    kw['use']        += [gem.name for gem in game_gems if Gem.LinkType.requires_linking(ctx, gem.link_type)]

@conf
def is_gem(ctx, module_name):
    for gem in GemManager.GetInstance(ctx).gems:
        if module_name == gem.name:
            return True
    return False
