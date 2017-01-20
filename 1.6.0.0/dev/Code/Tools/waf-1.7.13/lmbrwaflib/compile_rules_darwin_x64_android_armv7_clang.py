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
import os, sys
from waflib import Logs
from waflib.Configure import conf

TARGET_PLATFORM = 'android_armv7_clang'

################################################################
@conf
def get_android_armv7_clang_target_abi(conf):
    return 'armeabi-v7a'

################################################################
@conf
def load_darwin_x64_android_armv7_clang_common_settings(conf):
    """
    Setup all compiler and linker settings shared over all darwin_x64_android_armv7_clang configurations
    """

    ################################
    # make sure the android sdk/ndk installs are valid
    if not conf.validate_android_api_version():
        return False

    if not conf.load_android_tools():
        return False

    if not conf.validate_android_ndk_install():
        return False

    ################################
    # load the compiler toolchain
    ndk_root = conf.env['NDK_ROOT']

    clang_toolchain_path = os.path.join(ndk_root, 'toolchains', 'llvm', 'prebuilt', 'darwin-x86_64', 'bin')
    gcc_toolchain_path = os.path.join(ndk_root, 'toolchains', 'arm-linux-androideabi-4.9', 'prebuilt', 'darwin-x86_64', 'bin')
    if not conf.load_android_toolchains([clang_toolchain_path, gcc_toolchain_path], 'clang', 'clang++', 'arm-linux-androideabi-gcc', 'arm-linux-androideabi-g++', 'arm-linux-androideabi-ar', 'arm-linux-androideabi-strip'):
        return False

    ################################
    # common settings
    env = conf.env

    platform_root = conf.get_corrected_android_ndk_platform_includes(ndk_root, 'arch-arm')
    stl_root = os.path.join(ndk_root, 'sources', 'cxx-stl', 'llvm-libc++')

    env['INCLUDES'] += [
        os.path.join(platform_root, 'usr', 'include'),
        os.path.join(platform_root, 'usr', 'include', 'SLES'),
        os.path.join(ndk_root, 'sources', 'android', 'support', 'include'),
        os.path.join(stl_root, 'libcxx', 'include'),
        os.path.join(stl_root, 'libs', 'armeabi-v7a', 'include'),
        os.path.join(conf.path.abspath(), 'Code', 'SDKs', 'SDL2', 'src', 'include'),
    ]

    env['DEFINES'] += [
        '__ARM_NEON__'
    ]

    system_root = '--sysroot=%s' % platform_root
    common_flags = [
        system_root,

        '--target=armv7-none-linux-androideabi',
        '-mfloat-abi=softfp',       # float ABI: hardware code gen, soft calling convention
        '-mfpu=neon',               # enable neon, implies -mfpu=VFPv3-D32
    ]

    env['CFLAGS'] += common_flags[:]
    env['CXXFLAGS'] += common_flags[:]

    env['LIBPATH'] += [
        os.path.join(stl_root, 'libs', 'armeabi-v7a'),
        os.path.join(platform_root, 'usr', 'lib'),
    ]

    env['LINKFLAGS'] += [
        system_root,
        '-march=armv7-a',           # armv7-a architecture
        '-Wl,--fix-cortex-a8'       # required to fix a bug in some Cortex-A8 implementations for neon support
    ]

    env['ANDROID_ARCH'] = 'armeabi-v7a'

    ################################
    # required 3rd party libs that need to be included in the apk
    env['EXT_LIBS'] += [
        os.path.join(stl_root, 'libs', 'armeabi-v7a', 'libc++_shared.so'),
        os.path.join(ndk_root, 'prebuilt', 'android-arm', 'gdbserver', 'gdbserver')
    ]

    return True


################################################################
@conf
def load_debug_darwin_x64_android_armv7_clang_settings(conf):
    """
    Setup all compiler and linker settings shared over all darwin_x64_android_armv7_clang configurations for
    the 'debug' configuration
    """

    # load this first because it finds the compiler
    if not conf.load_darwin_x64_android_armv7_clang_common_settings():
        conf.fatal('[ERROR] %s setup failed' % TARGET_PLATFORM)

    # Load addional shared settings
    conf.load_debug_cryengine_settings()
    conf.load_debug_android_clang_settings()


################################################################
@conf
def load_profile_darwin_x64_android_armv7_clang_settings(conf):
    """
    Setup all compiler and linker settings shared over all darwin_x64_android_armv7_clang configurations for
    the 'profile' configuration
    """

    # load this first because it finds the compiler
    if not conf.load_darwin_x64_android_armv7_clang_common_settings():
        conf.fatal('[ERROR] %s setup failed' % TARGET_PLATFORM)

    # Load addional shared settings
    conf.load_profile_cryengine_settings()
    conf.load_profile_android_clang_settings()


################################################################
@conf
def load_performance_darwin_x64_android_armv7_clang_settings(conf):
    """
    Setup all compiler and linker settings shared over all darwin_x64_android_armv7_clang configurations for
    the 'performance' configuration
    """

    # load this first because it finds the compiler
    if not conf.load_darwin_x64_android_armv7_clang_common_settings():
        conf.fatal('[ERROR] %s setup failed' % TARGET_PLATFORM)

    # Load addional shared settings
    conf.load_performance_cryengine_settings()
    conf.load_performance_android_clang_settings()


################################################################
@conf
def load_release_darwin_x64_android_armv7_clang_settings(conf):
    """
    Setup all compiler and linker settings shared over all darwin_x64_android_armv7_clang configurations for
    the 'release' configuration
    """

    # load this first because it finds the compiler
    if not conf.load_darwin_x64_android_armv7_clang_common_settings():
        conf.fatal('[ERROR] %s setup failed' % TARGET_PLATFORM)

    # Load addional shared settings
    conf.load_release_cryengine_settings()
    conf.load_release_android_clang_settings()
