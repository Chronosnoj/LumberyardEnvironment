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

def load_clang_common_settings(v):
    """
    Setup all compiler/linker flags with are shared over all targets using the clang compiler
    
    !!! But not the actual compiler, since the compiler depends on the target !!!
    """
    
    # AR Tools
    v['ARFLAGS'] = 'rcs'
    v['AR_TGT_F'] = ''
    
    # CC/CXX Compiler   
    v['CC_NAME']    = v['CXX_NAME']     = 'clang' 
    v['CC_SRC_F']   = v['CXX_SRC_F']    = []
    v['CC_TGT_F']   = v['CXX_TGT_F']    = ['-c', '-o']
    
    v['CPPPATH_ST']     = '-I%s'
    v['DEFINES_ST']     = '-D%s'
    
    v['ARCH_ST']        = ['-arch']
    
    # Linker
    v['CCLNK_SRC_F'] = v['CXXLNK_SRC_F'] = []
    v['CCLNK_TGT_F'] = v['CXXLNK_TGT_F'] = '-o'
    
    v['LIB_ST']         = '-l%s'
    v['LIBPATH_ST']     = '-L%s'
    v['STLIB_ST']       = '-l%s'
    v['STLIBPATH_ST']   = '-L%s'
    
    v['RPATH_ST']       = '-Wl,-rpath,%s'

    # shared library settings   
    v['CFLAGS_cshlib'] = v['CFLAGS_cxxshlib']       = ['-fpic'] 
    v['CXXFLAGS_cshlib'] = v['CXXFLAGS_cxxshlib']   = ['-fpic']
        
    v['LINKFLAGS_cshlib']   = ['-shared']
    v['LINKFLAGS_cxxshlib'] = ['-shared']
    
    # static library settings   
    v['CFLAGS_cstlib'] = v['CFLAGS_cxxstlib']   = []     
    v['CXXFLAGS_cstlib'] = v['CXXFLAGS_cxxstlib']   = []
    
    v['LINKFLAGS_cxxstlib'] = ['-Wl,-Bstatic']
    v['LINKFLAGS_cxxshtib'] = ['-Wl,-Bstatic']
    
    # Set common compiler flags 
    COMMON_COMPILER_FLAGS = [
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
        '-Wno-deprecated-declarations',
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
        '-Wno-unknown-pragmas',
        '-Wno-unused-function',
        '-Wno-unused-private-field',
        '-Wno-unused-value',
        '-Wno-unused-variable',
        '-Wno-non-pod-varargs',
        
        # Other
        '-ffast-math',
        '-fvisibility=hidden',
        '-fvisibility-inlines-hidden',
        ]
        
    # Copy common flags to prevent modifing references
    v['CFLAGS'] += COMMON_COMPILER_FLAGS[:]
    
    v['CXXFLAGS'] += COMMON_COMPILER_FLAGS[:] + [
        '-std=c++1y',               # C++14
        '-fno-rtti',                # Disable RTTI
        '-fno-exceptions',          # Disable Exceptions
    ]
    
    # Linker Flags
    v['LINKFLAGS'] += []    
    
    # Doesn't work on Mac? v['SHLIB_MARKER']    = '-Wl,-Bdynamic'
    v['SONAME_ST']      = '-Wl,-h,%s'
    # Doesn't work on Mac? v['STLIB_MARKER']    = '-Wl,-Bstatic'
    
    # Compile options appended if compiler optimization is disabled
    v['COMPILER_FLAGS_DisableOptimization'] = [ '-O0' ] 
    
    # Compile options appended if debug symbols are generated   
    v['COMPILER_FLAGS_DebugSymbols'] = [ '-g' ] 
    
    # Linker flags when building with debug symbols
    v['LINKFLAGS_DebugSymbols'] = []
    
    # Store settings for show includes option
    v['SHOWINCLUDES_cflags'] = ['-H']
    v['SHOWINCLUDES_cxxflags'] = ['-H']
    
    # Store settings for preprocess to file option
    v['PREPROCESS_cflags'] = ['-E', '-dD']
    v['PREPROCESS_cxxflags'] = ['-E', '-dD']
    v['PREPROCESS_cc_tgt_f'] = ['-o']
    v['PREPROCESS_cxx_tgt_f'] = ['-o']
    
    # Store settings for preprocess to file option
    v['DISASSEMBLY_cflags'] = ['-S', '-fverbose-asm']
    v['DISASSEMBLY_cxxflags'] = ['-S', '-fverbose-asm']
    v['DISASSEMBLY_cc_tgt_f'] = ['-o']
    v['DISASSEMBLY_cxx_tgt_f'] = ['-o']
    
@conf
def load_debug_clang_settings(conf):
    """
    Setup all compiler/linker flags with are shared over all targets using the clang compiler
    for the "debug" configuration   
    """
    v = conf.env    
    load_clang_common_settings(v)
    
    COMPILER_FLAGS = [
        '-O0',      # No optimization
        '-g',       # debug symbols
        '-fno-inline', # don't inline functions
        '-fstack-protector' # Add additional checks to catch stack corruption issues
        ]
    
    v['CFLAGS'] += COMPILER_FLAGS
    v['CXXFLAGS'] += COMPILER_FLAGS
    
@conf
def load_profile_clang_settings(conf):
    """
    Setup all compiler/linker flags with are shared over all targets using the clang compiler
    for the "profile" configuration 
    """
    v = conf.env
    load_clang_common_settings(v)
    
    COMPILER_FLAGS = [
        '-O2',
        ]
    
    v['CFLAGS'] += COMPILER_FLAGS
    v['CXXFLAGS'] += COMPILER_FLAGS 
    
@conf
def load_performance_clang_settings(conf):
    """
    Setup all compiler/linker flags with are shared over all targets using the clang compiler
    for the "performance" configuration 
    """
    v = conf.env
    load_clang_common_settings(v)
    
    COMPILER_FLAGS = [
        '-O2',
        ]
    
    v['CFLAGS'] += COMPILER_FLAGS
    v['CXXFLAGS'] += COMPILER_FLAGS
    
@conf
def load_release_clang_settings(conf):  
    """
    Setup all compiler/linker flags with are shared over all targets using the clang compiler
    for the "release" configuration 
    """
    v = conf.env
    load_clang_common_settings(v)
    
    COMPILER_FLAGS = [
        '-O2',
        ]
    
    v['CFLAGS'] += COMPILER_FLAGS
    v['CXXFLAGS'] += COMPILER_FLAGS 
