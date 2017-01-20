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

from waflib import Logs
from waflib.Configure import conf
from waflib.TaskGen import feature, before_method, after_method
import subprocess

@conf
def load_darwin_common_settings(conf):
    """
    Setup all compiler and linker settings shared over all darwin configurations
    """
    v = conf.env
    
    # Setup common defines for darwin
    v['DEFINES'] += [ 'APPLE', 'MAC', '__APPLE__', 'DARWIN' ]
    
    # Set Minimum mac os version to 10.11
    v['CFLAGS'] += [ '-mmacosx-version-min=10.9' ]
    v['CXXFLAGS'] += [ '-mmacosx-version-min=10.9' ]
    v['LINKFLAGS'] += [ '-mmacosx-version-min=10.9', ]
    
    # For now, only support 64 bit MacOs Applications
    v['ARCH'] = ['x86_64']
    
    # Pattern to transform outputs
    v['cprogram_PATTERN']   = '%s'
    v['cxxprogram_PATTERN'] = '%s'
    v['cshlib_PATTERN']     = 'lib%s.dylib'
    v['cxxshlib_PATTERN']   = 'lib%s.dylib'
    v['cstlib_PATTERN']      = 'lib%s.a'
    v['cxxstlib_PATTERN']    = 'lib%s.a'
    v['macbundle_PATTERN']    = 'lib%s.dylib'
    
    # Specify how to translate some common operations for a specific compiler   
    v['FRAMEWORK_ST']       = ['-framework']
    v['FRAMEWORKPATH_ST']   = '-F%s'
    v['RPATH_ST'] = '-Wl,-rpath,%s'
    
    # Default frameworks to always link
    v['FRAMEWORK'] = [ 'Foundation', 'Cocoa', 'Carbon', 'CoreServices', 'ApplicationServices', 'AudioUnit', 'CoreAudio', 'AppKit', 'ForceFeedBack', 'IOKit', 'OpenGL' ]
    
    # Default libraries to always link
    v['LIB'] = ['c', 'm', 'pthread', 'ncurses']
    
    # Setup compiler and linker settings  for mac bundles
    v['CFLAGS_MACBUNDLE'] = v['CXXFLAGS_MACBUNDLE'] = '-fpic'
    v['LINKFLAGS_MACBUNDLE'] = [
        '-dynamiclib',
        '-undefined', 
        'dynamic_lookup'
        ]
        
    # Set the path to the current sdk
    sdk_path = subprocess.check_output(["xcrun", "--sdk", "macosx", "--show-sdk-path"]).strip()
    v['CFLAGS'] += [ '-isysroot' + sdk_path, ]
    v['CXXFLAGS'] += [ '-isysroot' + sdk_path, ]
    v['LINKFLAGS'] += [ '-isysroot' + sdk_path, ]

    # Since we only support a single darwin target (clang-64bit), we specify all tools directly here    
    v['AR'] = 'ar'
    v['CC'] = 'clang'
    v['CXX'] = 'clang++'
    v['LINK'] = v['LINK_CC'] = v['LINK_CXX'] = 'clang++'
    
@conf
def load_debug_darwin_settings(conf):
    """
    Setup all compiler and linker settings shared over all darwin configurations for
    the 'debug' configuration
    """
    v = conf.env
    conf.load_darwin_common_settings()

@conf
def load_profile_darwin_settings(conf):
    """
    Setup all compiler and linker settings shared over all darwin configurations for
    the 'debug' configuration
    """
    v = conf.env
    conf.load_darwin_common_settings()

@conf
def load_performance_darwin_settings(conf):
    """
    Setup all compiler and linker settings shared over all darwin configurations for
    the 'debug' configuration
    """
    v = conf.env
    conf.load_darwin_common_settings()

@conf
def load_release_darwin_settings(conf):
    """
    Setup all compiler and linker settings shared over all darwin configurations for
    the 'debug' configuration
    """
    v = conf.env
    conf.load_darwin_common_settings()


@feature('cprogram', 'cxxprogram')
@after_method('post_command_exec')
def add_post_build_mac_command(self):
    """
    Function to add a post build method if we are creating a mac launcher and
    add copy jobs for any 3rd party libraries that are used by the launcher to
    the executable directory.
    """
    if getattr(self, 'mac_launcher', False) and self.env['PLATFORM'] != 'project_generator':
        self.bld.add_post_fun(add_transfer_jobs_for_mac_launcher)

        # Screen the potential libpaths first
        libpaths = []
        for libpath in self.libpath:
            full_libpath = os.path.join(self.bld.root.abspath(), libpath)
            if not os.path.exists(full_libpath):
                Logs.warn('[WARN] Unable to find library path {} to copy libraries from.')
            else:
                libpaths.append(libpath)

        for lib in self.lib:
            for libpath in libpaths:
                libpath_node = self.bld.root.find_node(libpath)
                library = libpath_node.ant_glob("lib" + lib + ".dylib")
                if library:
                    self.copy_files(library[0])

import os, shutil
def add_transfer_jobs_for_mac_launcher(self):
    """
    Function to move CryEngine libraries that the mac launcher uses to the
    project .app Contents/MacOS folder where they need to be for the executable
    to run
    """
    output_folders = self.get_output_folders(self.env['PLATFORM'], self.env['CONFIGURATION'])
    for folder in output_folders:
        files_to_copy = folder.ant_glob("*.dylib")
        for project in self.get_enabled_game_project_list():
            executable_name = self.get_executable_name(project)
            for file in files_to_copy:
                shutil.copy2(file.abspath(), folder.abspath() + "/" + executable_name + ".app/Contents/MacOS")
