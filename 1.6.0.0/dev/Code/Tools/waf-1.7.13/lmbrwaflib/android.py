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
#  Android builder
"""
Usage (in wscript):

def options(opt):
    opt.load('android')
"""

import os, sys, random, time, re, stat, string
import atexit, shutil, threading, collections
import imghdr
import cry_utils

from subprocess import call, check_output

from waflib import Context, TaskGen, Build, Utils, Node, Logs, Options
from waflib.Build import POST_LAZY, POST_AT_ONCE
from waflib.Configure import conf
from waflib.Task import Task, ASK_LATER, RUN_ME, SKIP_ME
from waflib.TaskGen import feature, extension, before, before_method, after, after_method, taskgen_method


################################################################
#                     Defaults                                 #
BUILDER_DIR = 'Code/Launcher/AndroidLauncher/ProjectBuilder'
BUILDER_FILES = 'android_builder.json'

RESOLUTION_MESSAGE = 'Please re-run Setup Assistant with "Compile For Android" enabled and run the configure command again.'

RESOLUTION_SETTINGS = ( 'mdpi', 'hdpi', 'xhdpi', 'xxhdpi', 'xxxhdpi' )

# these are the default names for application icons and splash images
APP_ICON_NAME = 'app_icon.png'
APP_SPLASH_NAME = 'app_splash.png'

# supported version : ndk re-direct version
SUPPORTED_APIS = {
    'android-19' : 'android-19',
    'android-21' : 'android-21',
    'android-22' : 'android-21',
    'android-23' : 'android-23',
    'android-24' : 'android-24'
}
#                                                              #
################################################################


################################################################
"""
The os.symlink function does not work on windows machines.  So we
will need to create our own symlink function and overwrite the existing
os.symlink with the custom one on windows platforms.
NOTE: For this to work on windows, the command prompt needs to be
run under administrator.
"""
if Utils.unversioned_sys_platform() == "win32":
    def symlink_ms(source, link_name):
        import ctypes
        csl = ctypes.windll.kernel32.CreateSymbolicLinkW
        csl.argtypes = (ctypes.c_wchar_p, ctypes.c_wchar_p, ctypes.c_uint32)
        csl.restype = ctypes.c_ubyte
        flags = 1 if os.path.isdir(source) else 0
        try:
            if csl(link_name, source.replace('/', '\\'), flags) == 0:
                raise ctypes.WinError()
        except:
            pass
    os.symlink = symlink_ms

################################################################
@feature('cshlib', 'cxxshlib')
@after_method('apply_link')
def apply_so_name(self):
    """
    Adds the linker flag to set the DT_SONAME in ELF shared objects. The
    name used here will be used instead of the file name when the dynamic
    linker attempts to load the shared object
    """

    if 'android' in self.bld.env['PLATFORM'] and self.env.SONAME_ST:
        flag = self.env.SONAME_ST % self.link_task.outputs[0]
        self.env.append_value('LINKFLAGS', flag.split())

################################################################
def remove_file_and_empty_directory(directory, file):
    """
    Helper function for deleting a file and directory, if empty
    """

    file_path = os.path.join(directory, file)

    # first delete the file, if it exists
    if os.path.exists(file_path):
        os.remove(file_path)

    # then remove the directory, if it exists and is empty
    if os.path.exists(directory) and not os.listdir(directory):
        os.rmdir(directory)


################################################################
def construct_source_path(conf, project, source_path):
    """
    Helper to construct the source path to an asset override such as
    application icons or splash screen images
    """
    if os.path.isabs(source_path):
        path_node = conf.root.make_node(source_path)
    else:
        relative_path = os.path.join('Code', project, 'Resources', source_path)
        path_node = conf.path.make_node(relative_path)
    return path_node.abspath()


################################################################
def clear_splash_assets(project_node, path_prefix):

    target_directory = project_node.make_node(path_prefix)
    remove_file_and_empty_directory(target_directory.abspath(), APP_SPLASH_NAME)

    for resolution in RESOLUTION_SETTINGS:
        # The xxxhdpi resolution is only for application icons, its overkill to include them for drawables... for now
        if resolution == 'xxxhdpi':
            continue

        target_directory = project_node.make_node(path_prefix + '-' + resolution)
        remove_file_and_empty_directory(target_directory.abspath(), APP_SPLASH_NAME)


################################################################
def get_android_sdk_home(ctx):

    # search for the env file
    android_home = ctx.get_env_file_var('LY_ANDROID_SDK', required = True)
    if android_home == '':
        Logs.error('[ERROR] %s' % RESOLUTION_MESSAGE)

    return android_home


################################################################
def options(opt):
    group = opt.add_option_group('android-specific config')

    group.add_option('--android-toolchain', dest = 'android_toolchain', action = 'store', default = '', help = 'DEPRECATED: Android toolchain to use for building, valid options are gcc or clang')

    group.add_option('--dev-store-pass', dest = 'dev_store_pass', action = 'store', default = 'Lumberyard', help = 'The store password for the development keystore')
    group.add_option('--dev-key-pass', dest = 'dev_key_pass', action = 'store', default = 'Lumberyard', help = 'The key password for the development keystore')

    group.add_option('--distro-store-pass', dest = 'distro_store_pass', action = 'store', default = '', help = 'The store password for the distribution keystore')
    group.add_option('--distro-key-pass', dest = 'distro_key_pass', action = 'store', default = '', help = 'The key password for the distribution keystore')
    group.add_option('--android-apk-path', dest = 'apk_path', action = 'store', default = '', help = 'Path to apk to deploy. If not specified the default build path will be used')


################################################################
@conf
def validate_android_api_version(conf):
    sdk_version = conf.get_android_sdk_version()
    if sdk_version in SUPPORTED_APIS:
        return True

    api_list = ', '.join(sorted(SUPPORTED_APIS.keys()))
    Logs.error('[ERROR] Android SDK version - %s - is unsupported.  Please change SDK_VERSION in _WAF_/android/android_setting.json to a supported API and run the configure command again.\n'
                '\t-> Supported APIs are: %s' % (sdk_version, api_list))
    return False


################################################################
@conf
def validate_android_ndk_install(conf):

    # search for the env file
    ndk_root = conf.get_env_file_var('LY_ANDROID_NDK', required = True)
    if ndk_root == '':
        Logs.error('[ERROR] %s' % RESOLUTION_MESSAGE)
        return False

    conf.env['NDK_ROOT'] = ndk_root
    return True


################################################################
@conf
def get_corrected_android_ndk_platform_includes(conf, ndk_root, arch):
    sdk_version = conf.get_android_sdk_version()
    return os.path.join(ndk_root, 'platforms', SUPPORTED_APIS[sdk_version], arch)


################################################################
@conf
def load_android_toolchains(conf, search_paths, android_cc, android_cxx, android_linkcc, android_linkcxx, android_ar, android_strip):
    """
    Helper function for loading all the android toolchains
    """
    try:
        conf.find_program(android_cc, var = 'CC', path_list = search_paths, silent_output = True)

        # common cc settings
        conf.cc_load_tools()
        conf.cc_add_flags()
        conf.link_add_flags()

        conf.find_program(android_cxx, var = 'CXX', path_list = search_paths, silent_output = True)

        # common cxx settings
        conf.cxx_load_tools()
        conf.cxx_add_flags()
        conf.link_add_flags()

        conf.find_program(android_linkcc, var = 'LINK_CC', path_list = search_paths, silent_output = True)
        conf.find_program(android_linkcxx, var = 'LINK_CXX', path_list = search_paths, silent_output = True)
        conf.env['LINK'] = conf.env['LINK_CC']

        conf.find_program(android_ar, var = 'AR', path_list = search_paths, silent_output = True)

        # for debug symbol stripping
        conf.find_program(android_strip, var = 'STRIP', path_list = search_paths, silent_output = True)
    except:
        Logs.error('[ERROR] Failed to find the Android NDK standalone toolchain(s) in search path %s' % search_paths)
        return False

    return True


################################################################
@conf
def load_android_tools(conf):
    """
    This function is intended to be called in all android compiler setttings/rules
    so the android sdk build tools are part of the environment
    """

    android_home = get_android_sdk_home(conf)
    if android_home == '':
        return False

    conf.env['ANDROID_HOME'] = android_home

    build_tools_dir = os.path.join(android_home, 'build-tools')
    build_tools = os.path.join(build_tools_dir, conf.get_android_build_tools_version())

    platforms_dir = os.path.join(android_home, 'platforms')
    platform = os.path.join(platforms_dir, conf.get_android_sdk_version())

    # do you have the correct version of the SDK (for example, android-21)
    if conf.find_file('android.jar', path_list = [ platform ], mandatory = False) is None:
        Logs.error('[ERROR] Failed to find Android SDK version - %s - in path %s.  Please use Android SDK Manager to download the appropriate SDK version and run the configure command again.' % (conf.get_android_sdk_version(), platforms_dir))
        return False

    try:
        conf.find_program('aapt', var = 'AAPT', path_list = [ build_tools ], silent_output = True)
        conf.find_program('dx', var = 'DX', path_list = [ build_tools ], silent_output = True)
        conf.find_program('zipalign', var = 'ZIPALIGN', path_list = [ build_tools ], silent_output = True)
        conf.find_program('aidl', var = 'AIDL', path_list = [ build_tools ], silent_output = True)
    except:
        Logs.error('[ERROR] Failed to find Android SDK build-tools version - %s - in path %s.\n'
                    '\tPerform one of the following and run the configure command again\n'
                    '\t1. Install this version from the Android SDK Manager\n'
                    '\t2. Change BUILD_TOOLS_VER in _WAF_/android/android_settings.json to a version you have' % (conf.get_android_build_tools_version(), build_tools_dir))
        return False

    # check for the required java tools
    jdk_home = conf.get_env_file_var('LY_JDK', required = True)
    if jdk_home == '':
        Logs.error('[ERROR] %s' % RESOLUTION_MESSAGE)
        return False

    jdk_bin = os.path.join(jdk_home, 'bin')
    try:
        conf.find_program('javac', var = 'JAVA', path_list = [ jdk_bin ], silent_output = True)
        conf.find_program('jarsigner', var = 'JARSIGN', path_list = [ jdk_bin ], silent_output = True)
    except:
        Logs.error('[ERROR] Unable to find Java tools in path %s. %s' % (jdk_bin, RESOLUTION_MESSAGE))
        return False

    return True


################################################################
def inject_auto_gen_header(writer):
    writer('################################################################\n')
    writer('# This file was automatically created by WAF\n')
    writer('# WARNING! All modifications will be lost!\n')
    writer('################################################################\n\n')


################################################################
def replace_macros(text, macros):
    for key, value in macros.iteritems():
        regex = re.compile('%%%' + key + '%%%', re.MULTILINE)
        text = regex.sub(value, text)
    return text


################################################################
def process_json(conf, json_data, curr_node, root_node, template):
    for elem in json_data:

        if elem == 'NO_OP':
            continue

        if os.path.isabs(elem):
            source_curr = conf.root.make_node(elem)
        else:
            source_curr = root_node.make_node(elem)

        target_curr = curr_node.make_node(elem)

        if isinstance(json_data, dict):
            # resolve name overrides for the copy, if specified
            if isinstance(json_data[elem], unicode) or isinstance(json_data[elem], str):
                target_curr = curr_node.make_node(json_data[elem])

            # otherwise continue processing the tree
            else:
                target_curr.mkdir()
                process_json(conf, json_data[elem], target_curr, root_node, template)
                continue

        # leaf handing
        if imghdr.what(source_curr.abspath()) in ( 'rgb', 'gif', 'pbm', 'ppm', 'tiff', 'rast', 'xbm', 'jpeg', 'bmp', 'png' ):
            shutil.copyfile(source_curr.abspath(), target_curr.abspath())
        else:
            target_curr.write(replace_macros(source_curr.read(), template))


################################################################
@conf
def create_and_add_android_launchers_to_build(conf):
    """
    This function will generate the bare minimum android project
    and include the new android launcher(s) in the build path.
    So no Android Studio gradle files will be generated.
    """
    if not get_android_sdk_home(conf):
        return False

    android_root = conf.path.make_node(conf.get_android_project_relative_path())
    android_root.mkdir()

    source_node = conf.path.make_node(BUILDER_DIR)
    builder_file_src = source_node.make_node(BUILDER_FILES)
    builder_file_dest = conf.path.get_bld().make_node(BUILDER_DIR)

    if not os.path.exists(builder_file_src.abspath()):
        conf.fatal('[ERROR] Failed to find the Android project builder - %s - in path %s.  Verify file exists and run the configure command again.' % (BUILDER_FILES, BUILDER_DIR))
        return False

    project_subdir = 'src'
    created_directories = []
    for project in conf.get_enabled_game_project_list():
        # make sure the project has android options specified
        if conf.get_android_settings(project) == None:
            Logs.warn('[WARN] Android settings not found in %s/project.json, skipping.' % project)
            continue

        proj_root = android_root.make_node(conf.get_executable_name(project))

        # windows gets mad if you try to remove a non-empty directory
        # tree, so we have to ignore errors so it will still remove it
        if os.path.exists(proj_root.abspath()):
            shutil.rmtree(proj_root.abspath(), ignore_errors = True)
        proj_root.mkdir()

        proj_node = proj_root.make_node(project_subdir)
        created_directories.append(proj_node.path_from(android_root))

        # setup the macro replacement map for the builder files
        activity_name = '%sActivity' % project
        transformed_package = conf.get_android_package_name(project).replace('.', '/')

        template = {
            'ANDROID_PACKAGE' : conf.get_android_package_name(project),
            'ANDROID_PACKAGE_PATH' : transformed_package,
            'ANDROID_APP_NAME' : conf.get_launcher_product_name(project),    # external facing name
            'ANDROID_PROJECT_NAME' : project,                                # internal facing name (library/asset dir name)
            'ANDROID_PROJECT_ACTIVITY' : activity_name,
            'ANDROID_LAUNCHER_NAME' : conf.get_executable_name(project),     # first native library to load from java
            'ANDROID_VERSION_NUMBER' : conf.get_android_version_number(project),
            'ANDROID_VERSION_NAME' : conf.get_android_version_name(project),
            'ANDROID_SCREEN_ORIENTATION' : conf.get_android_orientation(project),
            'ANDROID_APP_PUBLIC_KEY' : conf.get_android_app_public_key(project),
        }

        # update the builder file with the correct pacakge name
        transformed_node = builder_file_dest.find_or_declare('%s_builder.json' % project)
        transformed_node.write(replace_macros(builder_file_src.read(), template))

        # process the builder file and create project
        json_data = conf.parse_json_file(transformed_node)
        process_json(conf, json_data, proj_node, source_node, template)

        # resolve the application icon overrides
        icon_overrides = conf.get_android_app_icons(project)
        if icon_overrides is not None:
            mipmap_path_prefix = 'res/mipmap'

            # if a default icon is specified, then copy it into the generic mipmap folder
            default_icon = icon_overrides.get('default', None)

            if default_icon is not None:
                default_icon_source_node = construct_source_path(conf, project, default_icon)

                default_icon_target_dir = proj_node.make_node(mipmap_path_prefix)
                default_icon_target_dir.mkdir()

                shutil.copyfile(default_icon_source_node, os.path.join(default_icon_target_dir.abspath(), APP_ICON_NAME))
            else:
                Logs.debug('android: No default icon override specified for %s' % project)

            # process each of the resolution overrides
            for resolution in RESOLUTION_SETTINGS:
                target_directory = proj_node.make_node(mipmap_path_prefix + '-' + resolution)

                # get the current resolution icon override
                icon_source = icon_overrides.get(resolution, default_icon)
                if icon_source is default_icon:

                    # if both the resolution and the default are unspecified, warn the user but do nothing
                    if icon_source is None:
                        Logs.warn('[WARN] No icon override found for "%s".  Either supply one for "%s" or a "default" in the android_settings "icon" section of the project.json file for %s' % (resolution, resolution, project))

                    # if only the resoultion is unspecified, remove the resolution specific version from the project
                    else:
                        Logs.debug('android: Default icon being used for "%s" in %s' % (resolution, project))
                        remove_file_and_empty_directory(target_directory.abspath(), APP_ICON_NAME)

                    continue

                icon_source_node = construct_source_path(conf, project, icon_source)
                icon_target_node = target_directory.make_node(APP_ICON_NAME)

                shutil.copyfile(icon_source_node, icon_target_node.abspath())

        # resolve the application splash screen overrides
        splash_overrides = conf.get_android_app_splash_screens(project)
        if splash_overrides is not None:
            drawable_path_prefix = 'res/drawable-'

            for orientation in ('land', 'port'):
                orientation_path_prefix = drawable_path_prefix + orientation

                oriented_splashes = splash_overrides.get(orientation, {})

                # if a default splash image is specified for this orientation, then copy it into the generic drawable-<orientation> folder
                default_splash_img = oriented_splashes.get('default', None)

                if default_splash_img is not None:
                    default_splash_img_source_node = construct_source_path(conf, project, default_splash_img)

                    default_splash_img_target_dir = proj_node.make_node(orientation_path_prefix)
                    default_splash_img_target_dir.mkdir()

                    shutil.copyfile(default_splash_img_source_node, os.path.join(default_splash_img_target_dir.abspath(), APP_SPLASH_NAME))
                else:
                    Logs.debug('android: No default splash screen override specified for "%s" orientation in %s' % (orientation, project))

                # process each of the resolution overrides
                for resolution in RESOLUTION_SETTINGS:
                    # The xxxhdpi resolution is only for application icons, its overkill to include them for drawables... for now
                    if resolution == 'xxxhdpi':
                        continue

                    target_directory = proj_node.make_node(orientation_path_prefix + '-' + resolution)

                    # get the current resolution splash image override
                    splash_img_source = oriented_splashes.get(resolution, default_splash_img)
                    if splash_img_source is default_splash_img:

                        # if both the resolution and the default are unspecified, warn the user but do nothing
                        if splash_img_source is None:
                            section = "%s-%s" % (orientation, resolution)
                            Logs.warn('[WARN] No splash screen override found for "%s".  Either supply one for "%s" or a "default" in the android_settings "splash_screen-%s" section of the project.json file for %s' % (section, resolution, orientation, project))

                        # if only the resoultion is unspecified, remove the resolution specific version from the project
                        else:
                            Logs.debug('android: Default icon being used for "%s-%s" in %s' % (orientation, resolution, project))
                            remove_file_and_empty_directory(target_directory.abspath(), APP_SPLASH_NAME)

                        continue

                    splash_img_source_node = construct_source_path(conf, project, splash_img_source)
                    splash_img_target_node = target_directory.make_node(APP_SPLASH_NAME)

                    shutil.copyfile(splash_img_source_node, splash_img_target_node.abspath())

        # additional optimization to only include the splash screens for the avaiable orientations allowed by the manifest
        requested_orientation = conf.get_android_orientation(project)

        if requested_orientation in ('landscape', 'reverseLandscape', 'sensorLandscape', 'userLandscape'):
            Logs.debug('android: Clearing the portrait assets from %s' % project)
            clear_splash_assets(proj_node, 'res/drawable-port')

        elif requested_orientation in ('portrait', 'reversePortrait', 'sensorPortrait', 'userPortrait'):
            Logs.debug('android: Clearing the landscape assets from %s' % project)
            clear_splash_assets(proj_node, 'res/drawable-land')

    # add all the projects to the root wscript
    android_wscript = android_root.make_node('wscript')
    with open(android_wscript.abspath(), 'w') as wscript_file:
        w = wscript_file.write

        inject_auto_gen_header(w)

        w('SUBFOLDERS = [\n')
        w('\t\'%s\'\n]\n\n' % '\',\n\t\''.join(created_directories))

        w('def build(bld):\n')
        w('\tvalid_subdirs = [x for x in SUBFOLDERS if bld.path.find_node("%s/wscript" % x)]\n')
        w('\tbld.recurse(valid_subdirs)\n')

    return True


################################################################
@conf
def is_module_for_game_project(self, module_name, game_project, project_name):
    """
    Check to see if the task generator is part of the build for a particular game project.
    The following rules apply:
        1. It is a gem requested by the game project
        2. It is the game project / project's launcher
        3. It is part of the general modules list
    """
    enabled_game_projects = self.get_enabled_game_project_list()

    if self.is_gem(module_name):
        gem_name_list = [gem.name for gem in self.get_game_gems(game_project)]
        return (True if module_name in gem_name_list else False)

    elif module_name == game_project or game_project == project_name:
        return True

    elif module_name not in enabled_game_projects and project_name is None:
        return True

    return False


################################################################
def collect_source_paths(android_task, src_path_tag):

    proj_name = android_task.project
    bld = android_task.bld

    platform = bld.env['PLATFORM']
    config = bld.env['CONFIGURATION']

    search_tags = [
        'android_{}'.format(src_path_tag),
        'android_{}_{}'.format(config, src_path_tag),

        '{}_{}'.format(platform, src_path_tag),
        'android_{}_{}'.format(platform, config, src_path_tag),
    ]

    sourc_paths = []
    for group in bld.groups:
        for task_generator in group:
            if not isinstance(task_generator, TaskGen.task_gen):
                continue

            Logs.debug('android: Processing task %s' % task_generator.name)

            if not (getattr(task_generator, 'posted', None) and getattr(task_generator, 'link_task', None)):
                Logs.debug('android:  -> Task is NOT posted, Skipping...')
                continue

            project_name = getattr(task_generator, 'project_name', None)
            if not bld.is_module_for_game_project(task_generator.name, proj_name, project_name):
                Logs.debug('android:  -> Task is NOT part of the game project, Skipping...')
                continue

            raw_paths = []
            for tag in search_tags:
                raw_paths += getattr(task_generator, tag, [])

            Logs.debug('android:  -> Raw Source Paths %s' % raw_paths)

            for path in raw_paths:
                if os.path.isabs(path):
                    path = bld.root.make_node(path)
                else:
                    path = task_generator.path.make_node(path)
                sourc_paths.append(path)

    return sourc_paths


################################################################
@conf
def AndroidLauncher(ctx, *k, **kw):

    project_name = kw['project_name']

    if (ctx.cmd == 'configure') or ('android' not in ctx.env['PLATFORM']) or (project_name not in ctx.get_enabled_game_project_list()):
        return

    root_input = ctx.path.get_src()
    root_output = ctx.path.get_bld()

    env = ctx.env

    # copy over the required 3rd party libs that need to be bundled into the apk
    if ctx.options.from_android_studio:
        # The variant name is constructed in the same fashion as how Gradle generates all it's build
        # variants.  After all the Gradle configurations and product flavors are evaluated, the variants
        # are generated in the following lower camel case format {product_flavor}{configuration}.
        # Our configuration and Gradle's configuration is a one to one mapping of what each describe,
        # while our platform is effectively Gradle's product flavor.
        variant_dir = '%s%s' % (ctx.env['PLATFORM'], ctx.env['CONFIGURATION'].title())
        jni_libs_sub_dir = os.path.join('..', variant_dir, 'jniLibs', env['ANDROID_ARCH'])

        local_native_libs_node = ctx.path.make_node(jni_libs_sub_dir)
    else:
        local_native_libs_node = root_output.make_node(os.path.join('builder', 'lib', env['ANDROID_ARCH']))

    local_native_libs_node.mkdir()
    Logs.debug('android: %s' % local_native_libs_node.abspath())
    env['%s_BUILDER_NATIVES' % project_name] = local_native_libs_node.abspath()

    libs_to_copy = env['EXT_LIBS']
    for lib in libs_to_copy:
        lib_name = os.path.basename(lib)

        dest = local_native_libs_node.make_node(lib_name)
        dest.delete()

        shutil.copy2(lib, dest.abspath())
        dest.chmod(511)

    # since we are having android studio building the apk we can kick out early
    if ctx.options.from_android_studio:
        return

    java_codegen = kw.get('java_code_gen_dir', os.path.join(env['PLATFORM'], 'gen'))
    java_source = Utils.to_list(kw['java_source_path'])

    # add the target platform specific source to the master source list
    platform_source_key = '{}_java_source_path'.format(env['PLATFORM'])
    java_source += Utils.to_list(kw[platform_source_key])

    Logs.debug('android: Source paths for target {} : {}'.format(project_name, java_source))

    env['%s_MANIFEST' % project_name] = root_input.make_node(kw['project_manifest']).abspath()
    env['%s_RESOURCES' % project_name] = root_input.make_node(kw['project_resources']).abspath()

    java_source_paths = java_source + [java_codegen]
    java_source_paths = [root_input.make_node(dir_name).abspath() for dir_name in java_source]
    env['%s_J_SRCPATH' % project_name] = os.pathsep.join(java_source_paths)

    env['%s_J_CLASSPATH' % project_name] = os.pathsep.join(env['J_CLASSPATH'])
    env['%s_J_INCLUDES' % project_name] = os.pathsep.join(env['J_INCLUDES'])

    # get the keystore passwords
    if ctx.get_android_build_environment() == 'Distribution':
        key_pass = ctx.options.distro_key_pass
        store_pass = ctx.options.distro_store_pass

        if not (key_pass and store_pass):
            ctx.fatal('[ERROR] Build environment is set to Distribution but --distro-key-pass or --distro-store-pass arguments were not specified or blank')

    else:
        key_pass = ctx.options.dev_key_pass
        store_pass = ctx.options.dev_store_pass

    env['KEYPASS'] = key_pass
    env['STOREPASS'] = store_pass

    # we have to manually create these directories because
    # waf doesn't seem to know how to create the output location
    # if a directory is supplied instead of a file
    java_out = root_output.make_node('java')
    java_out.mkdir()
    env['%s_J_OUT' % project_name] = java_out.abspath()

    gen_out = root_input.make_node(java_codegen)
    gen_out.mkdir()
    env['%s_J_GEN' % project_name] = gen_out.abspath()

    res_out = root_output.make_node('res')
    res_out.mkdir()
    env['%s_RES_OUT' % project_name] = res_out.abspath()

    builder_dir = root_output.make_node('builder')
    builder_dir.mkdir()
    env['%s_BUILDER_DIR' % project_name] = builder_dir.abspath()

    ################################
    # Push all the Android apk packaging into their own build groups with
    # lazy posting to ensure they are processed at the end of the build
    ctx.post_mode = POST_LAZY
    ctx.add_group('%s_java_group' % project_name)

    code_gen = ctx(name = 'code_gen',
        rule = '${AAPT} package ${AAPT_VERBOSE} -f -m -0 apk -M ${%s_MANIFEST} -S ${%s_RESOURCES} -I ${%s_J_INCLUDES} -J ${TGT} --generate-dependencies' % ((project_name,) * 3),
        target = gen_out,
        color = 'PINK',
        always = True,
        shell = False,
        after = 'copy_and_strip_android_natives')

    src_files = []
    for jsrc_dir in java_source:
        jsrc_node = root_input.make_node(jsrc_dir)

        src_files += jsrc_node.ant_glob('**/*.java')
        src_files += jsrc_node.ant_glob('**/*.aidl')

    ctx(name = 'process_java',
        features = 'android',
        source = src_files,
        project = project_name,
        after = code_gen,
        color = 'PINK',
        always = True,
        shell = False)

    # reset the build group/mode back to default
    ctx.post_mode = POST_AT_ONCE
    ctx.set_group('regular_group')


################################################################
@feature('android')
@before('process_source')
def apply_additional_java_to_build(self):
    proj_name = self.project
    Logs.debug('android: Java processing for game project %s' % proj_name)

    env = self.bld.env
    gen_out_node = self.bld.root.find_dir(env['%s_J_GEN' % proj_name])

    java_src_paths = [ gen_out_node ]
    java_src_paths += collect_source_paths(self, 'java_src_path')

    src_path_token = '%s_J_SRCPATH' % proj_name
    java_srcpath = [ env[src_path_token] ]

    src_files = []
    for src_path in java_src_paths:
        java_srcpath.append(src_path.abspath())
        src_files += src_path.ant_glob('**/*.java')

    env[src_path_token] = os.pathsep.join(java_srcpath)

    aidl_files = []
    aidl_paths = collect_source_paths(self, 'aidl_src_path')
    for aidl_dir in aidl_paths:
        aidl_files += aidl_dir.ant_glob('**/*.aidl')

    Logs.debug('android: -> New %s_J_SRCPATH %s' % (proj_name, env[src_path_token]))
    Logs.debug('android: -> Adding the following for Java processing %s' % src_files)
    Logs.debug('android: -> Adding the following for AIDL processing %s' % aidl_files)

    self.source = Utils.to_list(self.source) + aidl_files + src_files


################################################################
@extension('.aidl')
def process_aidl(self, node):
    # set the correct build group/mode
    self.bld.post_mode = POST_LAZY
    self.bld.set_group('%s_java_group' % self.project)

    self.bld(name = 'aidl',
        rule = '${AIDL} ${ANDROID_AIDL_FRAMEWORK} ${SRC}',
        source = node,
        color = 'PINK',
        always = True,
        shell = False)

    # reset the build group/mode back to default
    self.bld.post_mode = POST_AT_ONCE
    self.bld.set_group('regular_group')

################################################################
@extension('.java')
def process_java(self, node):
    # Java compile tasks unfortunately can't be done in parallel (on Windows machines) due to a periodic race condition when writing the .class output files for
    # locally dependent classes.  In order ensure they are processed serially, each task needs to be in its own build group instead of the <game>_java_group.
    self.bld.post_mode = POST_LAZY
    self.bld.add_group()

    self.bld(name = 'javac',
        rule = '${JAVA} -d ${%s_J_OUT} -classpath ${%s_J_CLASSPATH} -sourcepath ${%s_J_SRCPATH} -target ${J_VERSION} -bootclasspath ${%s_J_INCLUDES} -encoding UTF-8 -g -source ${J_VERSION} ${SRC}' % ((self.project,) * 4),
        source = node,
        project = self.project,
        after = 'aidl',
        color = 'PINK',
        always = True,
        shell = False)

    # reset the build group/mode back to default
    self.bld.post_mode = POST_AT_ONCE
    self.bld.set_group('regular_group')


################################################################
@taskgen_method
def handle_natives_copy(self, source_file, dest_location):

    lib_name = os.path.basename(source_file.abspath())
    output_node = dest_location.make_node(lib_name)

    if self.bld.options.from_android_studio:
        if os.path.exists(output_node.abspath()):
            os.remove(output_node.abspath())
        os.symlink(source_file.abspath(), output_node.abspath())
    else:
        self.create_task('copy_and_strip_android_natives', source_file, output_node)


###############################################################################
class copy_and_strip_android_natives(Task):
    color = 'CYAN'
    vars = [ 'STRIP' ]

    def run(self):
        src = self.inputs[0].abspath()
        tgt = self.outputs[0].abspath()

        return self.exec_command('"%s" --strip-debug -o "%s" "%s"' % (self.env.STRIP, tgt, src))

    def runnable_status(self):
        if super(copy_and_strip_android_natives, self).runnable_status() == -1:
            return ASK_LATER

        src = self.inputs[0].abspath()
        tgt = self.outputs[0].abspath()

        # If the target file is missing, we have to copy
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
            # only check timestamps
            if stat_src.st_mtime >= stat_tgt.st_mtime + 2:
                return RUN_ME

        # Everything fine, we can skip this task
        return SKIP_ME


################################################################
@after_method('add_install_copy')
@feature('c', 'cxx')
def add_android_natives_processing(self):
    if 'android' not in self.env['PLATFORM']:
        return

    if not getattr(self, 'link_task', None):
        return

    if self._type == 'stlib': # Do not copy static libs
        return

    output_node = self.bld.get_output_folders(self.bld.env['PLATFORM'], self.bld.env['CONFIGURATION'])[0]

    project_name = getattr(self, 'project_name', None)
    game_projects = self.bld.get_enabled_game_project_list()

    for src in self.link_task.outputs:
        src_lib = output_node.make_node(src.name)

        for game in game_projects:

            game_build_native_key = '%s_BUILDER_NATIVES' % game

            # If the game is a valid android project, a specific build native value will have been created during
            # the project configuration.  Only process games with valid android project settings
            if not game_build_native_key in self.env:
                continue
            game_build_native_node = self.bld.root.find_dir(self.env[game_build_native_key])

            if self.bld.is_module_for_game_project(self.name, game, project_name):
                self.handle_natives_copy(src_lib, game_build_native_node)


################################################################
@feature('android')
@after_method('process_source')
def build_android_apk(self):
    """
    The heavy lifter of the APK packaging process.  Since this process is very order dependent and
    is required to happen at the end of the build (native and java), we push each of the required
    task into it's own build group with lazy posting to ensure that requirement.
    """

    # set the new build group for apk building
    self.bld.post_mode = POST_LAZY
    self.bld.add_group('%s_apk_builder_group' % self.project)

    project_name = self.project
    root_input = self.path.get_src()
    root_output = self.path.get_bld()

    apk_out = root_output.make_node('APKs')
    final_apk_out = self.bld.get_output_folders(self.bld.env['PLATFORM'], self.bld.env['CONFIGURATION'])[0]

    ################################
    # create the dalvik executable
    self.bld(
        name = 'dex',
        rule = '${DX} --dex ${DEX_VERBOSE} --output ${TGT} ${%s_J_OUT}' % project_name,
        target = root_output.make_node('builder/classes.dex'),
        color = 'PINK',
        always = True,
        shell = False)

    ################################
    # handle the standard application assets
    self.bld(
        name = 'crunch',
        rule = '${AAPT} crunch ${AAPT_VERBOSE} -S ${%s_RESOURCES} -C ${TGT}' % project_name,
        target = root_output.make_node('res'),
        after = 'dex',
        color = 'PINK',
        always = True,
        shell = False)

    self.bld(
        name = 'package_resources',
        rule = '${AAPT} package --no-crunch ${AAPT_VERBOSE} -f ${DEBUG_MODE} -0 apk -M ${%s_MANIFEST} -S ${%s_RES_OUT} -S ${%s_RESOURCES} -I ${%s_J_INCLUDES} -F ${TGT} --generate-dependencies' % ((project_name,) * 4),
        target = root_output.make_node('bin/%s.ap_' % project_name),
        after = 'crunch',
        color = 'PINK',
        always = True,
        shell = False)

    ##################################################
    # determine if we need to pack assets into the APK
    pack_assets_in_apk = self.bld.get_android_place_assets_in_apk_setting(project_name)
    self.bld.env['%s_ASSETS' % project_name] = root_input.make_node('assets').abspath()
    target = self.bld.env['%s_ASSETS' % project_name]

    try:
        Logs.debug("android: Removing existing junction to assets")
        os.remove(target)
    except:
        pass

    junction_ready = False

    if pack_assets_in_apk:
        Logs.pprint('PINK',"[INFO] Creating junction to Asset Processor cache")

        assets = self.bld.get_bootstrap_assets("android")
        asset_cache_dir = "cache/{}/{}".format(project_name, assets).lower()
        asset_cache_node = self.bld.path.find_dir(asset_cache_dir)

        if asset_cache_node != None:
            try:
                os.symlink(asset_cache_node.abspath(),target)
                junction_ready = True
            except:
                self.bld.fatal("[ERROR] Could not create junction to Asset Processor cache, presumed to be at [%s]" % target)
        else:
                Logs.warn("[WARN] Could not create junction to Asset Processor cache at [%s] - you may need to run the Asset Processor to create the assets" % target )


    ################################
    # Generating all the APK has to be in the right order.  This is important for Android store APK uploads,
    # if the alignment happens before the signing, then the signing will blow over the alignment and will
    # require a realignment before store upload.
    # 1. Generate the unsigned, unaligned APK
    # 2. Sign the APK
    # 3. Align the APK

    rule_str = '${AAPT} package ${AAPT_VERBOSE} -f -M ${%s_MANIFEST} -S ${%s_RES_OUT} -S ${%s_RESOURCES} -I ${%s_J_INCLUDES} -F ${TGT} ${%s_BUILDER_DIR}' % ((project_name,) * 5)

    if pack_assets_in_apk is True and junction_ready is True:
        rule_str = '${AAPT} package ${AAPT_VERBOSE} -f -M ${%s_MANIFEST} -S ${%s_RES_OUT} -S ${%s_RESOURCES} -I ${%s_J_INCLUDES} -F ${TGT} -A ${%s_ASSETS} ${%s_BUILDER_DIR}' % ((project_name,) * 6)

    # 1. Generate the unsigned, unaligned APK
    raw_apk = self.bld(
        name = 'building_apk',
        rule = rule_str,
        target = apk_out.make_node('%s_unaligned_unsigned.apk' % project_name),
        after = 'package_resources',
        color = 'PINK',
        always = True,
        shell = False)

    # 2. Sign the APK
    signed_apk = self.bld(
        name = 'signing_apk',
        rule = '${JARSIGN} ${JARSIGN_VERBOSE} -keystore ${KEYSTORE} -storepass ${STOREPASS} -keypass ${KEYPASS} -signedjar ${TGT} ${SRC} ${KEYSTORE_ALIAS}',
        source = raw_apk.target,
        target = apk_out.make_node('%s_unaligned.apk' % project_name),
        after = 'building_apk',
        color = 'PINK',
        always = True,
        shell = False)

    # 3. Align the APK
    self.bld(
        name = 'aligning_apk',
        rule = '${ZIPALIGN} ${ZIPALIGN_VERBOSE} -f 4 ${SRC} ${TGT}',
        source = signed_apk.target,
        target = final_apk_out.make_node('%s.apk' % project_name),
        after = 'signing_apk',
        color = 'PINK',
        always = True,
        shell = False)

    # reset the build group/mode back to default
    self.bld.post_mode = POST_AT_ONCE
    self.bld.set_group('regular_group')


###############################################################################
def adb_call(cmd, *args, **keywords):
    ''' Issue a adb command. Args are joined into a single string with spaces
    in between and keyword arguments supported is device=serial # of device
    reported by adb.
    Examples:
    adb_call('shell', 'ls', '-ld') results in "adb shell ls -ld" being executed
    adb_call('shell', 'ls', device='123456') results in "adb -s 123456 shell ls" being executed
    '''
    device_option = ''
    if 'device' in keywords:
        device_option = "-s " + keywords['device']

    cmdline = 'adb {} {} {}'.format(device_option, cmd, ' '.join(list(args)))
    Logs.debug('adb_call: ADB Cmdline: ' + cmdline)
    try:
        return check_output(cmdline, shell=True)
    except:
        Logs.debug('adb_call: exception was thrown. adb command not executed')
        return ""


class adb_copy_output(Task):
    ''' Class to handle copying of a single file in the layout to the android
    device.
    '''

    def __init__(self, *k, **kw):
        Task.__init__(self, *k, **kw)
        self.device = ''
        self.target = ''

    def set_device(self, device):
        '''Sets the android device (serial number from adb devices command) to
        copy the file to'''
        self.device = device

    def set_target(self, target):
        '''Sets the target file directory (absolute path) and file name on the device'''
        self.target = target

    def run(self):
        # Embed quotes in src/target so that we can correctly handle spaces
        src = '"' + self.inputs[0].abspath() + '"'
        tgt = '"' + self.target + '"'

        Logs.debug("adb_copy_output: performing copy - %s to %s on device %s" % (src, tgt, self.device))
        adb_call('push', src, tgt, device=self.device)

        return 0

    def runnable_status(self):
        if Task.runnable_status(self) == ASK_LATER:
            return ASK_LATER

        return RUN_ME

@taskgen_method
def adb_copy_task(self, android_device, src_node, output_target):
    '''Create a adb_copy_output task to copy the src_node to the ouput_target
    on the specified device. output_target is the absolute path and file name
    of the target file.
    '''
    copy_task = self.create_task('adb_copy_output', src_node)
    copy_task.set_device(android_device)
    copy_task.set_target(output_target)


###############################################################################
# create a deployment context for each build variant
for configuration in ['debug', 'profile', 'release']:
    for compiler in ['clang', 'gcc']:
        class DeployAndroidContext(Build.BuildContext):
            fun = 'deploy'
            variant = 'android_armv7_' + compiler + '_' + configuration
            after = ['build_android_armv7_' + compiler + '_' + configuration]
            cmd = 'deploy_android_armv7_' + compiler + '_' + configuration

            def get_bootstrap_files(self):
                config_path = self.get_bootstrap_game().lower() + '/config/'
                config_path += 'game.xml'
                return ['bootstrap.cfg', config_path]

            def use_vfs(self):
                try:
                    return self.cached_use_vfs
                except:
                    self.cached_use_vfs = self.get_bootstrap_vfs() == '1'

                return self.use_vfs()

            def use_paks(self):
                try:
                    return self.cached_use_paks
                except:
                    (platform, configuration) = self.get_platform_and_configuration()
                    self.cached_use_paks = configuration == 'release' or self.is_option_true('deploy_android_paks')

                return self.use_paks()

            def get_layout_node(self):
                try:
                    return self.android_armv7_layout_node
                except:
                    if self.use_vfs():
                        self.android_armv7_layout_node = self.path.make_node('AndroidArmv7LayoutVFS')
                    elif self.use_paks():
                        self.android_armv7_layout_node = self.path.make_node('AndroidArmv7LayoutPak')
                    else:
                        # For android we don't have to copy the assets to the layout node, so just use the cache folder
                        game = self.get_bootstrap_game()
                        assets = self.get_bootstrap_assets("android")
                        asset_cache_dir = "cache/{}/{}".format(game, assets).lower()
                        self.android_armv7_layout_node = self.path.find_dir(asset_cache_dir)
                        if (self.android_armv7_layout_node == None):
                            Logs.warn("[WARN] android deploy: There is no asset cache to read from at " + asset_cache_dir)
                            Logs.warn("[WARN] android deploy: You probably need to run Bin64/AssetProcessorBatch.exe")

                return self.get_layout_node()

        class DeployAndroid(DeployAndroidContext):
            after = ['build_android_armv7_' + compiler + '_' + configuration]
            cmd = 'deploy_android_armv7_' + compiler + '_' + configuration
            features = ['deploy_android_prepare']

        class DeployAndroidDevices(DeployAndroidContext):
            after = ['deploy_android_armv7_' + compiler + '_' + configuration]
            cmd = 'deploy_android_devices_' + compiler + '_' + configuration
            features = ['deploy_android_devices']

def get_list_of_android_devices():
    ''' Gets the connected android devices using the adb command devices and
        returns a list of serial numbers of connected devices.
    '''
    devices_output = adb_call("devices").split(os.linesep)
    devices = []
    for output in devices_output:
        if "List" in output or "*" in output:
            Logs.debug("android: skipping the following line as it has 'List' or '*' in it: %s" % output)
            continue
        device_serial = output.split()
        if len(device_serial) != 0:
            devices.append(device_serial[0])
            Logs.debug("android: found device with serial: " + device_serial[0])
    return devices

@taskgen_method
@feature('deploy_android_prepare')
def prepare_to_deploy_android(ctx):
    '''Deploy an android build to connected android devices. This will create
    the AndroidLayout* folder first with all the assets in it, which is then
    copied to the device location specified in the deploy_android_root_dir
    option.
    '''

    color = 'CYAN'
    bld = ctx.bld
    platform = bld.env['PLATFORM']
    configuration = bld.env['CONFIGURATION']

    if platform not in ('android_armv7_gcc', 'android_armv7_clang'):
        return

    if not bld.is_option_true('deploy_android') or bld.options.from_android_studio:
        Logs.pprint(color, 'Skipping Android Deployment...')
        return

    if bld.use_vfs() and configuration == 'release':
        bld.fatal('Cannot use VFS in a release build, please set remote_filesystem=0 in bootstrap.cfg')

    game = bld.get_bootstrap_game()

    if bld.use_paks():
        assets = bld.get_bootstrap_assets("android")
        asset_cache_dir = "cache/{}/{}".format(game, assets).lower()
        asset_cache_node = bld.path.find_dir(asset_cache_dir)

        if (asset_cache_node != None):
            layout_node = bld.get_layout_node()

            call('Bin64\\rc\\rc.exe /job=Bin64\\rc\\RcJob_Generic_MakePaks.xml /p=es3 /src={} /trg={} /threads=8 /game={}'.format(asset_cache_dir, layout_node.relpath().lower(), game), shell=True)
        else:
            Logs.warn("[WARN] android deploy preparation: Layout generation is enabled, but there is no asset cache to read from at " + asset_cache_dir)
            Logs.warn("[WARN] android deploy preparation: You probably need to run Bin64/AssetProcessor.exe (GUI) or Bin64/AssetProcessorBatch.exe")


    compiler = platform.split('_')[2]
    Options.commands.append('deploy_android_devices_' + compiler + '_' + configuration)


@taskgen_method
@feature('deploy_android_devices')
def deploy_to_devices(ctx):
    '''Installs the project APK and copies the layout directory to all the
    android devices that are connected to the host.
    '''
    def should_copy_file(src_file_node, target_time):
        should_copy = False 
        try:
            stat_src = os.stat(src_file_node.abspath())
            should_copy = stat_src.st_mtime >= target_time
        except OSError:
            pass

        return should_copy


    # make sure the adb server is running first before we execute any other
    # commands
    adb_call('start-server')

    devices = get_list_of_android_devices()
    if len(devices) == 0:
        Logs.warn("[WARN] android deploy: there are no connected devices to deploy a build to. Skipping deploy step.")
        return

    color = 'CYAN'
    bld = ctx.bld
    platform = bld.env['PLATFORM']
    configuration = bld.env['CONFIGURATION']

    game = bld.get_bootstrap_game()

    # get location of APK either from command line option or default build location
    if Options.options.apk_path == '':
        output_folder = bld.get_output_folders(platform, configuration)[0]
        apk_name = output_folder.abspath() + "/" + game + ".apk"
    else:
        apk_name = Options.options.apk_path

    device_install_dir = "/storage/sdcard0/"
    if hasattr(Options.options, 'deploy_android_root_dir'):
        device_install_dir = getattr(Options.options, 'deploy_android_root_dir')

    output_target = "%s/%s/" % (device_install_dir, game)

    # This is the name of a file that we will use as a marker/timestamp. We
    # will get the timestamp of the file off the device and compare that with
    # asset files on the host machine to determine if the host machine asset
    # file is newer than what the device has, and if so copy it to the device.
    timestamp_file_name = "engineroot.txt"
    device_timestamp_file = output_target + timestamp_file_name

    do_clean = bld.is_option_true('deploy_android_clean_device')
    deploy_executable = bld.is_option_true("deploy_android_executable")
    deploy_assets = bld.is_option_true("deploy_android_assets") and not bld.use_vfs()

    Logs.debug('android: deploy options: do_clean %s, deploy_exec %s, deploy_assets %s' % (do_clean, deploy_executable, deploy_assets))

    deploy_count = 0

    for android_device in devices:
        Logs.pprint(color, 'Starting to deploy to android device ' + android_device)

        if do_clean:
            Logs.pprint(color, 'Cleaning target before deployment...')
            adb_call('shell', 'rm -rf ', output_target, device=android_device)
            Logs.pprint(color, 'Target Cleaned...')

            package_name = bld.get_android_package_name(game)
            if len(package_name) != 0 and deploy_executable:
                Logs.pprint(color, 'Uninstalling package ' + package_name)
                adb_call('uninstall', package_name, device=android_device)

        if deploy_executable:
            install_options = getattr(Options.options, 'deploy_android_install_options')
            replace_package = ''
            if bld.is_option_true('deploy_android_replace_apk'):
                replace_package = '-r'

            Logs.pprint(color, 'Installing ' + apk_name)
            adb_call('install', install_options, replace_package, '"' + apk_name + '"', device=android_device)

        if deploy_assets:

            ls_output = adb_call('shell', 'ls', device_install_dir, device=android_device)
            if 'Permission denied' in ls_output or 'No such file' in ls_output:
                Logs.warn('[WARN] android deploy: unable to access the destination %s on the target device %s - skipping deploying assets. Make sure deploy_android_root_dir option is valid for the device' % (output_target, android_device))
                continue

            layout_node = bld.get_layout_node()

            ls_output = adb_call('shell', 'ls -l ', device_timestamp_file, device=android_device)
            target_time = 0;
            if "No such file" not in ls_output:
                tgt_ls_fields = ls_output.split()
                target_time = time.mktime(time.strptime(tgt_ls_fields[4] + " " + tgt_ls_fields[5], "%Y-%m-%d %H:%M"))
                Logs.debug('android: %s time is %s' % (device_timestamp_file, target_time))

            if do_clean or target_time == 0:
                Logs.pprint(color, 'Copying all assets to the device %s. This may take some time...' % android_device)

                adb_call('push', '"' + layout_node.abspath() + '"', output_target)
            else:
                layout_files = layout_node.ant_glob('**/*')
                Logs.pprint(color, 'Scanning %d files to determine which ones need to be copied...' % len(layout_files))
                for src_file in layout_files:
                    # Faster to check if we should copy now rather than in the copy_task 
                    if should_copy_file(src_file, target_time):
                        final_target_dir = output_target + string.replace(src_file.path_from(layout_node), '\\', '/')
                        ctx.adb_copy_task(android_device, src_file, final_target_dir)

                # Push the timestamp_file_name last so that it has a timestamp
                # that we can use on the next deploy to know which files to
                # upload to the device
                host_timestamp_file = ctx.bld.path.find_node(timestamp_file_name)
                adb_call('push', '"' + host_timestamp_file.abspath() + '"', device_timestamp_file, device=android_device)


        Logs.pprint(color, 'Copying required files for booting ...' )
        for bs_file in bld.get_bootstrap_files():
            bs_src = ctx.bld.path.find_node(bs_file)
            if bs_src != None:
                target_file_with_dir = output_target + bs_src.relpath().replace('\\', '/')
                ls_output = adb_call('ls', target_file_with_dir, device=android_device)
                if 'Permission denied' in ls_output or 'No such file' in ls_output:
                    adb_call('mkdir', target_file_with_dir, device=android_device)
                adb_call('push', '"' + bs_src.abspath() + '"', target_file_with_dir, device=android_device)

        deploy_count = deploy_count + 1

    if deploy_count == 0:
        Logs.warn("[WARN] android deploy: could not deploy the build to any connected devices.")
        pass

