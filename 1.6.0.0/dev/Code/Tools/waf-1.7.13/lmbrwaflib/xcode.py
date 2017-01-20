#! /usr/bin/env python
# encoding: utf-8

# Modifications copyright Amazon.com, Inc. or its affiliates.

# XCode 3/XCode 4 generator for Waf
# Nicolas Mercier 2011
# Christopher Bolte: Created a copy to easier adjustment to crytek specific changes

"""
Usage:

def options(opt):
    opt.load('xcode')

$ waf configure xcode_ios
$ waf configure xcode_mac
$ waf configure xcode_appletv
"""

from waflib import Context, TaskGen, Build, Utils, Logs
from waflib.TaskGen import feature, after_method
import os, sys, copy

HEADERS_GLOB = '**/(*.h|*.hpp|*.H|*.inl)'

MAP_EXT = {
    '.h' :  "sourcecode.c.h",

    '.hh':  "sourcecode.cpp.h",
    '.inl': "sourcecode.cpp.h",
    '.hpp': "sourcecode.cpp.h",

    '.c':   "sourcecode.c.c",

    '.m':   "sourcecode.c.objc",

    '.mm':  "sourcecode.cpp.objcpp",

    '.cc':  "sourcecode.cpp.cpp",

    '.cpp': "sourcecode.cpp.cpp",
    '.C':   "sourcecode.cpp.cpp",
    '.cxx': "sourcecode.cpp.cpp",
    '.c++': "sourcecode.cpp.cpp",

    '.l':   "sourcecode.lex", # luthor
    '.ll':  "sourcecode.lex",

    '.y':   "sourcecode.yacc",
    '.yy':  "sourcecode.yacc",

    '.plist': "text.plist.xml",
    ".nib":   "wrapper.nib",
    ".xib":   "text.xib",
}

do_debug_print = False
root_dir = ''
id = 562000999
uintmax = 2147483647

def newid():
    global id
    id = id + 1
    return "%04X%04X%04X%012d" % (0, 10000, 0, id)

def XCodeEscapeSpacesForShell(spaceString):
    return spaceString.replace(' ', '\\ ')

class XCodeNode:
    def __init__(self):
        self._id = newid()

    def tostring(self, value):
        if isinstance(value, dict):
            result = "{\n"
            for k,v in value.items():
                result = result + "\t\t\t%s = %s;\n" % (k, self.tostring(v))
            result = result + "\t\t}"
            return result
        elif isinstance(value, str):
            return "\"%s\"" % value
        elif isinstance(value, list):
            result = "(\n"
            for i in value:
                result = result + "\t\t\t%s,\n" % self.tostring(i)
            result = result + "\t\t)"
            return result
        elif isinstance(value, XCodeNode):
            return value._id
        else:
            return str(value)

    def write_recursive(self, value, file):
        if isinstance(value, dict):
            for k,v in value.items():
                self.write_recursive(v, file)
        elif isinstance(value, list):
            for i in value:
                self.write_recursive(i, file)
        elif isinstance(value, XCodeNode):
            value.write(file)

    def write(self, file):
        for attribute,value in self.__dict__.items():
            if attribute[0] != '_':
                self.write_recursive(value, file)

        w = file.write
        w("\t%s = {\n" % self._id)
        w("\t\tisa = %s;\n" % self.__class__.__name__)
        for attribute,value in self.__dict__.items():
            if attribute[0] != '_':
                w("\t\t%s = %s;\n" % (attribute, self.tostring(value)))
        w("\t};\n\n")



# Configurations
class XCBuildConfiguration(XCodeNode):
    def __init__(self, name, settings = {}):
        XCodeNode.__init__(self)
        self.baseConfigurationReference = ""
        self.buildSettings = settings
        self.name = name


class XCConfigurationList(XCodeNode):
    def __init__(self, settings):
        XCodeNode.__init__(self)
        self.buildConfigurations = settings
        self.defaultConfigurationIsVisible = 0
        self.defaultConfigurationName = settings and settings[0].name or ""

# Group/Files
class PBXFileReference(XCodeNode):
    def __init__(self, name, path, filetype = '', sourcetree = "SOURCE_ROOT"):
        XCodeNode.__init__(self)
        self.fileEncoding = 4
        if not filetype:
            _, ext = os.path.splitext(name)
            filetype = MAP_EXT.get(ext, 'text')
        self.lastKnownFileType = filetype
        self.name = name
        self.path = path
        self.sourceTree = sourcetree

class PBXGroup(XCodeNode):
    def __init__(self, name, sourcetree = "<group>"):
        XCodeNode.__init__(self)
        self.children = []
        self.name = name
        self.sourceTree = sourcetree

    def sort_recursive(self):
        self.children.sort(key=lambda child: [not isinstance(child, PBXGroup), child.name])
        for child in self.children:
            if isinstance(child, PBXGroup):
                child.sort_recursive()

    def add(self, root, sources):
        global do_debug_print
        folders = {}
        def folder(node):
            if do_debug_print:
                print(node)
                print(node.name)
                print(node.parent)
            if node == root:
                return self
            if node == None:
                return self
            try:
                return folders[node]
            except KeyError:
                new_folder = PBXGroup(node.name)
                parent = folder(node.parent)
                folders[node] = new_folder
                parent.children.append(new_folder)
                return new_folder

        for s in sources:
            if do_debug_print:
                print('========')
                print(s)
            a_folder = folder(s.parent)
            source = PBXFileReference(s.name, s.abspath())
            a_folder.children.append(source)

    def get_child_file_references_recursive(self, file_references):
        for child in self.children:
            if isinstance(child, PBXGroup):
                child.get_child_file_references_recursive(file_references)
            elif isinstance(child, PBXFileReference):
                file_references.append(child)

# Targets
class PBXLegacyTarget(XCodeNode):
    def __init__(self, platform_name, project_spec, target, ctx):
        XCodeNode.__init__(self)
        global root_dir

        build_configuration_list = []
        for config in ctx.get_target_configurations():
            build_configuration_list.append(XCBuildConfiguration(config))
        self.buildConfigurationList = XCConfigurationList(build_configuration_list)

        action = 'build_' + platform_name + "_$CONFIGURATION"
        self.buildArgumentsString = "%s/%s -cwd %s %s -p%s --targets=%s" % (XCodeEscapeSpacesForShell(root_dir), XCodeEscapeSpacesForShell(sys.argv[0]), XCodeEscapeSpacesForShell(root_dir), action, project_spec, target)
        self.buildPhases = []
        self.buildToolPath = sys.executable
        self.buildWorkingDirectory = ""
        self.dependencies = []
        self.name = target or action
        self.productName = target or action
        self.passBuildSettingsInEnvironment = 0

class PBXShellScriptBuildPhase(XCodeNode):
    def __init__(self, action):
        XCodeNode.__init__(self)
        global root_dir
        self.buildActionMask = uintmax # default when adding a build phase manually from Xcode
        self.files = []
        self.inputPaths = []
        self.outputPaths = []
        self.runOnlyForDeploymentPostProcessing = 0
        self.shellPath = "/bin/sh"
        self.shellScript = "'%s' '%s/%s' -cwd '%s' %s" % (sys.executable, XCodeEscapeSpacesForShell(root_dir), XCodeEscapeSpacesForShell(sys.argv[0]), XCodeEscapeSpacesForShell(root_dir), action)

class PBXSourcesBuildPhase(XCodeNode):
    def __init__(self, files):
        XCodeNode.__init__(self)
        self.buildActionMask = uintmax # default when adding a build phase manually from Xcode
        self.files = []
        for file in files:
            self.files.append(PBXBuildFile(file)) 
        self.runOnlyForDeploymentPostprocessing = 0

class PBXResourcesBuildPhase(XCodeNode):
    def __init__(self, files):
        XCodeNode.__init__(self)
        self.buildActionMask = uintmax # default when adding a build phase manually from Xcode
        self.files = []
        for file in files:
            self.files.append(PBXBuildFile(file)) 
        self.runOnlyForDeploymentPostprocessing = 0


class PBXBuildFile(XCodeNode):
    def __init__(self, file_reference):
        XCodeNode.__init__(self)
        self.fileRef = file_reference._id
        self.settings = { 'COMPILER_FLAGS' : "" }


class PBXNativeTarget(XCodeNode):
    def __init__(self, platform_name, project_spec, task_generator, settings, ctx):
        XCodeNode.__init__(self)

        if (task_generator == None):
            return

        target = task_generator.name
        node = task_generator.link_task.outputs[0].change_ext('.app')
        env =  task_generator.env

        project_name = None
        for name in ctx.get_enabled_game_project_list():
            if target in ctx.project_and_platform_modules(name, platform_name):
                project_name = name
                break

        target_settings = copy.copy(settings)
        target_settings['PRODUCT_NAME'] = ctx.get_executable_name(project_name)
        target_settings['INFOPLIST_FILE'] = task_generator.to_nodes(getattr(task_generator, 'mac_plist', [''])[0])[0].abspath()
        target_settings['ASSETCATALOG_COMPILER_APPICON_NAME'] = ctx.get_app_icon(target)
        target_settings['ASSETCATALOG_COMPILER_LAUNCHIMAGE_NAME'] = ctx.get_launch_image(target)

        build_configuration_list = []
        for config in ctx.get_target_configurations():

            per_configuartion_build_settings = copy.copy(target_settings)
            per_configuartion_build_settings['CONFIGURATION_BUILD_DIR'] = ctx.get_output_folders(platform_name, config)[0].abspath()
            if env and env.ARCH:
                per_configuartion_build_settings['ARCHS'] = " ".join(env.ARCH)

            build_configuration_list.append(XCBuildConfiguration(config, per_configuartion_build_settings))
        self.buildConfigurationList = XCConfigurationList(build_configuration_list)

        build_action = 'build_' + platform_name + "_$CONFIGURATION" + ' -p ' + project_spec
        self.buildPhases = [PBXShellScriptBuildPhase(build_action)]
        self.buildRules = []
        self.dependencies = []
        self.name = target
        self.productName = task_generator.output_file_name
        self.productType = "com.apple.product-type.application"
        self.productReference = PBXFileReference(target, node.abspath(), 'wrapper.application', 'BUILT_PRODUCTS_DIR')

    def add_resources_to_target(self, files):
        self.buildPhases.append(PBXResourcesBuildPhase(files))

    def add_rsync_rsources_to_target(self, dev_assets_folder_ref, release_assets_folder_ref, target_assets_subdir):
        rsync_build_phase = PBXShellScriptBuildPhase('')
        clean_dev_folder_path               = '\\"' + XCodeEscapeSpacesForShell(dev_assets_folder_ref.path.rstrip('/')) + '/\\"'
        clean_dev_bootstrap_file_path       = '\\"' + XCodeEscapeSpacesForShell(dev_assets_folder_ref.path.rstrip('/')) + '/bootstrap.cfg\\"'
        clean_dev_config_file_path          = '\\"' + XCodeEscapeSpacesForShell(dev_assets_folder_ref.path.rstrip('/')) + '/' + self.name + '/config/game.xml\\"'
        clean_release_folder_path           = '\\"' + XCodeEscapeSpacesForShell(release_assets_folder_ref.path.rstrip('/')) + '/\\"'
        clean_target_folder_path            = '\\"$TARGET_BUILD_DIR/$WRAPPER_NAME/' + target_assets_subdir + '/\\"'
        clean_target_bootstrap_file_path    = '\\"$TARGET_BUILD_DIR/$WRAPPER_NAME/' + target_assets_subdir + '/bootstrap.cfg\\"'
        exclude_and_filter_options          = '--delete-excluded --exclude .DS_Store --exclude CVS --exclude .svn --exclude .git --exclude .hg --exclude assetcatalog.xml.tmp --filter \\"P /user/\\"'
        clean_target_config_folder          = '\\"$TARGET_BUILD_DIR/$WRAPPER_NAME/' + target_assets_subdir + '/' + self.name.lower() + '/config/\\"'
        clean_target_config_file_path       = '\\"$TARGET_BUILD_DIR/$WRAPPER_NAME/' + target_assets_subdir + '/' + self.name.lower() + '/config/game.xml\\"'

        # For release builds we need to use the asset paks, but for development builds
        # we want to use the loose assets (although the asset paks can be used instead).
        # Additionally, if we're running in Virtual File System (VFS) mode, we only want
        # to copy the bootstrap.cfg file
        rsync_build_phase.shellScript  = \
        'if [ \\"${CONFIGURATION}\\" = \\"release\\" ]; then\\n' \
        '  rsync -rtuvk ' + exclude_and_filter_options + ' ' + clean_release_folder_path + ' ' + clean_target_folder_path + '\\n' \
        'else\\n' \
        '  while read line; do\\n' \
        '    if [[ $line == remote_filesystem* ]]; then\\n' \
        '      eval $line\\n' \
        '      remote_filesystem=\\"$(echo -e \\"${remote_filesystem}\\" | tr -d \\\'[[:space:]]\\\')\\"\\n' \
        '    fi\\n' \
        '  done < ' + clean_dev_bootstrap_file_path + '\\n' \
        '  if [ \\"$remote_filesystem\\" -eq 1 ]; then\\n' \
        '    rm -rf ' + clean_target_folder_path + '\\n' \
        '    mkdir -p ' + clean_target_folder_path + '\\n' \
        '    mkdir -p ' + clean_target_config_folder + '\\n' \
        '    rsync -rtuvk ' + clean_dev_bootstrap_file_path + ' ' + clean_target_bootstrap_file_path + '\\n' \
        '    rsync -rtuvk ' + clean_dev_config_file_path + ' ' + clean_target_config_file_path + '\\n' \
        '  else\\n' \
        '    mkdir -p ' + clean_target_folder_path + '\\n' \
        '    rsync -rtuvk ' + exclude_and_filter_options + ' ' + clean_dev_folder_path + ' ' + clean_target_folder_path + '\\n' \
        '  fi\\n' \
        'fi'

        self.buildPhases.append(rsync_build_phase)

    def add_remove_embedded_provisioning_to_target(self):
        '''Remove the embedded provisioning file before each deploy to force xcode to sign the application.
           There are times when the executable is copied over but xcode does not sign it.
           When xcode then deploys the app it fails because the executable was not signed.
           Deleting the embeded manifest file forces xcode to regenerate it and sign the executable.'''
        remove_embedded_provisioning_build_phase = PBXShellScriptBuildPhase('')
        remove_embedded_provisioning_build_phase.shellScript = 'rm -f \\"$TARGET_BUILD_DIR/$WRAPPER_NAME/embedded.mobileprovision\\"'
        self.buildPhases.append(remove_embedded_provisioning_build_phase)


# Root project object
class PBXProject(XCodeNode):
    def __init__(self, name, version, ctx):
        XCodeNode.__init__(self)

        build_configuration_list = []
        for config in ctx.get_target_configurations():
            build_configuration_list.append(XCBuildConfiguration(config))
        self.buildConfigurationList = XCConfigurationList(build_configuration_list)
        self.compatibilityVersion = version[0]
        self.hasScannedForEncodings = 1;
        self.mainGroup = PBXGroup(name)
        self.projectRoot = ""
        self.projectDirPath = ""
        self.project_spec = 'all'
        self.platform_name = 'darwin_x64'
        self.config = 'debug'
        self.settings = {}
        self.targets = []
        self._objectVersion = version[1]
        self._output = PBXGroup('out')
        self.mainGroup.children.append(self._output)

    def set_project_spec(self, project_spec):
        if (project_spec):
            self.project_spec = project_spec

    def set_platform_name(self, platform):
        self.platform_name = platform

    def set_config(self, config):
        self.config = config

    def set_settings(self, settings):
        self.settings = settings

    def write(self, file):
        w = file.write
        w("// !$*UTF8*$!\n")
        w("{\n")
        w("\tarchiveVersion = 1;\n")
        w("\tclasses = {\n")
        w("\t};\n")
        w("\tobjectVersion = %d;\n" % self._objectVersion)
        w("\tobjects = {\n\n")

        XCodeNode.write(self, file)

        w("\t};\n")
        w("\trootObject = %s;\n" % self._id)
        w("}\n")

    def add_task_gen(self, task_generator, ctx):
        if not getattr(task_generator, 'mac_app', False) and not getattr(task_generator, 'ios_app', False) and not getattr(task_generator, 'appletv_app', False):
            self.targets.append(PBXLegacyTarget(self.platform_name, self.project_spec, task_generator.name, ctx))
        else:
            target = PBXNativeTarget(self.platform_name, self.project_spec, task_generator, self.settings, ctx)
            self.targets.append(target)
            self._output.children.append(target.productReference)

class xcode(Build.BuildContext):

    def get_settings(self):
        settings = {}
        return settings

    def collect_source(self, task_generator):
        source_files = task_generator.to_nodes(getattr(task_generator, 'source', []))
        plist_files = task_generator.to_nodes(getattr(task_generator, 'mac_plist', []))

        source = list(set(source_files + plist_files))
        return source

    def generate_project(self):
        global root_dir

        self.restore()
        if not self.all_envs:
            self.load_envs()
        self.load_user_settings()
        self.recurse([self.run_dir])

        root_dir = Context.launch_dir
        xcode_project_name = self.get_xcode_project_name()
        if not xcode_project_name:
            xcode_project_name = getattr(Context.g_module, Context.APPNAME, 'project')
        project = PBXProject(xcode_project_name, ('Xcode 3.2', 46), self)
        project.set_project_spec(self.options.project_spec)

        platform_name = self.get_target_platform_name()
        project.set_platform_name(platform_name)
        project.set_settings(self.get_settings())

        spec_modules = self.spec_modules(project.project_spec)
        project_modules = [];
        for project_name in self.get_enabled_game_project_list():
            project_modules = project_modules + self.project_and_platform_modules(project_name, platform_name)

        source_files = []
        for group in self.groups:
            for task_generator in group:
                if not isinstance(task_generator, TaskGen.task_gen):
                    continue

                if task_generator.target not in spec_modules and task_generator.target not in project_modules:
                    Logs.debug('xcode: not adding module to project %s' % task_generator.target)
                    continue

                task_generator.post()

                features = Utils.to_list(getattr(task_generator, 'features', ''))

                source_files = list(set(source_files + self.collect_source(task_generator)))

                if 'cprogram' or 'cxxprogram' in features:
                    project.add_task_gen(task_generator, self)

        project.mainGroup.add(self.srcnode, source_files)
        project.targets.sort(key=lambda target: [isinstance(target, PBXLegacyTarget), target.name])

        # Create a dummy target that builds all source files so Xcode find file/symbol functionality works
        source_file_references = []
        project.mainGroup.get_child_file_references_recursive(source_file_references)
        dummy_target = PBXNativeTarget(project.platform_name, project.project_spec, None, project.settings, self)
        dummy_target.buildPhases = [PBXSourcesBuildPhase(source_file_references)]
        dummy_target.buildRules = []
        dummy_target.dependencies = []
        dummy_target.name = "DummyTargetForSymbols"
        dummy_target.productName = "DummyTargetForSymbols"
        dummy_target.productType = "com.apple.product-type.tool"
        dummy_target.productReference = PBXFileReference("DummyTargetForSymbols", "DummyTargetForSymbols", 'compiled.mach-o.executable', 'BUILT_PRODUCTS_DIR')
        project._output.children.append(dummy_target.productReference)
        project.targets.append(dummy_target)

        # Create resource group/folder structure and attach it to the native
        # projects
        resource_group = PBXGroup("Resources")
        project.mainGroup.children.append(resource_group)

        root_assets_folder = self.srcnode.make_node("Cache")

        for game_project in self.get_enabled_game_project_list():
            game_resources_group = PBXGroup(game_project)
            resource_group.children.append(game_resources_group)

            dev_assets_folder = root_assets_folder.make_node(game_project).make_node(self.get_dev_source_assets_subdir())
            dev_assets_folder_ref = PBXFileReference('assets_dev', dev_assets_folder.abspath(), 'folder')
            game_resources_group.children.append(dev_assets_folder_ref)

            release_assets_folder = root_assets_folder.make_node(game_project).make_node(self.get_release_source_assets_subdir())
            release_assets_folder_ref = PBXFileReference('assets_release', release_assets_folder.abspath(), 'folder')
            game_resources_group.children.append(release_assets_folder_ref)

            xcode_assets_folder = self.launch_node().make_node(self.game_code_folder(game_project) + self.get_xcode_source_assets_subdir())
            xcode_assets_folder_ref = PBXFileReference('xcassets', xcode_assets_folder.abspath(), 'folder.assetcatalog')
            game_resources_group.children.append(xcode_assets_folder_ref)

            for target in project.targets:
                if isinstance(target, PBXNativeTarget) and target.name == game_project:
                    target.add_remove_embedded_provisioning_to_target()
                    target.add_rsync_rsources_to_target(dev_assets_folder_ref, release_assets_folder_ref, self.get_target_assets_subdir())
                    target.add_resources_to_target([xcode_assets_folder_ref])

        project.mainGroup.sort_recursive()

        projectDir = self.srcnode.make_node("/%s.xcodeproj" % xcode_project_name)
            
        projectDir.mkdir()
        node = projectDir.make_node('project.pbxproj')
        project.write(open(node.abspath(), 'w'))


class xcode_mac(xcode):
    '''Generate an xcode project for Mac'''
    cmd = 'xcode_mac'

    def get_target_platforms(self):
        return ['darwin', 'darwin_x64']

    def get_target_platform_name(self):
        return 'darwin_x64'

    def get_target_configurations(self):
        return self.get_supported_configurations()

    def get_xcode_project_name(self):
        return self.get_mac_project_name()

    def get_dev_source_assets_subdir(self):
        return '/osx_gl'

    def get_release_source_assets_subdir(self):
        return '/osx_gl_paks'

    def get_target_assets_subdir(self):
        return 'Contents/Resources/assets'

    def get_xcode_source_assets_subdir(self):
        return '/Resources/MacLauncher/Images.xcassets'

    def get_app_icon(self, project):
        return self.get_mac_app_icon(project)

    def get_launch_image(self, project):
        return self.get_mac_launch_image(project)

    def execute(self):
        self.generate_project()


class xcode_ios(xcode):
    '''Generate an xcode project for iOS'''
    cmd = 'xcode_ios'

    def get_target_platforms(self):
            return [ 'ios' ]

    def get_target_platform_name(self):
        return 'ios'

    def get_target_configurations(self):
        supported_configurations = []
        for config in self.get_supported_configurations():
            if not ('_dedicated' in config):
                supported_configurations.append(config)
        return supported_configurations

    def get_settings(self):
        settings = xcode.get_settings(self)
        settings_file = self.root.find_node(Context.launch_dir).find_node('/_WAF_/ios/iossettings.json')
        if settings_file != None:
            settings.update(self.parse_json_file(settings_file))
            if self.options.generate_debug_info:
                settings['GCC_GENERATE_DEBUGGING_SYMBOLS'] = True
            else:
                settings['GCC_OPTIMIZATION_LEVEL'] = 3

        else:
            Logs.warn('xcode: Could not find a iossettings.json file in %s. Project will not have all necessary options set correctly' % (Context.launch_dir + '/_WAF_/ios/'))

        # For symroot need to have the full path so change the value from
        # relative path to a full path
        if 'SYMROOT' in settings:
            settings['SYMROOT'] = Context.launch_dir + "/" + settings['SYMROOT']

        return settings
    
    def get_xcode_project_name(self):
        return self.get_ios_project_name()

    def get_dev_source_assets_subdir(self):
        return '/ios'

    def get_release_source_assets_subdir(self):
        return '/ios_paks'

    def get_target_assets_subdir(self):
        return 'assets'

    def get_xcode_source_assets_subdir(self):
        return '/Resources/IOSLauncher/Images.xcassets'

    def get_app_icon(self, project):
        return self.get_ios_app_icon(project)

    def get_launch_image(self, project):
        return self.get_ios_launch_image(project)

    def execute(self):
        self.generate_project()


class xcode_appletv(xcode):
    '''Generate an xcode project for apple tv'''
    cmd = 'xcode_appletv'

    def get_target_platforms(self):
            return [ 'appletv' ]

    def get_target_platform_name(self):
        return 'appletv'

    def get_target_configurations(self):
        supported_configurations = []
        for config in self.get_supported_configurations():
            if not ('_dedicated' in config):
                supported_configurations.append(config)
        return supported_configurations

    def get_settings(self):
        settings = xcode.get_settings(self)
        settings_file = self.root.find_node(Context.launch_dir).find_node('/_WAF_/appletv/appletvsettings.json')
        if settings_file != None:
            settings.update(self.parse_json_file(settings_file))
            if self.options.generate_debug_info:
                settings['GCC_GENERATE_DEBUGGING_SYMBOLS'] = True
            else:
                settings['GCC_OPTIMIZATION_LEVEL'] = 3

        else:
            Logs.warn('xcode: Could not find a appletvsettings.json file in %s. Project will not have all necessary options set correctly' % (Context.launch_dir + '/_WAF_/appletv/'))

        # For symroot need to have the full path so change the value from
        # relative path to a full path
        if 'SYMROOT' in settings:
            settings['SYMROOT'] = Context.launch_dir + "/" + settings['SYMROOT']

        return settings
    
    def get_xcode_project_name(self):
        return self.get_appletv_project_name()

    def get_dev_source_assets_subdir(self):
        return '/ios'

    def get_release_source_assets_subdir(self):
        return '/ios_paks'

    def get_target_assets_subdir(self):
        return 'assets'

    def get_xcode_source_assets_subdir(self):
        return '/Resources/AppleTVLauncher/Images.xcassets'

    def get_app_icon(self, project):
        return self.get_appletv_app_icon(project)

    def get_launch_image(self, project):
        return self.get_appletv_launch_image(project)

    def execute(self):
        self.generate_project()

