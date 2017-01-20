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
def load_android_clang_common_settings(conf):
    """
    Setup all compiler/linker flags with are shared over all targets using the android compiler
    !!! But not the actual compiler, since the compiler depends on the target !!!
    """

    ################################
    # common settings
    env = conf.env

    # CC/CXX Compiler
    env['CC_NAME']  = env['CXX_NAME'] = 'clang'
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
        # Enable all warnings and treat them as errors,
        # but don't warn about unkown warnings in order
        # to maintain backwards compatibility with older
        # toolchain versions.
        '-Wall',
        '-Werror',
        '-Wno-unknown-warning-option',
        
        # Disabled warnings (please do not disable any others without first consulting ly-warnings)
        '-Wno-#pragma-messages',
        '-Wno-absolute-value',
        '-Wno-dynamic-class-memaccess',
        '-Wno-format-security',
        '-Wno-inconsistent-missing-override',
        '-Wno-invalid-offsetof',
        '-Wno-multichar',
        '-Wno-parentheses',
        '-Wno-reorder',
        '-Wno-self-assign',
        '-Wno-switch',
        '-Wno-tautological-compare',
        '-Wno-unused-function',
        '-Wno-unused-private-field',
        '-Wno-unused-value',
        '-Wno-unused-variable',
        
        # Other
        '-femulated-tls',               # All accesses to TLS variables are converted to calls to __emutls_get_address in the runtime library
        '-ffast-math',                  # Enable fast math
        '-fms-extensions',              # Allow MSVC language extensions
        '-fPIC',                        # Emits position-independent code for dynamic linking
        '-fvisibility=hidden',
        '-fvisibility-inlines-hidden',
    ]

    env['CFLAGS'] += common_flags[:]

    env['CXXFLAGS'] += common_flags[:]
    env['CXXFLAGS'] += [
        '-std=c++1y',                   # C++14
        '-fno-rtti',                    # Disable RTTI
        '-fno-exceptions',              # Disable Exceptions
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
        'c++_shared',           # shared library of llvm stl
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
def load_debug_android_clang_settings(conf):
    """
    Setup all compiler/linker flags with are shared over all targets using the android compiler
    for the "debug" configuration
    """

    conf.load_android_clang_common_settings()
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
def load_profile_android_clang_settings(conf):
    """
    Setup all compiler/linker flags with are shared over all targets using the android compiler
    for the "profile" configuration
    """

    conf.load_android_clang_common_settings()
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
def load_performance_android_clang_settings(conf):
    """
    Setup all compiler/linker flags with are shared over all targets using the android compiler
    for the "performance" configuration
    """

    conf.load_android_clang_common_settings()
    env = conf.env

    env['DEFINES'] += [ 'NDEBUG' ]

    common_flags = [
        '-O3',          # all optimizations
    ]

    env['CFLAGS'] += common_flags[:]
    env['CXXFLAGS'] += common_flags[:]


################################################################
@conf
def load_release_android_clang_settings(conf):
    """
    Setup all compiler/linker flags with are shared over all targets using the android compiler
    for the "release" configuration
    """

    conf.load_android_clang_common_settings()
    env = conf.env

    env['DEFINES'] += [ 'NDEBUG' ]

    common_flags = [
        '-O3',          # all optimizations
    ]

    env['CFLAGS'] += common_flags[:]
    env['CXXFLAGS'] += common_flags[:]

