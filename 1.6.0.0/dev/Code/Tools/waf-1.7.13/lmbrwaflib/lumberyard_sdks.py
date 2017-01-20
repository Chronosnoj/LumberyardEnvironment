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

from waflib.TaskGen import feature, before_method, after_method
from waflib.Configure import conf
from waflib.Tools.ccroot import lib_patterns, SYSTEM_LIB_PATHS
from waflib import Node, Utils, Errors
from waflib.Build import BuildContext
from waf_branch_spec import PLATFORMS
import os

def is_win_x64_platform(ctx):
    platform = ctx.env['PLATFORM'].lower()
    return ('win_x64' in platform) and (platform in PLATFORMS['win32'])

def is_linux_platform(ctx):
    platform = ctx.env['PLATFORM'].lower()
    return ('linux' in platform)

def get_static_suffix():
    return "_static"

def get_shared_suffix():
    return "_shared"

def should_link_aws_native_sdk_statically(bld):
    platform = bld.env['PLATFORM']
    if any(substring in platform for substring in ['darwin', 'ios', 'appletv', 'linux']):
        return True
    return False

def get_dynamic_lib_extension(bld):
    platform = bld.env['PLATFORM']
    if any(substring in platform for substring in ['darwin', 'ios', 'appletv']):
        return '.dylib'
    elif 'linux' in platform:
        return '.so'
    return '.dll'

def get_platform_lib_prefix(bld):
    platform = bld.env['PLATFORM']
    if any(substring in platform for substring in ['darwin', 'ios', 'appletv', 'linux']):
        return 'lib'
    return ''

def should_project_include_aws_native_sdk(bld):
    platform = bld.env['PLATFORM']
    if 'project_generator' in platform:
        return True # Build
    return does_platform_support_aws_native_sdk(bld)

def does_platform_support_aws_native_sdk(bld):
    platform = bld.env['PLATFORM']
    if any(substring in platform for substring in ['win', 'darwin', 'ios', 'appletv']):
        return True
    elif 'linux' in platform:
        return True
    return False

@conf
def add_aws_native_sdk_platform_defines(self,define_list):
    if should_project_include_aws_native_sdk(self):
        define_list.append('PLATFORM_SUPPORTS_AWS_NATIVE_SDK')

@conf
def make_static_library_task_name(self, libName):
        return libName + get_static_suffix()

@conf
def make_shared_library_task_name(self, libName):
    return libName + get_shared_suffix()

@conf
def make_shared_library_task_list(self, libraryList):
        return [ self.make_shared_library_task_name(l) for l in libraryList ]

@conf
def make_static_library_task_list(self, libraryList):
        return [ self.make_static_library_task_name(l) for l in libraryList ]

@conf
def make_aws_library_task_list(self, libraryList):
    shouldLinkStatically = isinstance(self, BuildContext) and should_link_aws_native_sdk_statically(self) 
    if shouldLinkStatically:
        return self.make_static_library_task_list(libraryList)
    else:
        return self.make_shared_library_task_list(libraryList)


def convert_dual_task_name_to_lib_name(taskName):
        shared_index = taskName.find(get_shared_suffix())
        if shared_index >= 0:
                return taskName[0:shared_index]
        else:
                return taskName[0:taskName.find(get_static_suffix())]
        
@conf
def read_dual_library_shared(self, name, paths=[], export_includes=[], export_defines=[]):
    """
    Read a system shared library, enabling its use as a local library.   Performs name-mangling on the task name to ensure uniqueness with
    a static version of the same library.  Will trigger a rebuild if the file changes.

    """

    return self(name=self.make_shared_library_task_name(name), features='fake_dual_lib', lib_paths=paths, lib_type='shlib', export_includes=export_includes, export_defines=export_defines)

@conf
def read_dual_library_static(self, name, paths=[], export_includes=[], export_defines=[]):
    """
    Read a system static library, enabling a use as a local library. Performs name-mangling on the task name to ensure uniqueness with
    a shared version of the same library.  Will trigger a rebuild if the file changes.
    """
    return self(name=self.make_static_library_task_name(name), features='fake_dual_lib', lib_paths=paths, lib_type='stlib', export_includes=export_includes, export_defines=export_defines)


@feature('fake_dual_lib')
def process_dual_lib(self):
    """
    Find the location of a foreign library. Used by :py:class:`lmbrwaflib.lumberyard_sdks.read_dual_library_shared` and 
    :py:class:`lmbrwaflib.lumberyard_sdks.read_dual_library_static`.
    """
    node = None
    library_name = convert_dual_task_name_to_lib_name(self.name)

    names = [x % library_name for x in lib_patterns[self.lib_type]]
    for x in self.lib_paths + [self.path] + SYSTEM_LIB_PATHS:
        if not isinstance(x, Node.Node):
            x = self.bld.root.find_node(x) or self.path.find_node(x)
            if not x:
                continue

        for y in names:
            node = x.find_node(y)
            if node:
                node.sig = Utils.h_file(node.abspath())
                break
        else:
            continue
        break
    else:
        raise Errors.WafError('could not find library %r' % self.name)
    self.link_task = self.create_task('fake_%s' % self.lib_type, [], [node])
    if not getattr(self, 'target', None):
        self.target = library_name
    if not getattr(self, 'output_file_name', None):
        self.output_file_name = library_name

@conf
def install_internal_sdk_headers(bld, build_task_name, sdk_name, header_list):
    if isinstance(bld, BuildContext):
        try:
            task_gen = bld.get_tgen_by_name(bld.make_shared_library_task_name(build_task_name))
            if task_gen != None:
                for header in header_list:
                    task_gen.create_task('copy_outputs', 
                                         task_gen.path.make_node(build_task_name + '/include/' + header),
                                         bld.srcnode.make_node('Tools/InternalSDKs/' + sdk_name + '/include/' + header) )
        except:
            return 

def _get_gamelift_client_build_dir(self, forceReleaseDir=False):
    build_dir = 'x64' # default to x64, unless we have a better case

    platform = self.env['PLATFORM'].lower()
    # Return early for these platforms as they don't follow the same directory
    # structure or naming convention as window platforms
    if 'ios' in platform:
        build_dir = 'ios'
        return build_dir
    elif 'appletv' in platform:
        build_dir = 'appletv'
        return build_dir
    elif 'mac' in platform:
        build_dir = 'OSX'
        return build_dir
    elif 'linux' in platform:
        build_dir = 'linux'
        return build_dir

    build_dir += '/'

    config = self.env['CONFIGURATION'].lower()
    if forceReleaseDir is True:
        build_dir += 'Release'
    else:
        
        if 'debug' in config:
            build_dir += 'Debug'
        elif 'profile' in config:
            build_dir += 'Release'
        else:
            build_dir += 'Release'

    return build_dir

def _get_gamelift_server_build_dir(self, forceReleaseDir=False):
    build_dir = 'x64_v120'

    config = self.env['CONFIGURATION'].lower()
    if forceReleaseDir is True:
        build_dir += '_Release'
    else:
        if 'debug' in config:
            build_dir += '_Debug'
        elif 'profile' in config:
            build_dir += '_Release'
        else:
            build_dir += '_Release'

    return build_dir

RELEASE_GAMELIFT_X64_ARTIFACTS = [
    'Code/SDKs/GameLiftServerSDK/lib/x64_v120_Release_Dll/aws-cpp-sdk-gamelift-server.dll',
    'Code/SDKs/GameLiftServerSDK/lib/x64_v120_Release_Dll/aws-cpp-sdk-gamelift-server.pdb',
    'Code/SDKs/GameLiftSDK/bin/x64/release/aws-cpp-sdk-gamelift-client.dll',
    'Code/SDKs/GameLiftSDK/bin/x64/release/aws-cpp-sdk-gamelift-client.pdb'
]


DEBUG_GAMELIFT_X64_ARTIFACTS = [
    'Code/SDKs/GameLiftServerSDK/lib/x64_v120_Debug_Dll/aws-cpp-sdk-gamelift-server.dll',
    'Code/SDKs/GameLiftServerSDK/lib/x64_v120_Debug_Dll/aws-cpp-sdk-gamelift-server.pdb',
    'Code/SDKs/GameLiftSDK/bin/x64/debug/aws-cpp-sdk-gamelift-client.dll',
    'Code/SDKs/GameLiftSDK/bin/x64/debug/aws-cpp-sdk-gamelift-client.pdb'
]

RELEASE_GAMELIFT_LINUX_ARTIFACTS = [
    'Code/SDKs/GameLiftServerSDK/lib/linux_clang_3.4_release/libaws-cpp-sdk-gamelift-server.a'
]

DEBUG_GAMELIFT_LINUX_ARTIFACTS = [
    'Code/SDKs/GameLiftServerSDK/lib/linux_clang_3.4_debug/libaws-cpp-sdk-gamelift-server.a'
]

def BuildGameScaleLibraryList():
    return ['aws-cpp-sdk-gamelift-client',
            'aws-cpp-sdk-gamelift-server']

@conf
def RegisterGameLiftLibaries(bld):
    if isinstance(bld, BuildContext) and is_win_x64_platform(bld) :
        #client
        clientSharedLibraryDirectory = bld.CreateRootRelativePath('Code/SDKs/GameLiftSDK/bin/' + _get_gamelift_client_build_dir(bld))
        clientStaticLibraryDirectory = bld.CreateRootRelativePath('Code/SDKs/GameLiftSDK/lib/' + _get_gamelift_client_build_dir(bld))
        clientStaticsExist = os.path.exists(clientStaticLibraryDirectory)
        clientIncludeDir = bld.CreateRootRelativePath('Code/SDKs/GameLiftSDK/include')
        bld.read_dual_library_shared(
            name            = 'aws-cpp-sdk-gamelift-client',
            export_defines  = ['AWS_CUSTOM_MEMORY_MANAGEMENT', 'USE_IMPORT_EXPORT', 'BUILD_GAMELIFT_CLIENT'],
            export_includes = [clientIncludeDir],
            paths           = [clientSharedLibraryDirectory]
        )
        if clientStaticsExist:
            bld.read_dual_library_static(
                name            = 'aws-cpp-sdk-gamelift-client',
                export_defines  = ['AWS_CUSTOM_MEMORY_MANAGEMENT', 'BUILD_GAMELIFT_CLIENT'],
                export_includes = [clientIncludeDir],
                paths           = [clientStaticLibraryDirectory]
            )

        #server
        serverSharedLibraryDirectory = bld.CreateRootRelativePath('Code/SDKs/GameLiftServerSDK/lib/' + _get_gamelift_server_build_dir(bld) + '_Dll')
        serverStaticLibraryDirectory = bld.CreateRootRelativePath('Code/SDKs/GameLiftServerSDK/lib/' + _get_gamelift_server_build_dir(bld) + '_Static')
        serverStaticsExist = os.path.exists(serverStaticLibraryDirectory)
        serverIncludeDir = bld.CreateRootRelativePath('Code/SDKs/GameLiftServerSDK/include')
        bld.read_dual_library_shared(
            name            = 'aws-cpp-sdk-gamelift-server',
            export_defines  = ['AWS_CUSTOM_MEMORY_MANAGEMENT', 'USE_IMPORT_EXPORT', 'BUILD_GAMELIFT_SERVER'],
            export_includes = [serverIncludeDir],
            paths           = [serverSharedLibraryDirectory]
        )
        if serverStaticsExist:
            bld.read_dual_library_static(
                name            = 'aws-cpp-sdk-gamelift-server',
                export_defines  = ['AWS_CUSTOM_MEMORY_MANAGEMENT', 'BUILD_GAMELIFT_SERVER'],
                export_includes = [serverIncludeDir],
                paths           = [serverStaticLibraryDirectory]
            )

    if isinstance(bld, BuildContext) and is_linux_platform(bld):
        #server
        if 'debug' in bld.env["CONFIGURATION"].lower():
            config = 'debug'
        else:
            config = 'release'
 
        serverStaticLibraryDirectory = bld.CreateRootRelativePath('Code/SDKs/GameLiftServerSDK/lib/linux_clang_3.4_' + config)
        serverStaticsExist = os.path.exists(serverStaticLibraryDirectory)
        serverIncludeDir = bld.CreateRootRelativePath('Code/SDKs/GameLiftServerSDK/include')
        if serverStaticsExist:
            bld.read_dual_library_static(
                name            = 'aws-cpp-sdk-gamelift-server',
                export_defines  = ['AWS_CUSTOM_MEMORY_MANAGEMENT', 'BUILD_GAMELIFT_SERVER'],
                export_includes = [serverIncludeDir],
                paths           = [serverStaticLibraryDirectory]
            )

@feature('AWSGameLift')
@before_method('apply_incpaths')
def apply_aws_gamelift_flags(self):
    config = self.env['CONFIGURATION'].lower()
    platform = self.env['PLATFORM'].lower()
    if 'linux' in platform:
        if 'debug' in config:
            self.env['SOURCE_ARTIFACTS_INCLUDE'] += DEBUG_GAMELIFT_LINUX_ARTIFACTS
        else:
            self.env['SOURCE_ARTIFACTS_INCLUDE'] += RELEASE_GAMELIFT_LINUX_ARTIFACTS
    elif 'win' in platform:
        if 'debug' in config:
            self.env['SOURCE_ARTIFACTS_INCLUDE'] += DEBUG_GAMELIFT_X64_ARTIFACTS
        else:
            self.env['SOURCE_ARTIFACTS_INCLUDE'] += RELEASE_GAMELIFT_X64_ARTIFACTS

def _get_library_list_with_prefix(bld):
    returnList = BuildNativeSDKLibraryList()
    bldPrefix = get_platform_lib_prefix(bld)
    if bldPrefix != '':
        returnList = [bldPrefix + thisItem for thisItem in returnList]
    return returnList

def BuildNativeSDKLibraryList():
    return ['aws-cpp-sdk-core',
            'aws-cpp-sdk-cognito-identity',
            'aws-cpp-sdk-dynamodb',
            'aws-cpp-sdk-identity-management',
            'aws-cpp-sdk-lambda',
            'aws-cpp-sdk-gamelift',
            'aws-cpp-sdk-mobileanalytics',
            'aws-cpp-sdk-queues',          
            'aws-cpp-sdk-s3',
            'aws-cpp-sdk-sns', 
            'aws-cpp-sdk-sqs', 
            'aws-cpp-sdk-sts']


@conf
def BuildPlatformLibraryDirectory(bld, forceStaticLinking):
    config = bld.env['CONFIGURATION']
    platform = bld.env['PLATFORM']

    if 'debug' in config:
        configDir = 'Debug'
    else:
        configDir = 'Release'

    if forceStaticLinking:
        libDir = 'lib'
    else:
        libDir = 'bin'

    if 'ios' in platform:
        platformDir = 'ios'
    elif 'appletv' in platform:
        platformDir = 'appletv'
    elif 'darwin' in platform:
        platformDir = 'mac'
    elif 'linux' in platform:
        platformDir = 'linux/intel64'
    else:
        # TODO: add support for other platforms as versions become available
        platformDir = 'windows/intel64'
        compilerDir = None

        if bld.env['MSVC_VERSION'] == 14:
            compilerDir = 'vs2015'
        elif bld.env['MSVC_VERSION'] == 12:
            compilerDir = 'vs2013'
        
        if compilerDir != None:
            platformDir += '/{}'.format(compilerDir)

    return '{}/{}/{}'.format(libDir, platformDir, configDir)


def Build3rdPartySDKLibraryDirectory(bld, SDKName, forceStaticLinking):
    return 'Code/SDKs/{}/{}'.format(SDKName, bld.BuildPlatformLibraryDirectory(forceStaticLinking))

@conf
def RegisterAWSNativeSDKLibaries(bld):
    if isinstance(bld, BuildContext) and does_platform_support_aws_native_sdk(bld):
        useStatics = should_link_aws_native_sdk_statically(bld)
        sharedLibraryDirectory = bld.CreateRootRelativePath(Build3rdPartySDKLibraryDirectory(bld, 'AWSNativeSDK', False))
        staticLibraryDirectory = bld.CreateRootRelativePath(Build3rdPartySDKLibraryDirectory(bld, 'AWSNativeSDK', True))
        includeDir = bld.CreateRootRelativePath('Code/SDKs/AWSNativeSDK/include')

        staticsExist = os.path.exists(staticLibraryDirectory)

        for library in BuildNativeSDKLibraryList():
 
            if library == 'aws-cpp-sdk-core':
                if not useStatics:
                    bld.read_dual_library_shared(
                        name            = library,
                        export_defines  = ['AWS_CUSTOM_MEMORY_MANAGEMENT', 'USE_IMPORT_EXPORT'],
                        export_includes = [includeDir],
                        paths           = [sharedLibraryDirectory]
                    )
                if staticsExist:
                    bld.read_dual_library_static(
                        name            = library,
                        export_defines  = ['AWS_CUSTOM_MEMORY_MANAGEMENT'],
                        export_includes = [includeDir],
                        paths           = [staticLibraryDirectory]
                    )
            else:
                if not useStatics:
                    bld.read_dual_library_shared(
                        name            = library,
                        paths           = [sharedLibraryDirectory]
                    )
                if staticsExist:
                    bld.read_dual_library_static(
                        name            = library,
                        paths           = [staticLibraryDirectory]
                    )

@feature('AWSNativeSDK')
@before_method('apply_incpaths')
def copy_aws_libraries(self):
    libraryDirectory = Build3rdPartySDKLibraryDirectory(self.bld, 'AWSNativeSDK', False)
    dll_extension = get_dynamic_lib_extension(self.bld)
    lib_prefix = get_platform_lib_prefix(self.bld)
    for library in _get_library_list_with_prefix(self.bld):
        dllPath = libraryDirectory + '/' +  library + dll_extension
        pdbPath = libraryDirectory + '/' +  library + '.pdb'
        self.env['SOURCE_ARTIFACTS_INCLUDE'] += [ dllPath ]
        self.env['SOURCE_ARTIFACTS_INCLUDE'] += [ pdbPath ]

@feature('AWSNativeSDK')
@after_method('apply_monolithic_pch_objs')
def link_aws_sdk_core_after(self):
    # AWSNativeSDK has a requirement that the aws-cpp-sdk-core library be linked after other aws libraries
    # The use system make this difficult as it adds dependent libraries in a first-seen order, and this results
    # in the wrong link order for monolithic builds.
    # This function removes the library from the use system after includes are applied but before the libs are
    # calculated, and instead adds the library to the libs, which are added after the use libraries, fixing the link order
    platform = self.env['PLATFORM']
    if 'linux' in platform and 'aws-cpp-sdk-core_static' in self.use:
        self.use.remove('aws-cpp-sdk-core_static')
        if 'aws-cpp-sdk-core' not in self.env['LIB']:
            self.env['LIB'] += ['aws-cpp-sdk-core']

@feature('ExternalLyIdentity')
@before_method('apply_incpaths')
def copy_external_ly_identity(self):
    if not os.path.exists(self.bld.CreateRootRelativePath('Code/Tools/LyIdentity/wscript')):
        sharedLibraryPath = 'Tools/InternalSDKs/LyIdentity/' + self.bld.BuildPlatformLibraryDirectory(False) 
        self.env['SOURCE_ARTIFACTS_INCLUDE'] += [ sharedLibraryPath + "/LyIdentity_shared.dll" ]

@conf
def register_ly_identity_as_external(self):
    if isinstance(self, BuildContext) and is_win_x64_platform(self):
        staticLibraryPath = self.CreateRootRelativePath('Tools/InternalSDKs/LyIdentity/' + self.BuildPlatformLibraryDirectory(True))
        sharedLibraryPath = self.CreateRootRelativePath('Tools/InternalSDKs/LyIdentity/' + self.BuildPlatformLibraryDirectory(False)) 
        identityIncludeDir = self.CreateRootRelativePath('Tools/InternalSDKs/LyIdentity/include')

        self.read_shlib(
            name            = 'LyIdentity_shared',
            export_defines  = ['LINK_LY_IDENTITY_DYNAMICALLY'],
            export_includes = [identityIncludeDir],
            paths           = [ sharedLibraryPath ]
        )

        self.read_stlib(
            name            = 'LyIdentity_static',
            export_includes = [ identityIncludeDir ],
            paths           = [ staticLibraryPath ]
        )


@feature('ExternalLyMetrics')
@before_method('apply_incpaths')
def copy_external_ly_metrics(self):
    if not os.path.exists(self.bld.CreateRootRelativePath('Code/Tools/LyMetrics/wscript')):
        sharedLibraryPath = 'Tools/InternalSDKs/LyMetrics/' + self.bld.BuildPlatformLibraryDirectory(False)
        self.env['SOURCE_ARTIFACTS_INCLUDE'] += [ sharedLibraryPath + "/LyMetricsShared_shared.dll" ]
        self.env['SOURCE_ARTIFACTS_INCLUDE'] += [ sharedLibraryPath + "/LyMetricsProducer_shared.dll" ]

@conf
def register_ly_metrics_as_external(self):
    if isinstance(self, BuildContext) and is_win_x64_platform(self):
        staticLibraryPath = self.CreateRootRelativePath('Tools/InternalSDKs/LyMetrics/' + self.BuildPlatformLibraryDirectory(True))
        sharedLibraryPath = self.CreateRootRelativePath('Tools/InternalSDKs/LyMetrics/' + self.BuildPlatformLibraryDirectory(False))
        metricsSharedIncludeDir = self.CreateRootRelativePath('Tools/InternalSDKs/LyMetrics/include')
        metricsProducerIncludeDir = self.CreateRootRelativePath('Tools/InternalSDKs/LyMetrics/include')

        self.read_shlib(
            name            = 'LyMetricsShared_shared',
            export_defines  = ['LINK_LY_METRICS_DYNAMICALLY'],
            export_includes = [ metricsSharedIncludeDir ],
            paths           = [ sharedLibraryPath ]
        )

        self.read_stlib(
            name            = 'LyMetricsShared_static',
            export_includes = [ metricsSharedIncludeDir ],
            paths           = [ staticLibraryPath ]
        )

        self.read_shlib(
            name            = 'LyMetricsProducer_shared',
            export_defines  = ['LINK_LY_METRICS_PRODUCER_DYNAMICALLY'],
            export_includes = [ metricsProducerIncludeDir ],
            paths           = [ sharedLibraryPath ]
        )

        self.read_stlib(
            name            = 'LyMetricsProducer_static',
            export_includes = [ metricsProducerIncludeDir ],
            paths           = [ staticLibraryPath ]
        )


@feature('GoogleMock')
@before_method('process_use')
def apply_gtest_static_flags(self):
    # legacy - just add the gmock to the use list for everyone that uses this 'feature'.
    self.use += ['gmock']

@feature('SocialGaming')
@before_method('apply_incpaths')
def apply_socialgaming_static_flags(self):
    if is_win_x64_platform(self):
        build_variant = 'x64_v120_' + ('debug' if 'debug' in self.env['CONFIGURATION'].lower() else 'release')

        includes = [self.bld.CreateRootRelativePath('Code/SDKs/dyad/src')]
        libs = ['libdyad']
        libpath = [self.bld.CreateRootRelativePath('Code/SDKs/dyad/lib/' + build_variant)]

        self.includes = includes + self.includes
        self.env['LIB'] += libs
        self.env['LIBPATH'] += libpath

    if 'linux' in self.env['PLATFORM']:
        build_variant = 'linux_' + ('debug' if 'debug' in self.env['CONFIGURATION'].lower() else 'release')

        includes = [self.bld.CreateRootRelativePath('Code/SDKs/dyad/src')]
        libs = ['dyad']
        libpath = [self.bld.CreateRootRelativePath('Code/SDKs/dyad/lib/' + build_variant)]

        self.includes = includes + self.includes
        self.env['LIB'] += libs
        self.env['LIBPATH'] += libpath

@feature('EmbeddedPython')
@before_method('apply_incpaths')
def enable_embedded_python(self):

    # Only win_x64 builds support embedding Python.
    #
    # We assume 'project_generator' (the result of doing a lmbr_waf configure) is
    # also for a win_x64 build so that the include directory is configured property
    # for Visual Studio.

    platform = self.env['PLATFORM'].lower()
    config = self.env['CONFIGURATION'].lower()

    if platform in ['win_x64', 'win_x64_test', 'linux_x64', 'project_generator']:

        # Set the USE_DEBUG_PYTHON environment variable to the location of 
        # a debug build of python if you want to use one for debug builds.
        #
        # This does NOT work with the boost python helpers, which are used
        # by the Editor, since the boost headers always undef _DEBUG before 
        # including the python headers.
        #
        # Any code that includes python headers directly should also undef 
        # _DEBUG before including those headers except for when USE_DEBUG_PYTHON 
        # is defined.

        if 'debug' in config and 'USE_DEBUG_PYTHON' in os.environ:
            
            python_home = os.environ['USE_DEBUG_PYTHON']
            python_dll = '{}/python27_d.dll'.format(python_home)

            self.env['DEFINES'] += ['USE_DEBUG_PYTHON']

        else:

            python_version = '2.7.11'
            if platform == 'linux_x64':
                python_variant = 'linux_x64'
                python_libs = 'lib'
                python_dll_name = 'lib/libpython2.7.so'
            else:
                python_variant = 'windows'
                python_libs = 'libs'
                python_dll_name = 'python27.dll'
            python_home = os.path.join('Tools', 'Python', python_version, python_variant)
            python_dll = os.path.join(python_home, python_dll_name)

        python_include_dir = os.path.join(python_home, 'include')

        if platform == 'linux_x64':
            python_include_dir = os.path.join(python_include_dir, 'python2.7')

        python_libs_dir = os.path.join(python_home, python_libs)

        self.includes += [self.bld.CreateRootRelativePath(python_include_dir)]
        self.env['LIBPATH'] += [self.bld.CreateRootRelativePath(python_libs_dir)]
        self.env['DEFINES'] += ['DEFAULT_LY_PYTHONHOME="@root@/{}"'.format(python_home.replace('\\', '\\\\'))] # need to escape backslashes for Windows

        # Ideally we would load python27.dll from the python home directory. The best way
        # to do that may be to delay load python27.dll and use SetDllDirectory to insert
        # the python home directory into the DLL search path. However that doesn't work 
        # because the boost python helpers import a data symbol.
        self.env['SOURCE_ARTIFACTS_INCLUDE'] += [python_dll]

    if platform in ['darwin_x64']:
        python_home = '/System/Library/Frameworks/Python.framework/Versions/2.7'

        python_include_dir = '{}/include/python2.7'.format(python_home)
        python_lib_dir = '{}/lib/python2.7'.format(python_home)

        # TODO: figure out what needs to be set for OSX builds.
        self.includes += [python_include_dir]
        self.env['LIBPATH'] += [python_lib_dir]
