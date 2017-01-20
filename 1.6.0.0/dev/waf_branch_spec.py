########################################################################################
# All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
# its licensors.
#
# For complete copyright and license terms please see the LICENSE at the root of this
# distribution (the "License"). All use of this software is governed by the License,
# or, if provided, by the license below or the license accompanying this file. Do not
# remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#
########################################################################################

# Central configuration file for a branch, should never be integrated since it is unique for each branch
import re

######################
## Build Layout
BINTEMP_FOLDER = 'BinTemp'
SOLUTION_FOLDER = 'Solutions'
SOLUTION_NAME = 'Lumberyard'

######################
## Build Configuration
COMPANY_NAME = 'Amazon.com, Inc.'
COPYRIGHT_AMAZON = 'Copyright (c) Amazon.com, Inc. or its affiliates'
COPYRIGHT_CRYTEK = 'Copyright (c) Amazon.com, Inc., its affiliates or its licensors'
LUMBERYARD_VERSION = '1.6.0.0'
LUMBERYARD_BUILD = 306493

# validate the Lumberyard version string above
VERSION_NUMBER_PATTERN = re.compile("^(\.?\d+)*$")
if VERSION_NUMBER_PATTERN.match(LUMBERYARD_VERSION) is None:
    raise ValueError('Invalid version string for the Lumberyard Version ({})'.format(LUMBERYARD_VERSION))

######################
## Supported branch platforms/configurations
## This is a map of host -> target platforms
PLATFORMS = {
    'darwin': [
        'darwin_x64',
        'android_armv7_gcc',
        'android_armv7_clang',
        'ios',
        'appletv'
    ],
    'win32' : [
        'win_x64',
        
        
        
        
        'android_armv7_gcc',
        'android_armv7_clang'
    ],
    'linux': [
        'linux_x64'
    ]
}

# To handle platform aliases that can map to one or more actual platforms (Above).
# Make sure to maintain and update the alias based on updates to the PLATFORMS
# dictionary
PLATFORM_SHORTCUT_ALIASES = {
    'win': [
        'win_x64',
        
        
    ],
    'darwin': [
        'darwin_x64'
    ],
    'android': [
        'android_armv7_gcc',
        'android_armv7_clang'
    ],
    'linux': [
        'linux_x64'
    ],
    'all' : [
        'darwin_x64',
        'ios',
        'appletv',
        'win_x64',
        
        
        
        
        'android_armv7_gcc',
        'android_armv7_clang'
    ]
}

# Keep the aliases in sync
for platform_alias_list in PLATFORM_SHORTCUT_ALIASES.values():
    for platform_alias in platform_alias_list:
        found = False
        for platform_list in PLATFORMS.values():
            for platform in platform_list:
                if platform_alias == platform:
                    found = True
                    break
            if found:
                break
        if not found:
            raise ValueError("Invalid platform alias {} found in waf_branch_spec.py".format(platform_alias))

######################
## Only allow specific build platforms to be included in specific versions of MSVS since not all target platforms are
## # compatible with all MSVS version
MSVS_PLATFORM_VERSION = {
    'win_x64'               : [ '2013', '14' ],
    
    
    
    
    'android_armv7_clang'   : [ '2013','14' ]
}


WAF_PLATFORM_TO_VS_PLATFORM_PREFIX_AND_TOOL_SET_DICT = {
    'win_x86'               :   ('Win32', 'win', ['v120']),
    'win_x64'               :   ('x64', 'win', ['v120']),
    
    
    
    
    'android_armv7_clang'   :   ('ARM', 'android', ['v120']),
}


# Helper method to reverse the waf platform to vs platform and prefix dict (WAF_PLATFORM_TO_VS_PLATFORM_AND_PREFIX_DICT)
# to a vs to waf platform and prefix dict dictionary.
def _waf_platform_dict_to_vs_dict(src_dict):

    dict_result = {}
    src_keys = src_dict.viewkeys()

    for src_key in src_keys:
        src_value_tuple = src_dict[src_key]

        if not isinstance(src_value_tuple, tuple):
            raise ValueError('Value of key entry {} must be a tuple'.format(src_key))

        src_value = src_value_tuple[0]
        if not isinstance(src_value, str):
            raise ValueError('Tuple value(0) of key entry {} must be a string'.format(src_key))
        prefix_value = src_value_tuple[1]
        if not isinstance(prefix_value, str):
            raise ValueError('Tuple value(1) of key entry {} must be a string'.format(src_key))
        toolset_value = src_value_tuple[2]
        if not isinstance(toolset_value, list):
            raise ValueError('Tuple value(2) of key entry {} must be a list'.format(src_key))

        if src_value in dict_result:
            raise ValueError('Duplicate value of {} found in source dictionary'.format(src_value))
        dict_result[src_value] = (src_key, prefix_value, list(toolset_value))

    return dict_result


VS_PLATFORM_TO_WAF_PLATFORM_PREFIX_AND_TOOL_SET_DICT = _waf_platform_dict_to_vs_dict(WAF_PLATFORM_TO_VS_PLATFORM_PREFIX_AND_TOOL_SET_DICT)


# list of build configurations to generate for each supported platform
CONFIGURATIONS = [ 'debug',           'profile',           'performance',           'release',
                   'debug_dedicated', 'profile_dedicated', 'performance_dedicated', 'release_dedicated',
                   'debug_test',      'profile_test',
                   'debug_test_dedicated', 'profile_test_dedicated']

# To handle configurations aliases that can map to one or more actual configurations (Above).
# Make sure to maintain and update the alias based on updates to the CONFIGURATIONS
# dictionary
CONFIGURATION_SHORTCUT_ALIASES = {
    'all': CONFIGURATIONS,
    'debug_all': ['debug', 'debug_dedicated', 'debug_test', 'debug_test_dedicated'],
    'profile_all': ['profile', 'profile_dedicated', 'profile_test', 'profile_test_dedicated'],
    'performance_all': ['performance', 'performance_dedicated'],
    'release_all': ['release', 'release_dedicated'],
    'dedicated_all': ['debug_dedicated', 'profile_dedicated', 'performance_dedicated', 'release_dedicated'],
    'non_dedicated': ['debug', 'profile', 'performance', 'release'],
    'test_all': ['debug_test', 'debug_test_dedicated', 'profile_test', 'profile_test_dedicated'],
    'non_test': ['debug', 'debug_dedicated', 'profile', 'profile_dedicated', 'performance', 'performance_dedicated',
                 'release', 'release_dedicated']
}

# Keep the aliases in sync
for configuration_alias_key in CONFIGURATION_SHORTCUT_ALIASES:
    if configuration_alias_key in CONFIGURATIONS:
        raise ValueError("Invalid configuration shortcut alias '{}' in waf_branch_spec.py. Duplicates an existing configuration.".format(configuration_alias_key))
    configuration_alias_list = CONFIGURATION_SHORTCUT_ALIASES[configuration_alias_key]
    for configuration_alias in configuration_alias_list:
        if configuration_alias not in configuration_alias:
            raise ValueError("Invalid configuration '{}' for configuration shortcut alias '{}' in waf_branch_spec.py".format(configuration_alias,configuration_alias_key))

# build/clean commands are generated using PLATFORM_CONFIGURATION for all platform/configuration
# not all platform/configuration commands are valid.
# if an entry exists in this dictionary, only the configurations listed will be built
PLATFORM_CONFIGURATION_FILTER = {
    # Remove these as testing comes online for each platform
    platform : CONFIGURATION_SHORTCUT_ALIASES['non_test'] for platform in ('android_armv7_gcc', 'android_armv7_clang', 
        'ios', 'appletv',   'linux_x64_gcc', 'linux_x64_clang')
}

## what conditions do you want a monolithic build ?  Uses the same matching rules as other settings
## so it can be platform_configuration, or configuration, or just platform for the keys, and the Value is assumed
## false by default.
## monolithic builds produce just a statically linked executable with no dlls.

MONOLITHIC_BUILDS = [
        'win_x64_release',
        'release_dedicated',
        'performance_dedicated',
        'performance',
        'ios',
        'appletv',
        
        'darwin_release',
        'durango_release'
        ]

## List of available launchers by spec module
AVAILABLE_LAUNCHERS = {
    'modules':
        [
            'WindowsLauncher',
            
            
            'MacLauncher',
            'DedicatedLauncher',
            'IOSLauncher',
            'AppleTVLauncher',
            'AndroidLauncher',
            'LinuxLauncher'
        ]
    }

## List of projects that originated from crytek
## Note: Make sure to maintain this list if new modules are integrated from crytek
CRYTEK_ORIGINAL_PROJECTS = ["Cry3DEngine",
                            "CryAction",
                            "CryAISystem",
                            "CryAnimation",
                            "CryCommon",
                            "CryEntitySystem",
                            "CryFont",
                            "CryInput",
                            "CryLiveCreate",
                            "CryMovie",
                            "CryNetwork",
                            "CryPhysics",
                            "CryScriptSystem",
                            "CrySoundSystem",
                            "CryAudioImplWwise",
                            "CryAudioCommon",
                            "CrySystem",
                            "CryUnit",
                            "CryRenderD3D11",
                            "CryRenderGL",
                            "CryRenderNULL",
                            "Editor",
                            "AssetTagging",
                            "EditorAudioControlsEditor",
                            "EditorAudioControlsEditorCommon",
                            "EditorAudioControlsEditorWwise",
                            "EditorCommon",
                            "EditorAnimation",
                            "EditorFbxImport",
                            "CryDesigner",
                            "FFMPEGPlugin",
                            "FBXPlugin",
                            "PerforcePlugin",
                            "EditorModularBehaviorTree",
                            "AssetTaggingTools",
                            "CrySCompileServer",
                            
                            
                            
                            
                            "CryPerforce",
                            "CryPhysicsRC",
                            "CryXML",
                            "ResourceCompiler",
                            "ResourceCompilerABC",
                            "ResourceCompilerFBX",
                            "ResourceCompilerImage",
                            "ResourceCompilerPC",
                            "ResourceCompilerXML",
                            "DXOrbisShaderCompiler",
                            "DBAPI",
                            "PRT",
                            "ShaderCacheGen"
]