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
import os,sys
from waflib import Utils, Logs
from waflib.Configure import conf
from waflib.TaskGen import feature, before_method, after_method

################################################################
@conf
def load_android_gcc_common_settings(conf):
    """
    Setup all compiler/linker flags with are shared over all targets using the android compiler
    !!! But not the actual compiler, since the compiler depends on the target !!!
    """

    ################################
    # common settings
    env = conf.env

    # Figure out GCC compiler version
    conf.get_cc_version([ env['CC'] ], gcc = True)

    # CC/CXX Compiler
    env['CC_NAME']  = env['CXX_NAME'] = 'gcc'
    env['CC_SRC_F'] = env['CXX_SRC_F'] = []
    env['CC_TGT_F'] = env['CXX_TGT_F'] = ['-c', '-o']
    env['ARFLAGS'] = 'rcs'

    env['CPPPATH_ST'] = '-I%s'
    env['DEFINES_ST'] = '-D%s'

    env['DEFINES'] += [
        '_LINUX',
        'LINUX',
        'LINUX32',
        'ANDROID',
        'MOBILE',
        '_HAS_C9X',
        'ENABLE_TYPE_INFO',
    ]

    common_flags = [
        '-Werror',                  # treat warnings as errors

        # Disable pragma warnings from being errors
        '-Wno-error=pragmas',       # Bad pragmas wont cause compile errors, just warnings

        # Enabled Warnings
        '-Wsequence-point',         # undefined semantics warning
        '-Wmissing-braces',         # missing brackets in union initializers
        '-Wreturn-type',            # no return value
        '-Winit-self',              # un-initialized vars used to init themselves

        # Disabled Warnings
        '-Wno-multichar',           # multicharacter constant being used
        '-Wno-narrowing',           # narrowing conversions within '{ }'
        '-Wno-unused-result',       # warn_unused_result function attribute usage
        '-Wno-write-strings',       # string literal to non-const char *
        '-Wno-format-security',     # non string literal format for printf and scanf
        '-Wno-unused-function',     # unused inline static functions / no definition to static functions
        '-Winvalid-pch',            # cant use found pre-compiled header
        '-Wno-deprecated',          # allow deprecated features

        # Other
        '-ffast-math',              # Enable fast math
        '-fms-extensions',          # allow some non standard constructs
        '-fvisibility=hidden',      # ELF image symbol visibility
        '-MP',                      # add PHONY targets to each dependency
        '-MMD',                     # use file name as object file name
        '-fPIC',                    # emits position-independent code for dynamic linking
    ]

    env['CFLAGS'] += common_flags[:]

    env['CXXFLAGS'] += common_flags[:]
    env['CXXFLAGS'] += [
        # Disabled Warnings
        '-Wno-conversion-null',         # conversion between NULL and non pointer types
        '-Wno-invalid-offsetof',        # offsetof used on non POD types

        # Other
        '-fno-rtti',                    # disable RTTI
        '-fno-exceptions',              # disable Exceptions
        '-fvisibility-inlines-hidden',  # comparing function pointers between two shared objects
        '-fpermissive',                 # allow some non-conformant code
        '-std=gnu++1y',                 # set the c++ standard to C++14
    ]

    # Linker
    env['CCLNK_SRC_F'] = env['CXXLNK_SRC_F'] = []
    env['CCLNK_TGT_F'] = env['CXXLNK_TGT_F'] = '-o'

    env['LIB_ST'] = env['STLIB_ST'] = '-l%s'
    env['LIBPATH_ST'] = env['STLIBPATH_ST'] = '-L%s'

    env['CFLAGS_cshlib'] = env['CFLAGS_cxxshlib'] = ['-fPIC']
    env['CXXFLAGS_cshlib'] = env['CXXFLAGS_cxxshlib'] = ['-fPIC']

    env['LINKFLAGS_cshlib'] = env['LINKFLAGS_cxxshlib'] = ['-shared']

    env['CFLAGS_cstlib'] = env['CFLAGS_cxxstlib'] = []
    env['CXXFLAGS_cstlib'] = env['CXXFLAGS_cxxstlib'] = []

    env['LINKFLAGS_cxxstlib'] = env['LINKFLAGS_cxxshtib'] = ['-Wl,-Bstatic']

    env['LIB'] += [
        'android',              # android library
        'c',                    # c library for android
        'log',                  # log library for android
        'gnustl_shared',        # shared library of gnu stl
        'dl',                   # dynamic library
    ]

    env['LINKFLAGS'] += [
        '-rdynamic',            # add ALL symbols to the dynamic symbol table
    ]

    env['RPATH_ST'] = '-Wl,-rpath,%s'

    env['SONAME_ST'] = '-Wl,-soname,%s'   # sets the DT_SONAME field in the shared object, used for ELF object loading

    env['SHLIB_MARKER'] = '-Wl,-Bdynamic'
    env['STLIB_MARKER'] = '-Wl,-Bstatic'

    env['cprogram_PATTERN'] = env['cxxprogram_PATTERN'] = '%s'
    env['cshlib_PATTERN'] = env['cxxshlib_PATTERN'] = 'lib%s.so'
    env['cstlib_PATTERN'] = env['cxxstlib_PATTERN'] = 'lib%s.a'

    ################################
    # frameworks aren't supported on Android, disable it
    env['FRAMEWORK'] = []
    env['FRAMEWORK_ST'] = ''
    env['FRAMEWORKPATH'] = []
    env['FRAMEWORKPATH_ST'] = ''

    ################################
    # apk packaging settings
    platform = os.path.join(env['ANDROID_HOME'], 'platforms', conf.get_android_sdk_version())

    java_common = [
        os.path.join(platform, 'android.jar'),
    ]

    env['J_INCLUDES'] = java_common[:]
    env['J_CLASSPATH'] = java_common[:]
    env['J_DEBUG_MODE'] = ''
    env['J_VERSION'] = '1.7'

    env['KEYSTORE_ALIAS'] = conf.get_android_env_keystore_alias()
    env['KEYSTORE'] = conf.get_android_env_keystore_path()

    env['DEX_VERBOSE'] = env['JARSIGN_VERBOSE'] = env['AAPT_VERBOSE'] = env['ZIPALIGN_VERBOSE'] = ''

    env['ANDROID_AIDL_FRAMEWORK'] = '-p' + platform + '/framework.aidl'

    if conf.options.verbose >= 1:
        env['DEX_VERBOSE'] = '--verbose'
        env['JARSIGN_VERBOSE'] = '-verbose'
        env['AAPT_VERBOSE'] = env['ZIPALIGN_VERBOSE'] = '-v'



################################################################
@conf
def load_debug_android_gcc_settings(conf):
    """
    Setup all compiler/linker flags with are shared over all targets using the android compiler
    for the "debug" configuration
    """

    conf.load_android_gcc_common_settings()
    env = conf.env

    common_flags = [
        '-O0',                  # No optimization
        '-fno-inline',          #
        '-fstack-protector',    #
        '-g',                   # debugging information
        '-gdwarf-2',            # DAWRF 2 debugging information
    ]

    env['CFLAGS'] += common_flags[:]
    env['CXXFLAGS'] += common_flags[:]

    env['J_DEBUG_MODE'] = '--debug-mode'


################################################################
@conf
def load_profile_android_gcc_settings(conf):
    """
    Setup all compiler/linker flags with are shared over all targets using the android compiler
    for the "profile" configuration
    """

    conf.load_android_gcc_common_settings()
    env = conf.env

    env['DEFINES'] += [ 'NDEBUG' ]

    common_flags = [
        '-O3',          # all optimizations
        '-g',           # debugging information
        '-gdwarf-2'     # DAWRF 2 debugging information
    ]

    env['CFLAGS'] += common_flags[:]
    env['CXXFLAGS'] += common_flags[:]


################################################################
@conf
def load_performance_android_gcc_settings(conf):
    """
    Setup all compiler/linker flags with are shared over all targets using the android compiler
    for the "performance" configuration
    """

    conf.load_android_gcc_common_settings()
    env = conf.env

    env['DEFINES'] += [ 'NDEBUG' ]

    common_flags = [
        '-O3',          # all optimizations
    ]

    env['CFLAGS'] += common_flags[:]
    env['CXXFLAGS'] += common_flags[:]


################################################################
@conf
def load_release_android_gcc_settings(conf):
    """
    Setup all compiler/linker flags with are shared over all targets using the android compiler
    for the "release" configuration
    """

    conf.load_android_gcc_common_settings()
    env = conf.env

    env['DEFINES'] += [ 'NDEBUG' ]

    common_flags = [
        '-O3',          # all optimizations
    ]

    env['CFLAGS'] += common_flags[:]
    env['CXXFLAGS'] += common_flags[:]

