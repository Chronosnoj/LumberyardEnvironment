#! /usr/bin/env python
# encoding: utf-8

# Modifications copyright Amazon.com, Inc. or its affiliates.

"""
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

3. The name of the author may not be used to endorse or promote products
   derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
"""

"""
To add this tool to your project:
def options(conf):
	opt.load('msvs')

It can be a good idea to add the sync_exec tool too.

To generate solution files:
$ waf configure msvs

To customize the outputs, provide subclasses in your wscript files:

from waflib.extras import msvs
class vsnode_target(msvs.vsnode_target):
	def get_build_command(self, props):
		# likely to be required
		return "waf.bat build"
	def collect_source(self):
		# likely to be required
		...
class msvs_bar(msvs.msvs_generator):
	def init(self):
		msvs.msvs_generator.init(self)
		self.vsnode_target = vsnode_target

The msvs class re-uses the same build() function for reading the targets (task generators),
you may therefore specify msvs settings on the context object:

def build(bld):
	bld.solution_name = 'foo.sln'
	bld.waf_command = 'waf.bat'
	bld.projects_dir = bld.srcnode.make_node('foo.depproj')
	bld.projects_dir.mkdir()

For visual studio 2008, the command is called 'msvs2008', and the classes
such as vsnode_target are wrapped by a decorator class 'wrap_2008' to
provide special functionality.

ASSUMPTIONS:
* a project can be either a directory or a target, vcxproj files are written only for targets that have source files
* each project is a vcxproj file, therefore the project uuid needs only to be a hash of the absolute path
"""

import os, re, sys, errno, datetime, time
import uuid # requires python 2.5
from waflib.Build import BuildContext
from waflib import Utils, TaskGen, Logs, Task, Context, Node, Options
from waflib.Configure import conf
from cry_utils import append_to_unique_list
from waf_branch_spec import WAF_PLATFORM_TO_VS_PLATFORM_PREFIX_AND_TOOL_SET_DICT,VS_PLATFORM_TO_WAF_PLATFORM_PREFIX_AND_TOOL_SET_DICT,PLATFORM_CONFIGURATION_FILTER


import mscv_helper, cry_utils

HEADERS_GLOB = '**/(*.h|*.hpp|*.H|*.inl|*.hxx)'

PROJECT_TEMPLATE = r'''<?xml version="1.0" encoding="UTF-8"?>
<Project DefaultTargets="Build" ToolsVersion="14.0"
	xmlns="http://schemas.microsoft.com/developer/msbuild/2003">

	<ItemGroup Label="ProjectConfigurations">
		${for b in project.build_properties}
		<ProjectConfiguration Include="${b.configuration}|${b.platform}">
			<Configuration>${b.configuration}</Configuration>
			<Platform>${b.platform}</Platform>
		</ProjectConfiguration>
		${endfor}
	</ItemGroup>

	<PropertyGroup Label="Globals">
		<ProjectGuid>{${project.uuid}}</ProjectGuid>
		<Keyword>MakefileProj</Keyword>
		<ProjectName>${project.name}</ProjectName>
	</PropertyGroup>
	<Import Project="$(VCTargetsPath)\Microsoft.Cpp.Default.props" />

	${for b in project.build_properties}
	<PropertyGroup Condition="'$(Configuration)|$(Platform)'=='${b.configuration}|${b.platform}'" Label="Configuration">
		<ConfigurationType>Makefile</ConfigurationType>
		<OutDir>${b.outdir}</OutDir>
		${if b.platform == 'ARM'}
		<Keyword>Android</Keyword>
		<PlatformToolset>Gcc_4_9</PlatformToolset>
		<ApplicationType>Android</ApplicationType>
		<AndroidAPILevel>android-21</AndroidAPILevel>
		${else}
		<PlatformToolset>${project.get_platform_toolset(b.platform)}</PlatformToolset>
		${endif}
	</PropertyGroup>
	${endfor}
	
	<Import Project="$(VCTargetsPath)\Microsoft.Cpp.props" />
	<ImportGroup Label="ExtensionSettings">
	</ImportGroup>

	<ImportGroup Label="PropertySheets" Condition="${project.get_all_config_platform_conditional()}">
		<Import Project="$(MSBuildProjectDirectory)\$(MSBuildProjectName).vcxproj.default.props" Condition="exists('$(MSBuildProjectDirectory)\$(MSBuildProjectName).vcxproj.default.props')"/>
		<Import Project="$(UserRootDir)\Microsoft.Cpp.$(Platform).user.props" Condition="exists('$(UserRootDir)\Microsoft.Cpp.$(Platform).user.props')" Label="LocalAppDataPlatform" />
	</ImportGroup>

	${for b in project.build_properties}
	<PropertyGroup Condition="'$(Configuration)|$(Platform)'=='${b.configuration}|${b.platform}'">
		<NMakeBuildCommandLine>${xml:project.get_build_command(b)}</NMakeBuildCommandLine>
		<NMakeReBuildCommandLine>${xml:project.get_rebuild_command(b)}</NMakeReBuildCommandLine>
		<NMakeCleanCommandLine>${xml:project.get_clean_command(b)}</NMakeCleanCommandLine>
		<NMakeIncludeSearchPath>${xml:b.includes_search_path}</NMakeIncludeSearchPath>
		<NMakePreprocessorDefinitions>${xml:b.preprocessor_definitions};$(NMakePreprocessorDefinitions)</NMakePreprocessorDefinitions>
		<IncludePath>${xml:b.includes_search_path}</IncludePath>
		${if getattr(b, 'output_file', None)}
		<NMakeOutput>${xml:b.output_file}</NMakeOutput>
		<ExecutablePath>${xml:b.output_file}</ExecutablePath>
		${endif}
		${if not getattr(b, 'output_file', None)}
		<NMakeOutput>not_supported</NMakeOutput>
		<ExecutablePath>not_supported</ExecutablePath>
		${endif}
		${if getattr(b, 'deploy_dir', None)}
		<RemoteRoot>${xml:b.deploy_dir}</RemoteRoot>
		${endif}
		${if b.platform == 'Durango'}
		${if 'Debug' in b.configuration}
		<OutDir>${xml:project.get_output_folder('durango','Debug')}</OutDir>
		${else}
		<OutDir>${xml:project.get_output_folder('durango','')}</OutDir>
		${endif}
		<LayoutDir>${xml:project.get_layout_folder(b.platform)}</LayoutDir>
		<LayoutExtensionFilter>*.ilk;*.exp;*.lib;*.winmd;*.appxrecipe;*.pri</LayoutExtensionFilter>
		${if getattr(b, 'output_file_name', None)}
		<TargetName>${b.output_file_name}</TargetName>
		${endif}
		${endif}
		${if b.platform == 'ORBIS'}
		${if b.configuration == 'Debug'}
		<OutDir>${xml:project.get_output_folder('orbis','Debug')}</OutDir>
		${else}
		<OutDir>${xml:project.get_output_folder('orbis','')}</OutDir>
		${endif}
		${endif}
		${if b.platform == 'Linux X64 GCC'}
		${if b.configuration == 'Debug'}
		<OutDir>${xml:project.get_output_folder('linux_x64_gcc','Debug')}</OutDir>
		${else}
		<OutDir>${xml:project.get_output_folder('linux_x64_gcc','')}</OutDir>
		${endif}
		${endif}
	</PropertyGroup>
	${endfor}
	
	${for b in project.build_properties}
		${if getattr(b, 'deploy_dir', None)}
	<ItemDefinitionGroup Condition="'$(Configuration)|$(Platform)'=='${b.configuration}|${b.platform}'">
		<Deploy>
			<DeploymentType>CopyToHardDrive</DeploymentType>
		</Deploy>
	</ItemDefinitionGroup>
		${endif}
	${endfor}

	<ItemGroup>
		${for x in project.source}
		<${project.get_key(x)} Include='${x.abspath()}' />
		${endfor}
	</ItemGroup>

	<Import Project="$(VCTargetsPath)\Microsoft.Cpp.targets" />
	<Import Project="$(MSBuildProjectDirectory)\..\..\_WAF_\msbuild\waf_build.targets" />
	
	<ImportGroup Label="ExtensionTargets">
	</ImportGroup>
</Project>
'''
#Note:
#<LocalDebuggerWorkingDirectory></LocalDebuggerWorkingDirectory> is not a mistake, do not remove this as this overrides the inherited value
PROJECT_USER_TEMPLATE = '''<?xml version="1.0" encoding="UTF-8"?>
<Project ToolsVersion="4.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
	${for b in project.build_properties}
	${if b.platform == 'ORBIS'}
	<PropertyGroup Condition="'$(Configuration)|$(Platform)'=='${b.configuration}|${b.platform}'">
	<LocalDebuggerWorkingDirectory></LocalDebuggerWorkingDirectory>
	<LocalMappingFile>$(SolutionDir)..\config.ps4path</LocalMappingFile>
	</PropertyGroup>
	${endif}
	${if b.platform == 'Durango'}
	<PropertyGroup Condition="'$(Configuration)|$(Platform)'=='${b.configuration}|${b.platform}'">
	<DeployMode>External</DeployMode>
	</PropertyGroup>
	${endif}
	${endfor}
</Project>
'''

FILTER_TEMPLATE = '''<?xml version="1.0" encoding="UTF-8"?>
<Project ToolsVersion="4.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
	<ItemGroup>
		${for x in project.source}
			<${project.get_key(x)} Include="${x.abspath()}">
			${if project.get_filter_name(x) != '.'}
				<Filter>${project.get_filter_name(x)}</Filter>
			${endif}
			</${project.get_key(x)}>
		${endfor}
	</ItemGroup>
	<ItemGroup>
		${for x in project.dirs()}
			<Filter Include="${x}">
				<UniqueIdentifier>{${project.make_uuid(x)}}</UniqueIdentifier>
			</Filter>
		${endfor}
	</ItemGroup>
</Project>
'''

# Note:
# Please mirror any changes in the solution template in the msvs_override_handling.py | get_solution_overrides()
# This also include format changes such as spaces

SOLUTION_TEMPLATE = '''Microsoft Visual Studio Solution File, Format Version ${project.numver}
# Visual Studio ${project.vsver}
${for p in project.all_projects}
Project("{${p.ptype()}}") = "${p.name}", "${p.title}", "{${p.uuid}}"
	${if p != project.waf_project}
	ProjectSection(ProjectDependencies) = postProject
		{${project.waf_project.uuid}} = {${project.waf_project.uuid}}
	EndProjectSection
	${endif}
EndProject${endfor}
Global
	GlobalSection(SolutionConfigurationPlatforms) = preSolution
		${if project.all_projects}
		${for (configuration, platform) in project.all_projects[0].ctx.project_configurations()}
		${configuration}|${platform} = ${configuration}|${platform}
		${endfor}
		${endif}
	EndGlobalSection
	GlobalSection(ProjectConfigurationPlatforms) = postSolution
		${for p in project.all_projects}
			${if hasattr(p, 'source')}
			${for b in p.build_properties}
				{${p.uuid}}.${b.configuration}|${b.platform}.ActiveCfg = ${b.configuration}|${b.platform}
				${if getattr(p, 'is_active', None)}
					{${p.uuid}}.${b.configuration}|${b.platform}.Build.0 = ${b.configuration}|${b.platform}
				${endif}
				${if getattr(p, 'is_deploy', None) and b.platform.lower() in p.is_deploy}
				{${p.uuid}}.${b.configuration}|${b.platform}.Deploy.0 = ${b.configuration}|${b.platform}
				${endif}
			${endfor}
			${endif}
		${endfor}
	EndGlobalSection
	GlobalSection(SolutionProperties) = preSolution
		HideSolutionNode = FALSE
	EndGlobalSection
	GlobalSection(NestedProjects) = preSolution
	${for p in project.all_projects}
		${if p.parent}
		{${p.uuid}} = {${p.parent.uuid}}		
		${endif}
	${endfor}
	EndGlobalSection
EndGlobal
'''

PROPERTY_SHEET_TEMPLATE = r'''<?xml version="1.0" encoding="UTF-8"?>
<Project DefaultTargets="Build" ToolsVersion="4.0"
	xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
	
	${for b in project.build_properties}	
	${if getattr(b, 'output_file_name', None)}
	<PropertyGroup Condition="'$(Configuration)|$(Platform)'=='${b.configuration}|${b.platform}'">
		<WAF_TargetFile>${xml:b.output_file}</WAF_TargetFile>
		<TargetPath Condition="'$(WAF_TargetFile)' != ''">$(WAF_TargetFile)</TargetPath>
		<LocalDebuggerCommand Condition="'$(LocalDebuggerCommand)'==''">$(TargetPath)</LocalDebuggerCommand>
		<LocalDebuggerWorkingDirectory>$(OutDir)</LocalDebuggerWorkingDirectory>
	</PropertyGroup>
	${endif}
	${endfor}
	
	${for b in project.build_properties}	
	<ItemDefinitionGroup Condition="'$(Configuration)|$(Platform)'=='${b.configuration}|${b.platform}'">
		<ClCompile>
		
			<WAF_SingleCompilationMode>Code</WAF_SingleCompilationMode>
			<WAF_TargetSolution>${xml:project.ctx.get_solution_node().abspath()} </WAF_TargetSolution>
			
			${if getattr(b, 'target_spec', None)}
			<WAF_TargetSpec>${xml:b.target_spec}</WAF_TargetSpec>
			${endif}		
			
			${if getattr(b, 'target_config', None)}
			<WAF_TargetConfig>${xml:b.target_config}</WAF_TargetConfig>
			${endif}		
		
			${if getattr(b, 'output_file_name', None)}
			<WAF_TargetName>${xml:b.output_file_name}</WAF_TargetName>
			${endif}
			
			${if getattr(b, 'output_file', None)}
			<WAF_TargetFile>${xml:b.output_file} </WAF_TargetFile>
			${endif}
			
			${if getattr(b, 'includes_search_path', None)}
			<WAF_IncludeDirectories>${xml:b.includes_search_path}</WAF_IncludeDirectories>
			${endif}
			
			${if getattr(b, 'preprocessor_definitions', None)}
			<WAF_PreprocessorDefinitions>${xml:b.preprocessor_definitions}</WAF_PreprocessorDefinitions>
			${endif}
			
			${if getattr(b, 'deploy_dir', None)}
			<WAF_DeployDir>${xml:b.deploy_dir}</WAF_DeployDir>
			${endif}
			
			${if getattr(b, 'output_dir', None)}
			<WAF_OutputDir>${xml:b.output_dir}</WAF_OutputDir>
			${endif}
			
			${if getattr(b, 'layout_dir', None)}
			<LayoutDir>${xml:b.deploy_dir}</LayoutDir>
			${endif}
			
			${if getattr(b, 'layout_extension_filter', None)}
			<LayoutExtensionFilter>${xml:b.deploy_dir}</LayoutExtensionFilter>
			${endif}
						
			${if getattr(b, 'c_flags', None)}
			<WAF_CompilerOptions_C>${xml:b.c_flags} </WAF_CompilerOptions_C>
			${endif}
			
			${if getattr(b, 'cxx_flags', None)}
			<WAF_CompilerOptions_CXX>${xml:b.cxx_flags} </WAF_CompilerOptions_CXX>
			${endif}
			
			${if getattr(b, 'link_flags', None)}
			<WAF_LinkerOptions>${xml:b.link_flags}</WAF_LinkerOptions>
			${endif}
			
			<WAF_DisableCompilerOptimization>false</WAF_DisableCompilerOptimization>
			<WAF_ExcludeFromUberFile>false</WAF_ExcludeFromUberFile>

			<WAF_BuildCommandLine>${xml:project.get_build_command(b)}</WAF_BuildCommandLine>
			<WAF_RebuildCommandLine>${xml:project.get_rebuild_command(b)}</WAF_RebuildCommandLine>
			<WAF_CleanCommandLine>${xml:project.get_clean_command(b)}</WAF_CleanCommandLine>

		</ClCompile>
		<Deploy>
			<DeploymentType>CopyToHardDrive</DeploymentType>
		</Deploy>
	</ItemDefinitionGroup>

	${endfor}
		
	<ItemDefinitionGroup />		
	<ItemGroup />
	
</Project>
'''

COMPILE_TEMPLATE = '''def f(project):
	lst = []
	def xml_escape(value):
		return value.replace("&", "&amp;").replace('"', "&quot;").replace("'", "&apos;").replace("<", "&lt;").replace(">", "&gt;")

	%s

	#f = open('cmd.txt', 'w')
	#f.write(str(lst))
	#f.close()
	return ''.join(lst)
'''


@conf
def convert_waf_platform_to_vs_platform(self, waf_platform):

    if waf_platform not in WAF_PLATFORM_TO_VS_PLATFORM_PREFIX_AND_TOOL_SET_DICT:
        Logs.error('Invalid platform error for waf platform {}:  Platform not found'.format(waf_platform))
        return 'UNKNOWN'
    else:
        vs_platform = WAF_PLATFORM_TO_VS_PLATFORM_PREFIX_AND_TOOL_SET_DICT[waf_platform][0]
        return vs_platform


@conf
def convert_vs_platform_to_waf_platform(self, vs_platform):

    if vs_platform not in VS_PLATFORM_TO_WAF_PLATFORM_PREFIX_AND_TOOL_SET_DICT:
        Logs.error('Invalid platform error for waf platform {}:  Platform not found'.format(vs_platform))
        return 'UNKNOWN'
    else:
        waf_platform = VS_PLATFORM_TO_WAF_PLATFORM_PREFIX_AND_TOOL_SET_DICT[vs_platform][0]
        return waf_platform


@conf
def convert_vs_platform_to_prefix(self, vs_platform):

    if vs_platform not in VS_PLATFORM_TO_WAF_PLATFORM_PREFIX_AND_TOOL_SET_DICT:
        Logs.error('Invalid platform error for waf platform {}:  Platform not found'.format(vs_platform))
        return 'UNKNOWN'
    else:
        waf_platform = VS_PLATFORM_TO_WAF_PLATFORM_PREFIX_AND_TOOL_SET_DICT[vs_platform][1]
        return waf_platform


@conf
def convert_waf_configuration_to_vs_configuration(self, configuration):
    configuration = configuration.replace('debug', 		'Debug')
    configuration = configuration.replace('profile',	'Profile')
    configuration = configuration.replace('release', 	'Release')
    configuration = configuration.replace('performance', 	'Performance')
    return configuration

@conf
def convert_vs_configuration_to_waf_configuration(self, configuration):
    configuration = configuration.replace('Debug', 'debug')
    configuration = configuration.replace('Profile', 'profile')
    configuration = configuration.replace('Release', 'release')
    configuration = configuration.replace('Performance', 'performance')
    return configuration

def is_valid_spec(ctx, spec_name):
    """ Check if the spec should be included when generating visual studio projects """
    if ctx.options.specs_to_include_in_project_generation == '':
        return True

    allowed_specs = ctx.options.specs_to_include_in_project_generation.replace(' ', '').split(',')
    if spec_name in allowed_specs:
        return True

    return False

@conf
def convert_waf_spec_to_vs_spec(self, ctx, spec_name):
    """ Helper to get the Visual Studio Spec name from a WAF spec name """
    return ctx.spec_visual_studio_name(spec_name)

###############################################################################
# Copy pasted from root wscript...
@conf
def convert_vs_configuration_to_waf_configuration(self, configuration):
    dedicated = ''
    test = ''
    if '_dedicated' in configuration:
        dedicated = '_dedicated'
    if '_test' in configuration:
        test = '_test'

    if 'Debug' in configuration:
        return 'debug' + test + dedicated
    if 'Profile' in configuration:
        return 'profile' + test + dedicated
    if 'Release' in configuration:
        return 'release' + test + dedicated
    if 'Performance' in configuration:
        return 'performance' + test + dedicated

    return ''

@conf
def convert_vs_spec_to_waf_spec(ctx, spec):
    """ Convert a Visual Studio spec into a WAF spec """
    # Split string "[spec] config" -> "spec"
    specToTest = spec.split("[")[1].split("]")[0]

    # Check VS spec name against all specs
    loaded_specs = ctx.loaded_specs()
    for spec_name in loaded_specs:
        if specToTest == ctx.spec_visual_studio_name(spec_name):
            return spec_name

    Logs.warn('Cannot find WAF spec for "%s"' % spec)
    return ''

reg_act = re.compile(r"(?P<backslash>\\)|(?P<dollar>\$\$)|(?P<subst>\$\{(?P<code>[^}]*?)\})", re.M)
def compile_template(line):
    """
    Compile a template expression into a python function (like jsps, but way shorter)
    """
    extr = []
    def repl(match):
        g = match.group
        if g('dollar'): return "$"
        elif g('backslash'):
            return "\\"
        elif g('subst'):
            extr.append(g('code'))
            return "<<|@|>>"
        return None

    line2 = reg_act.sub(repl, line)
    params = line2.split('<<|@|>>')
    assert(extr)

    indent = 0
    buf = []
    app = buf.append

    def app(txt):
        buf.append(indent * '\t' + txt)

    for x in range(len(extr)):
        if params[x]:
            app("lst.append(%r)" % params[x])

        f = extr[x]
        if f.startswith('if') or f.startswith('for'):
            app(f + ':')
            indent += 1
        elif f.startswith('py:'):
            app(f[3:])
        elif f.startswith('endif') or f.startswith('endfor'):
            indent -= 1
        elif f.startswith('else') or f.startswith('elif'):
            indent -= 1
            app(f + ':')
            indent += 1
        elif f.startswith('xml:'):
            app('lst.append(xml_escape(%s))' % f[4:])
        else:
            #app('lst.append((%s) or "cannot find %s")' % (f, f))
            app('lst.append(%s)' % f)

    if extr:
        if params[-1]:
            app("lst.append(%r)" % params[-1])

    fun = COMPILE_TEMPLATE % "\n\t".join(buf)
    #print(fun)
    return Task.funex(fun)


re_blank = re.compile('(\n|\r|\\s)*\n', re.M)
def rm_blank_lines(txt):
    txt = re_blank.sub('\r\n', txt)
    return txt

BOM = '\xef\xbb\xbf'
try:
    BOM = bytes(BOM, 'iso8859-1') # python 3
except:
    pass

def stealth_write(self, data, flags='wb'):
    try:
        x = unicode
    except:
        data = data.encode('utf-8') # python 3
    else:
        data = data.decode(sys.getfilesystemencoding(), 'replace')
        data = data.encode('utf-8')

    if self.name.endswith('.vcproj') or self.name.endswith('.vcxproj'):
        data = BOM + data

    try:
        txt = self.read(flags='rb')
        if txt != data:
            raise ValueError('must write')
    except (IOError, ValueError):
        self.write(data, flags=flags)
    else:
        Logs.debug('msvs: skipping %s' % self.abspath())
Node.Node.stealth_write = stealth_write

re_quote = re.compile("[^a-zA-Z0-9-.]")
def quote(s):
    return re_quote.sub("_", s)

def xml_escape(value):
    return value.replace("&", "&amp;").replace('"', "&quot;").replace("'", "&apos;").replace("<", "&lt;").replace(">", "&gt;")

def make_uuid(v, prefix = None):
    """
    simple utility function
    """
    if isinstance(v, dict):
        keys = list(v.keys())
        keys.sort()
        tmp = str([(k, v[k]) for k in keys])
    else:
        tmp = str(v)
    d = Utils.md5(tmp.encode()).hexdigest().upper()
    if prefix:
        d = '%s%s' % (prefix, d[8:])
    gid = uuid.UUID(d, version = 4)
    return str(gid).upper()

def diff(node, fromnode):
    # difference between two nodes, but with "(..)" instead of ".."
    c1 = node
    c2 = fromnode

    c1h = c1.height()
    c2h = c2.height()

    lst = []
    up = 0

    while c1h > c2h:
        lst.append(c1.name)
        c1 = c1.parent
        c1h -= 1

    while c2h > c1h:
        up += 1
        c2 = c2.parent
        c2h -= 1

    while id(c1) != id(c2):
        lst.append(c1.name)
        up += 1

        c1 = c1.parent
        c2 = c2.parent

    for i in range(up):
        lst.append('(..)')
    lst.reverse()
    return tuple(lst)

class build_property(object):
    pass

class vsnode(object):
    """
    Abstract class representing visual studio elements
    We assume that all visual studio nodes have a uuid and a parent
    """
    def __init__(self, ctx):
        self.ctx = ctx # msvs context
        self.name = '' # string, mandatory
        self.vspath = '' # path in visual studio (name for dirs, absolute path for projects)
        self.uuid = '' # string, mandatory
        self.parent = None # parent node for visual studio nesting

    def get_waf(self):
        """
        Override in subclasses...
        """
        return 'cd /d "%s" & "%s"' % (self.ctx.srcnode.abspath(), getattr(self.ctx, 'waf_command', cry_utils.WAF_EXECUTABLE))

    def ptype(self):
        """
        Return a special uuid for projects written in the solution file
        """
        pass

    def write(self):
        """
        Write the project file, by default, do nothing
        """
        pass

    def make_uuid(self, val):
        """
        Alias for creating uuid values easily (the templates cannot access global variables)
        """
        return make_uuid(val)

class vsnode_vsdir(vsnode):
    """
    Nodes representing visual studio folders (which do not match the filesystem tree!)
    """
    VS_GUID_SOLUTIONFOLDER = "2150E333-8FDC-42A3-9474-1A3956D46DE8"
    def __init__(self, ctx, uuid, name, vspath=''):
        vsnode.__init__(self, ctx)
        self.title = self.name = name
        self.uuid = uuid
        self.vspath = vspath or name

    def ptype(self):
        return self.VS_GUID_SOLUTIONFOLDER

def _get_filter_name(project_filter_dict, file_name):
    """ Helper function to get a correct project filter """
    if file_name.endswith('wscript') or file_name.endswith('.waf_files'):
        return '_WAF_' # Special case for wscript files

    if not file_name in project_filter_dict:
        return 'FILE_NOT_FOUND'

    project_filter = project_filter_dict[file_name]
    project_filter = project_filter.replace('/', '\\')
    if project_filter.lower() == 'root':
        return '.'

    return project_filter

class vsnode_project(vsnode):
    """
    Abstract class representing visual studio project elements
    A project is assumed to be writable, and has a node representing the file to write to
    """
    VS_GUID_VCPROJ = "8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942"
    def ptype(self):
        return self.VS_GUID_VCPROJ

    def __init__(self, ctx, node):
        vsnode.__init__(self, ctx)
        self.path = node
        self.uuid = make_uuid(node.abspath())
        self.name = node.name
        self.title = self.path.abspath()
        self.source = [] # list of node objects
        self.build_properties = [] # list of properties (nmake commands, output dir, etc)

        # Determine the visual studio version platform toolset
        if ctx.numver == '12.00':
            self.defaultPlatformToolSet = 'v120'
        elif ctx.numver=='14.00':
            self.defaultPlatformToolSet = 'v140'
        else:
            self.defaultPlatformToolSet = 'v120'

    def get_platform_toolset(self, vs_platform):
        if vs_platform not in VS_PLATFORM_TO_WAF_PLATFORM_PREFIX_AND_TOOL_SET_DICT:
            Logs.warn('Invalid platform error for vs platform {}:  Platform not found, defaulting to {}'.format(vs_platform,self.defaultPlatformToolSet))
            return self.defaultPlatformToolSet
        else:
            supported_platform_toolsets = VS_PLATFORM_TO_WAF_PLATFORM_PREFIX_AND_TOOL_SET_DICT[vs_platform][2]
            if len(supported_platform_toolsets) == 1:
                return supported_platform_toolsets[0]
            else:
                for supported_platform_toolset in supported_platform_toolsets:
                    if supported_platform_toolset == self.defaultPlatformToolSet:
                        return supported_platform_toolset
                # If we reach this point, that means that the toolset for platform specified in the vs platform dictionary
                # cannot be supported by the current VS ide.  It should not reach this point because the project should
                # be filter out, but log a warning anyways and continue
                Logs.warn('Invalid platform error for vs platform {}:  Platform not found, defaulting to {}'.format(vs_platform,self.defaultPlatformToolSet))

    def dirs(self):
        """
        Get the list of parent folders of the source files (header files included)
        for writing the filters
        """
        lst = []

        def add(x):
            if not x.lower() == '.':
                lst.append(x)

            seperator_pos = x.rfind('\\')
            if seperator_pos != -1:
                add(x[:seperator_pos])


        for source in self.source:
            add( _get_filter_name(self.project_filter, source.abspath()) )

        # Remove duplicates
        lst = list(set(lst))
        return lst

    def write(self):
        Logs.debug('msvs: creating %r' % self.path)

        def _write(template_name, output_node):
            template = compile_template(template_name)
            template_str = template(self)
            template_str = rm_blank_lines(template_str)
            output_node.stealth_write(template_str)

        _write(PROJECT_TEMPLATE, self.path)
        _write(FILTER_TEMPLATE, self.path.parent.make_node(self.path.name + '.filters'))
        _write(PROJECT_USER_TEMPLATE, self.path.parent.make_node(self.path.name + '.user'))
        _write(PROPERTY_SHEET_TEMPLATE, self.path.parent.make_node(self.path.name + '.default.props'))

    def get_key(self, node):
        """
        required for writing the source files
        """
        name = node.name
        if name.endswith('.cpp') or name.endswith('.c'):
            return 'ClCompile'
        return 'ClInclude'

    def collect_properties(self):
        """
        Returns a list of triplet (configuration, platform, output_directory)
        """
        ret = []
        for c in self.ctx.configurations:
            for p in self.ctx.platforms:
            
                waf_spec = self.ctx.convert_vs_spec_to_waf_spec(c)
                # if this spec has no game projects enabled, then don't generate this configuration.
                if '_dedicated' in  c:
                    if self.ctx.spec_disable_games(waf_spec):
                        Logs.debug('lumberyard:  Not generating (%s|%s) for %s - it has no game projects' % (c, p, waf_spec))
                        continue

                # not all plaform/configurations are valid, skip if suppressed
                waf_platform = self.ctx.convert_vs_platform_to_waf_platform(p)
                if waf_platform in PLATFORM_CONFIGURATION_FILTER:
                    waf_config = self.ctx.convert_vs_configuration_to_waf_configuration(c)
                    if waf_config not in PLATFORM_CONFIGURATION_FILTER[waf_platform]:
                        Logs.debug('lumberyard:  skipping (%s|%s) for %s - PLATFORM_CONFIGURATION_FILTER has removed this project' % (c, p, waf_spec))
                        continue

                x = build_property()
                x.outdir = ''
                x.configuration = c
                x.platform = p
                
                active_projects = self.ctx.get_enabled_game_project_list()
                if len(active_projects) != 1:
                    x.game_project = ''
                else:
                    x.game_project = active_projects[0]

                x.preprocessor_definitions = ''
                x.includes_search_path = ''

                # can specify "deploy_dir" too
                ret.append(x)
        self.build_properties = ret

    def get_build_params(self, props):
        opt = '--execsolution="%s"' % self.ctx.get_solution_node().abspath()
        return (self.get_waf(), opt)

    def get_build_command(self, props):
        params = self.get_build_params(props)
        return "%s build_" % params[0] + self.ctx.convert_vs_platform_to_waf_platform(props.platform) + '_' + self.ctx.convert_vs_configuration_to_waf_configuration(props.configuration) + ' --project-spec ' + self.ctx.convert_vs_spec_to_waf_spec(props.configuration) + " %s" % params[1]

    def get_clean_command(self, props):
        params = self.get_build_params(props)
        return "%s clean_" % params[0] + self.ctx.convert_vs_platform_to_waf_platform(props.platform) + '_' + self.ctx.convert_vs_configuration_to_waf_configuration(props.configuration) + ' --project-spec ' + self.ctx.convert_vs_spec_to_waf_spec(props.configuration) + " %s" % params[1]

    def get_output_folder(self, platform, configuration):
        output_folders = self.ctx.get_output_folders(platform, configuration)
        if len(output_folders) == 0:
            return ''

        output_path = output_folders[0]
        relative_path = output_path.path_from(self.ctx.path)

        return '$(SolutionDir)..\\' + relative_path + '\\'

    def get_layout_folder(self, platform):
        return '$(SolutionDir)..\\' + platform + 'Layout\\'

    def get_rebuild_command(self, props):
        params = self.get_build_params(props)
        return "%s" % params[0] + \
               " clean_" + self.ctx.convert_vs_platform_to_waf_platform(props.platform) + '_' + self.ctx.convert_vs_configuration_to_waf_configuration(props.configuration) + \
               " build_" + self.ctx.convert_vs_platform_to_waf_platform(props.platform) + '_' + self.ctx.convert_vs_configuration_to_waf_configuration(props.configuration) + \
               ' --project-spec ' + self.ctx.convert_vs_spec_to_waf_spec(props.configuration) +\
               " %s" % params[1]

    def get_filter_name(self, node):
        return _get_filter_name(self.project_filter, node.abspath())

    def get_all_config_platform_conditional(self):
        condition_string = " Or ".join(["('$(Configuration)|$(Platform)'=='%s|%s')" % (prop.configuration, prop.platform) for prop in self.build_properties])
        return condition_string


class vsnode_alias(vsnode_project):
    def __init__(self, ctx, node, name):
        vsnode_project.__init__(self, ctx, node)
        self.name = name
        self.output_file = ''
        self.project_filter = {}

class vsnode_build_all(vsnode_alias):
    """
    Fake target used to emulate the behaviour of "make all" (starting one process by target is slow)
    This is the only alias enabled by default
    """
    def __init__(self, ctx, node, name='_WAF_'):
        vsnode_alias.__init__(self, ctx, node, name)
        self.is_active = True

    def collect_properties(self):
        """
        Visual studio projects are associated with platforms and configurations (for building especially)
        """
        super(vsnode_build_all, self).collect_properties()

        for x in self.build_properties:
            x.outdir = self.path.parent.abspath()
            x.preprocessor_definitions = ''
            x.includes_search_path = ''
            x.c_flags = ''
            x.cxx_flags = ''
            x.link_flags = ''
            x.target_spec = ''
            x.target_config = ''

            current_spec_name = self.ctx.convert_vs_spec_to_waf_spec(x.configuration)
            waf_platform = self.ctx.convert_vs_platform_to_waf_platform(x.platform)
            waf_configuration = self.ctx.convert_vs_configuration_to_waf_configuration(x.configuration)

            x.target_spec = current_spec_name
            x.target_config = waf_platform + '_' + waf_configuration

        # Collect WAF files
        waf_data_dir            = self.ctx.root.make_node(Context.launch_dir).make_node('_WAF_')
        waf_source_dir          = self.ctx.root.make_node(Context.launch_dir).make_node('Code/Tools/waf-1.7.13')

        waf_config_files        = waf_data_dir.ant_glob('**/*', maxdepth=0)
        waf_spec_files          = waf_data_dir.make_node('specs').ant_glob('**/*')

        waf_scripts             = waf_source_dir.make_node('waflib').ant_glob('**/*.py')
        waf_lumberyard_scripts  = waf_source_dir.make_node('lmbrwaflib').ant_glob('**/*.py')

        # Remove files found in lmbrwaflib from waf scripts
        tmp = []
        waf_lumberyard_scripts_files = []
        for node in waf_lumberyard_scripts:
            waf_lumberyard_scripts_files += [os.path.basename(node.abspath())]

        for x in waf_scripts:
            if os.path.basename(x.abspath()) in waf_lumberyard_scripts_files:
                continue
            tmp += [x]
        waf_scripts = tmp

        for file in waf_config_files:
            self.project_filter[file.abspath()] = 'Settings'
        for file in waf_spec_files:
            self.project_filter[file.abspath()] = 'Specs'

        for file in waf_scripts:
            filter_name = 'Scripts'
            subdir = os.path.dirname( file.path_from( waf_source_dir.make_node('waflib') ) )
            if subdir != '':
                filter_name += '/' + subdir
            self.project_filter[file.abspath()] = filter_name

        for file in waf_lumberyard_scripts:
            self.project_filter[file.abspath()] = 'Lumberyard Scripts'

        self.source += waf_config_files + waf_spec_files + waf_scripts + waf_lumberyard_scripts

        # without a cpp file, VS wont compile this project
        dummy_node = self.ctx.get_bintemp_folder_node().make_node('__waf_compile_dummy__.cpp')
        self.project_filter[dummy_node.abspath()] = 'DummyCompileNode'
        self.source += [ dummy_node ]


class vsnode_install_all(vsnode_alias):
    """
    Fake target used to emulate the behaviour of "make install"
    """
    def __init__(self, ctx, node, name='install_all_projects'):
        vsnode_alias.__init__(self, ctx, node, name)

    def get_build_command(self, props):
        return "%s build install %s" % self.get_build_params(props)

    def get_clean_command(self, props):
        return "%s clean %s" % self.get_build_params(props)

    def get_rebuild_command(self, props):
        return "%s clean build install %s" % self.get_build_params(props)

class vsnode_project_view(vsnode_alias):
    """
    Fake target used to emulate a file system view
    """
    def __init__(self, ctx, node, name='project_view'):
        vsnode_alias.__init__(self, ctx, node, name)
        self.tg = self.ctx() # fake one, cannot remove
        self.exclude_files = Node.exclude_regs + '''
waf-1.7.*
waf3-1.7.*/**
.waf-1.7.*
.waf3-1.7.*/**
**/*.sdf
**/*.suo
**/*.ncb
**/%s
        ''' % Options.lockfile

    def collect_source(self):
        # this is likely to be slow
        self.source = self.ctx.srcnode.ant_glob('**', excl=self.exclude_files)

    def get_build_command(self, props):
        params = self.get_build_params(props) + (self.ctx.cmd,)
        return "%s %s %s" % params

    def get_clean_command(self, props):
        return ""

    def get_rebuild_command(self, props):
        return self.get_build_command(props)

class vsnode_target(vsnode_project):
    """
    Visual studio project representing a targets (programs, libraries, etc) and bound
    to a task generator
    """
    def __init__(self, ctx, tg):
        """
        A project is more or less equivalent to a file/folder
        """
        base = getattr(ctx, 'projects_dir', None) or tg.path
        node = base.make_node(quote(tg.name) + ctx.project_extension) # the project file as a Node
        vsnode_project.__init__(self, ctx, node)
        self.name = quote(tg.name)
        self.tg     = tg  # task generator
        if getattr(tg, 'need_deploy', None):
            if not isinstance(tg.need_deploy, list):
                self.is_deploy = [ tg.need_deploy ]
            else:
                self.is_deploy = tg.need_deploy
        self.project_filter = self.tg.project_filter

    def get_build_params(self, props):
        """
        Override the default to add the target name
        """
        opt = '--execsolution="%s"' % self.ctx.get_solution_node().abspath()
        if getattr(self, 'tg', None):
            opt += " --targets=%s" % self.tg.name
        # Propagate verbosity to vc proj build commands
        if self.ctx.options.verbose:
            opt += " -"
            for i in range(self.ctx.options.verbose):
                opt += 'v'
        return (self.get_waf(), opt)

    def collect_source(self):
        tg = self.tg
        source_files = tg.to_nodes(getattr(tg, 'source', []))
        include_dirs = Utils.to_list(getattr(tg, 'msvs_includes', []))
        waf_file_entries = self.tg.waf_file_entries;

        include_files = []
        """"
        for x in include_dirs:
            if isinstance(x, str):
                x = tg.path.find_node(x)
            if x:
                lst = [y for y in x.ant_glob(HEADERS_GLOB, flat=False)]
                include_files.extend(lst)
        """
        # remove duplicates
        self.source.extend(list(set(source_files + include_files)))
        self.source += [ tg.path.make_node('wscript') ] + waf_file_entries
        self.source.sort(key=lambda x: x.abspath())

    def ConvertToDict(self, var):
        # dict type
        if isinstance(var, dict):
            return dict(var)

        # list type
        if isinstance(var, list):
            return dict(var)

        # class type
        try:
            return dict(var.__dict__)
        except:
            return None

    def GetPlatformSettings(self, target_platform, target_configuration, name, settings):
        """
        Util function to apply flags based on current platform
        """

        result = []
        platforms = [target_platform]
        # Append common win platform for windows hosts
        if target_platform in ('win_x86', 'win_x64'):
            platforms.append('win')
        if target_platform in ('linux_x64'):
            platforms.append('linux')
        if target_platform in ('darwin_x86', 'darwin_x64'):
            platforms.append('darwin')

        settings_dict = self.ConvertToDict(settings)

        if not settings_dict:
            print ("WAF_ERROR: Unsupported type '%s' for 'settings' variable encountered." % type(settings))
            return

        # add non platform specific settings
        try:
            if isinstance(settings_dict[name],list):
                result += settings_dict[name]
            else:
              result += [settings_dict[name]]
        except:
            pass

        # add per configuration flags
        configuration_specific_name = (target_configuration + '_' + name)
        try:
            if isinstance(settings_dict[configuration_specific_name],list):
                result += settings_dict[configuration_specific_name]
            else:
              result += [settings_dict[configuration_specific_name]]
        except:
            pass

        # add per platform flags
        for platform in platforms:
            platform_specific_name = (platform + '_' + name)
            try:
                if isinstance(settings_dict[platform_specific_name],list):
                    result += settings_dict[platform_specific_name]
                else:
                    result += [settings_dict[platform_specific_name]]
            except:
                pass

        # add per platform_configuration flags
        for platform in platforms:
            platform_configuration_specific_name = (platform + '_' + target_configuration + '_' + name)
            try:
                if isinstance(settings_dict[platform_configuration_specific_name],list):
                    result += settings_dict[platform_configuration_specific_name]
                else:
                    result += [settings_dict[platform_configuration_specific_name]]
            except:
                pass

        return result

    # Method to recurse a taskgen item's use dependencies and build up a unique include_list for include paths
    def recurse_use_includes_and_defines(self, waf_platform, cur_tg, include_list, defines_list, uselib_cache):
        """
        Method to recurse a taskgen item's use dependencies and build up a unique include_list for include paths

        :param waf_platform:    The current waf_platform
        :param cur_tg:          The taskgen to inspect
        :param include_list:    The includes list to populate
        :param defines_list:    The defines list to populate
        :param uselib_cache:    uselib cache to maintain to prevent unnecessary processing
        """
        visited_nodes = set()

        def _recurse(tg):

            # Prevent infinite loops from possible cyclic dependencies
            visited_nodes.add(tg.name)

            # Make sure that a list of strings is provided as the export includes
            check_export_includes = Utils.to_list(tg.export_includes) if tg.export_includes is not None else None

            # Make sure the export includes is not empty before processing
            if check_export_includes is not None and len(check_export_includes) > 0:
                for raw_export_include in check_export_includes:
                    if (isinstance(raw_export_include, Node.Node)):
                        abs_export_include_path = raw_export_include.abspath()
                        append_to_unique_list(include_list, abs_export_include_path)
                    elif os.path.isabs(raw_export_include):
                        append_to_unique_list(include_list, raw_export_include)
                    else:
                        abs_export_include_path = tg.path.make_node(raw_export_include).abspath()
                        append_to_unique_list(include_list, abs_export_include_path)

            # Include export defines if provided
            if hasattr(tg, 'export_defines'):
                check_export_defines = Utils.to_list(tg.export_defines)
                if len(check_export_defines) > 0:
                    for export_define in check_export_defines:
                        append_to_unique_list(defines_list, export_define)

            # Perform additional includes and defines analysis on uselibs
            self.process_uselib_include_and_defines(waf_platform, cur_tg, include_list, defines_list, uselib_cache)

            # Recurse into the 'use' for this taskgen if possible
            if not hasattr(tg, 'use') or len(tg.use) == 0:
                return
            for use_module in tg.use:
                if use_module in self.ctx.task_gen_cache_names:
                    use_tg = self.ctx.task_gen_cache_names[use_module]
                    if use_tg.name not in visited_nodes:
                        _recurse(use_tg)
                        visited_nodes.add(use_tg.name)

        _recurse(cur_tg)

    def process_uselib_include_and_defines(self, waf_platform, cur_tg, include_list, defines_list, uselib_cache):
        """
        Perform inspection of a taskgen's uselib (and autod_uselib) for additional includes and defines

        :param waf_platform:    The current waf_platform
        :param cur_tg:          The taskgen to inspect
        :param include_list:    The includes list to populate
        :param defines_list:    The defines list to populate
        :param uselib_cache:    uselib cache to maintain to prevent unnecessary processing
        """
        tg_uselibs = getattr(cur_tg, 'uselib', []) + getattr(cur_tg, 'autod_uselib', [])

        for tg_uselib in tg_uselibs:

            if tg_uselib in uselib_cache:
                # Get the cached includes value if any
                tg_uselib_includes_value = uselib_cache[tg_uselib][0]
                if tg_uselib_includes_value is not None and len(tg_uselib_includes_value) > 0:
                    append_to_unique_list(include_list, tg_uselib_includes_value)

                # Get the cached defines value if any
                tg_uselib_defines_value = uselib_cache[tg_uselib][1]
                if tg_uselib_defines_value is not None and len(tg_uselib_defines_value) > 0:
                    append_to_unique_list(defines_list, tg_uselib_defines_value)
            else:
                # Using the platform, build the uselib variable key to lookup in the cached environment of the tg

                # Lookup the includes
                tg_uselib_includes_value = None
                includes_variable_name = 'INCLUDES_{}_{}'.format(waf_platform, tg_uselib)
                if includes_variable_name in cur_tg.env:
                    tg_uselib_includes_value = cur_tg.env[includes_variable_name]
                    if tg_uselib_includes_value is not None:
                        append_to_unique_list(include_list, tg_uselib_includes_value)

                # Lookup the defines
                tg_uselib_defines_value = None
                defines_variable_name = 'DEFINES_{}_{}'.format(waf_platform, tg_uselib)
                if defines_variable_name in cur_tg.env:
                    tg_uselib_defines_value = cur_tg.env[defines_variable_name]
                    if tg_uselib_defines_value is not None:
                        append_to_unique_list(defines_list, tg_uselib_defines_value)

                # Cache the results
                uselib_cache[tg_uselib] = (tg_uselib_includes_value,tg_uselib_defines_value)

    def collect_properties(self):

        """
        Visual studio projects are associated with platforms and configurations (for building especially)
        """

        project_generator_node = self.ctx.bldnode.make_node('project_generator')
        project_generator_prefix = project_generator_node.abspath()

        super(vsnode_target, self).collect_properties()
        for x in self.build_properties:
            x.outdir = self.path.parent.abspath()
            x.preprocessor_definitions = ''
            x.includes_search_path = ''
            x.c_flags = ''
            x.cxx_flags = ''
            x.link_flags = ''
            x.target_spec = ''
            x.target_config = ''

            if not hasattr(self.tg ,'link_task') and 'create_static_library' not in self.tg.features:
                continue

            if True:
                current_spec_name = self.ctx.convert_vs_spec_to_waf_spec(x.configuration)
                waf_platform = self.ctx.convert_vs_platform_to_waf_platform(x.platform)
                waf_configuration = self.ctx.convert_vs_configuration_to_waf_configuration(x.configuration)
                spec_modules = self.ctx.spec_modules(current_spec_name)

                bIncludeProject = True

                # Determine the module's supported platform(s) and configuration(s) and see if it needs to be included
                # in the project
                module_platforms = self.ctx.get_module_platforms(self.tg.target)
                module_configurations = self.ctx.get_module_configurations(self.tg.target, waf_platform)
                if waf_platform not in module_platforms:
                    bIncludeProject = False
                elif waf_configuration not in module_configurations:
                    bIncludeProject = False
                elif self.tg.target not in spec_modules and \
                    self.tg.target not in self.ctx.get_all_module_uses(current_spec_name, waf_platform, waf_configuration):
                    bIncludeProject = False

                # Check if this project is supported for the current spec
                if not bIncludeProject:
                    # Has to be valid file name. Otherwise the solution will not be able to load in VS
                    output_file_name = 'Error__Invalid_startup_project__%s__Selected_project_not_supported_for_configuration__%s__' % (self.tg.target.replace(' ', '_'), x.configuration.strip().replace(' ', '_').replace('[', '_').replace(']','_'))
                    x.output_file = output_file_name
                    x.output_file_name = output_file_name
                    x.includes_search_path = ""
                    x.preprocessor_definitions = ""
                    x.c_flags = ''
                    x.cxx_flags = ''
                    x.link_flags = ''
                    x.target_spec = ''
                    continue

                current_env = self.ctx.all_envs[waf_platform + '_' + waf_configuration]
                mscv_helper.verify_options_common(current_env)
                x.c_flags = " ".join(current_env['CFLAGS']) # convert list to space separated string
                x.cxx_flags = " ".join(current_env['CXXFLAGS']) # convert list to space separated string
                x.link_flags = " ".join(current_env['LINKFLAGS']) # convert list to space separated string

                x.target_spec = current_spec_name
                x.target_config = waf_platform + '_' + waf_configuration

                has_link_task = False

                # For VS projects with a single game project target
                if hasattr(self.tg, 'link_task'): # no link task implies special static modules which are static libraries
                    link_task_pattern_name = self.tg.link_task.__class__.__name__ + '_PATTERN'
                    has_link_task = True
                else:
                    link_task_pattern_name = 'cxxstlib_PATTERN'

                pattern = current_env[link_task_pattern_name]

                output_folder_nodes = self.ctx.get_output_folders(waf_platform, waf_configuration)
                output_folder_node = output_folder_nodes[0]
                if hasattr(self.tg, 'output_sub_folder'):
                    output_folder_node = output_folder_node.make_node(self.tg.output_sub_folder)

                output_file_name = self.tg.output_file_name
                # Save project info
                output_file = output_folder_node.make_node(output_file_name)
                x.output_file = output_folder_node.abspath() + '\\' + pattern % output_file_name
                x.output_file_name = output_file_name
                x.output_path = os.path.dirname(x.output_file)
                x.outdir = x.output_path
                x.assembly_name = output_file_name
                x.output_type = os.path.splitext(x.output_file)[1][1:] # e.g. ".dll" to "dll"

                if not has_link_task: # Project with no outputs (static lib or pile of objects)
                    # Save project info
                    output_file_name = 'Error__Invalid_startup_project__%s__Selected_project_has_no_executable' % (self.tg.target.replace(' ', '_'))
                    x.output_file = output_file_name
                    x.output_file_name = output_file_name

                # Collect all defines for this configuration
                define_list =  list(current_env['DEFINES'])

                define_list += self.GetPlatformSettings( waf_platform, waf_configuration, 'defines', self.tg )

                include_list = list(current_env['INCLUDES'])
                include_list += self.GetPlatformSettings( waf_platform, waf_configuration, 'includes', self.tg )
                # make sure we only have absolute path for intellisense
                # If we don't have a absolute path, assume a relative one, hence prefix the path with the taskgen path and comvert it into an absolute one
                for i in range(len(include_list)):
                    if (isinstance(include_list[i], Node.Node)): # convert nodes to abs paths
                        include_list[i] = include_list[i].abspath()
                    else:
                        if not os.path.isabs( include_list[i] ):
                            include_list[i] = os.path.abspath( self.tg.path.abspath() + '/' + include_list[i] )

                # For generate header files that need to be included, the include path during project generation will
                # be $ROOT\BinTemp\project_generator\... because the task is generated based on files that are to be
                # created.  At this point, the include paths from this step will be included if they exist (ie UI4)
                # so we will filter and replace all paths that start with $ROOT\BinTemp\project_generator\... and
                # replace the header with what the files WILL BE during the build process, where those platform
                # specific header files will be generated
                platform_configuration = waf_platform + '_' + waf_configuration;
                replace_project_generator_prefix = self.ctx.bldnode.make_node(platform_configuration).abspath()
                include_list = [ p.replace(project_generator_prefix, replace_project_generator_prefix) for p in include_list]

                # Do a depth-first recursion into the use dependencies to collect any additional include paths
                uselib_include_cache = {}
                self.recurse_use_includes_and_defines(waf_platform, self.tg, include_list, define_list, uselib_include_cache)

                if 'qt5' in self.tg.features:
                    # Special case for projects that use QT.  It needs to resolve the intermediate/generated QT related files based
                    # on the configuration.
                    qt_intermediate_include_path = os.path.join(self.tg.bld.bldnode.abspath(),
                                                                x.target_config,
                                                                'qt5',
                                                                '{}.{}'.format(self.name, self.tg.idx))
                    include_list.append(qt_intermediate_include_path)

                x.includes_search_path = ';'.join(include_list)
                x.preprocessor_definitions = ';'.join(define_list)

class msvs_generator(BuildContext):
    after = ['gen_spec_use_map_file']

    '''generates a visual studio 2013 solution'''
    cmd = 'msvs'
    fun = 'build'

    def init(self):

        # Set the correct version number and version based on the minimum msvc compiler specified in the user_settings.options (or passed in)
        build_options = self.options
        msvs_version = build_options.msvs_version
        if '12' == msvs_version:
            self.numver = '12.00'
            self.vsver  = '2013'
        elif '14' == msvs_version:
            self.numver = '14.00'
            self.vsver  = '14'
        else:
            Logs.warn('The minimum compiler version is not set properly.  Defaulting to Visual Studio 2013 for the generated solutions')
            self.numver = '12.00'
            self.vsver  = '2013'

        # default project is used to sort one project to the top of the projects list.  That will cause it to become
        # the default startup project if its not overridden in the .suo file
        self.default_project = build_options.default_project

        # Collect the valid specs
        valid_specs = []
        for spec in self.loaded_specs():
            if is_valid_spec(self, spec):
                valid_specs.append(spec)

        # Collect any spec-restricted platforms (or all platforms)
        valid_platforms = []
        for spec in valid_specs:
            restricted_platforms = self.preprocess_platform_list(self.spec_platforms(spec))
            if len(restricted_platforms) == 0:
                # If there are any specs that are unfiltered, then we must consider all platforms
                valid_platforms = [self.convert_waf_platform_to_vs_platform(platform) \
                                   for platform in self.get_supported_platforms() if self.check_msvs_compatibility(platform, self.vsver)]
                break
            else:
                for restricted_platform in [self.convert_waf_platform_to_vs_platform(platform) \
                                                for platform in restricted_platforms \
                                                    if self.check_msvs_compatibility(platform, self.vsver) and platform not in valid_platforms]:
                    valid_platforms.append(restricted_platform)

        """
        Some data that needs to be present
        """
        if not getattr(self, 'platforms', None):
            self.platforms = valid_platforms

        if not getattr(self, 'configurations', None):
            build_configurations = self.get_supported_configurations()
            self.configurations = []
            for spec in self.loaded_specs():
                # Only care about valid specs
                if not is_valid_spec(self, spec):
                    continue

                # Check if the spec has restricted build configurations
                targeted_configurations = self.preprocess_configuration_list(None, None, self.spec_configurations(spec))

                for conf in build_configurations:

                    if (len(targeted_configurations) > 0) and (conf not in targeted_configurations):
                        continue

                    solution_conf_name = '[' + self.convert_waf_spec_to_vs_spec(self, spec) + '] ' + conf
                    solution_conf_name_vs = self.convert_waf_configuration_to_vs_configuration( solution_conf_name )
                    if '_dedicated' in conf and self.spec_disable_games(spec):
                        Logs.debug('lumberyard:  Not generating dedicated server configuration "%s" for [%s] - no game projects' % (conf, spec))
                        continue

                    # Make sure that there is at least one module that is defined for the platform and configuration
                    skipConfig = True
                    for p in self.platforms:
                        wafPlatform = convert_vs_platform_to_waf_platform(self, p)
                        all_spec_modules = self.spec_modules(spec,wafPlatform,conf)
                        if len(all_spec_modules)>0:
                            skipConfig = False
                            break
                    if skipConfig:
                        Logs.debug('lumberyard:  Skipping {} due to no modules defined'.format(solution_conf_name_vs))
                        continue

                    self.configurations.append(solution_conf_name_vs)

        if not getattr(self, 'all_projects', None):
            self.all_projects = []
        if not getattr(self, 'project_extension', None):
            self.project_extension = '.vcxproj'
        if not getattr(self, 'projects_dir', None):
            self.projects_dir = self.srcnode.make_node(self.get_solution_dep_proj_folder_name())
            self.projects_dir.mkdir()

        # bind the classes to the object, so that subclass can provide custom generators
        if not getattr(self, 'vsnode_vsdir', None):
            self.vsnode_vsdir = vsnode_vsdir
        if not getattr(self, 'vsnode_target', None):
            self.vsnode_target = vsnode_target
        if not getattr(self, 'vsnode_build_all', None):
            self.vsnode_build_all = vsnode_build_all
        if not getattr(self, 'vsnode_install_all', None):
            self.vsnode_install_all = vsnode_install_all
        if not getattr(self, 'vsnode_project_view', None):
            self.vsnode_project_view = vsnode_project_view

        self.waf_project = None


    def execute(self):
        """
        Entry point
        """
        self.restore()
        if not self.all_envs:
            self.load_envs()

        self.load_user_settings()
        Logs.info("[WAF] Executing 'msvs' in '%s'" % (self.variant_dir)	)
        self.recurse([self.run_dir])

        # user initialization
        self.init()

        # two phases for creating the solution
        self.collect_projects() # add project objects into "self.all_projects"
        self.write_files() # write the corresponding project and solution files
        self.post_build()

    def collect_projects(self):
        """
        Fill the list self.all_projects with project objects
        Fill the list of build targets
        """
        self.collect_targets()
        self.add_aliases()
        self.collect_dirs()
        default_project = getattr(self, 'default_project', None)
        def sortfun(x):
            if x.name == default_project:
                return ''
            return getattr(x, 'path', None) and x.path.abspath() or x.name
        self.all_projects.sort(key=sortfun)

    def write_files(self):
        """
        Write the project and solution files from the data collected
        so far. It is unlikely that you will want to change this
        """
        for p in self.all_projects:
            Logs.debug('lumberyard:  Writing Visual Studio project for module %s ' % p.name)
            p.write()

        # and finally write the solution file
        node = self.get_solution_node()
        node.parent.mkdir()
        Logs.warn('Creating %r' % node)
        template1 = compile_template(SOLUTION_TEMPLATE)
        sln_str = template1(self)
        sln_str = rm_blank_lines(sln_str)
        node.stealth_write(sln_str)


    def get_solution_node(self):
        """
        The solution filename is required when writing the .vcproj files
        return self.solution_node and if it does not exist, make one
        """
        try:
            return self.solution_node
        except:
            pass

        solution_name = getattr(self, 'solution_name', None)
        if not solution_name:
            solution_name = getattr(Context.g_module, Context.APPNAME, 'project') + '.sln'
        if os.path.isabs(solution_name):
            self.solution_node = self.root.make_node(solution_name)
        else:
            self.solution_node = self.srcnode.make_node(solution_name)
        return self.solution_node

    def project_configurations(self):
        """
        Helper that returns all the pairs (config,platform)
        """
        ret = []
        for c in self.configurations:
            for p in self.platforms:
                ret.append((c, p))
        return ret

    def collect_targets(self):
        """
        Process the list of task generators
        """
        # we will iterate over every platform and configuration permutation
        # and add any that match the current "live" set to the project.
        for g in self.groups:

            for tg in g:

                if not isinstance(tg, TaskGen.task_gen):
                    continue

                # Skip any taskgen that does not have a project filter (not specified for VS)
                if not hasattr(tg,'project_filter'):
                    continue

                # Get the platform restrictions from the module definitions
                module_platforms = self.get_module_platforms(tg.target)

                # only include projects which are valid for any spec which we
                # have or are specified in the project spec
                if self.options.specs_to_include_in_project_generation != '':

                    bSkipModule = True
                    allowed_specs = self.options.specs_to_include_in_project_generation.replace(' ', '').split(',')

                    for spec_name in allowed_specs:
                        for p in self.platforms:

                            wafPlatform = convert_vs_platform_to_waf_platform(self, p)
                            # The module must support the platform
                            if wafPlatform not in module_platforms:
                                bSkipModule = True
                                continue

                            # Get any configuration restrictions for the current platform
                            module_configurations = self.get_module_configurations(tg.target, wafPlatform)

                            # Go through each of the possible configurations to make sure that this module qualifies
                            for c in self.configurations:

                                wafConfiguration = convert_vs_configuration_to_waf_configuration(self, c)

                                # The module must support the configuration
                                if wafConfiguration not in module_configurations:
                                    bSkipModule = True
                                    continue

                                # The module must either exist in the spec, or must exist as a module inferred use module
                                all_spec_modules = self.spec_modules(spec_name, wafPlatform, wafConfiguration)
                                all_inferred_spec_modules = self.get_all_module_uses(spec_name,wafPlatform,wafConfiguration)

                                if tg.target in all_spec_modules:
                                    bSkipModule = False
                                if tg.target in all_inferred_spec_modules:
                                    bSkipModule = False
                                if not bSkipModule:
                                    break
                            if not bSkipModule:
                                break
                        if not bSkipModule:
                            break

                    if bSkipModule:
                        # Check the project settings....
                        for project in self.get_enabled_game_project_list():
                            for platform in self.platforms:
                                platform_prefix = convert_vs_platform_to_prefix(self, platform)
                                if tg.target in self.project_and_platform_modules(project, platform_prefix):
                                    bSkipModule = False
                                    break
                        if bSkipModule:
                            Logs.debug('lumberyard:  Not creating Visual Studio project for module %s because its not in any enabled module in current specs' % tg.target)
                            continue

                Logs.debug('lumberyard:  Creating Visual Studio project for module %s ' % tg.target)

                if not hasattr(tg, 'msvs_includes'):
                    tg.msvs_includes = tg.to_list(getattr(tg, 'includes', [])) + tg.to_list(getattr(tg, 'export_includes', []))
                tg.post()

                p = self.vsnode_target(self, tg)
                p.collect_source() # delegate this processing
                p.collect_properties()
                self.all_projects.append(p)

    def add_aliases(self):
        """
        Add a specific target that emulates the "make all" necessary for Visual studio when pressing F7
        We also add an alias for "make install" (disabled by default)
        """
        base = getattr(self, 'projects_dir', None) or self.tg.path

        node_project = base.make_node('_WAF_' + self.project_extension) # Node.
        p_build = self.vsnode_build_all(self, node_project)
        p_build.collect_properties()
        self.all_projects.append(p_build)
        self.waf_project = p_build


    def collect_dirs(self):
        """
        Create the folder structure in the Visual studio project view
        """
        seen = {}

        for p in self.all_projects[:]: # iterate over a copy of all projects
            if not getattr(p, 'tg', None):
                # but only projects that have a task generator
                continue

            if getattr(p, 'parent', None):
                # aliases already have parents
                continue

            filter_name = self.get_project_vs_filter(p.name)

            proj = p
            filter_list = filter_name.split('/')
            filter_list.reverse()
            for f in filter_list:
                if f in seen:
                    proj.parent = seen[f]
                    break
                else:
                    n = proj.parent = seen[f] = self.vsnode_vsdir(self, make_uuid(f), f)
                    self.all_projects.append(n)
                    proj = n

def options(ctx):
    """
    If the msvs option is used, try to detect if the build is made from visual studio
    """
    ctx.add_option('--execsolution', action='store', help='when building with visual studio, use a build state file')

    old = BuildContext.execute
    def override_build_state(ctx):
        def lock(rm, add):
            uns = ctx.options.execsolution.replace('.sln', rm)
            uns = ctx.root.make_node(uns)
            try:
                Logs.debug('msvs: Removing %s' % uns.abspath())
                uns.delete()
            except:
                pass

            uns = ctx.options.execsolution.replace('.sln', add)
            uns = ctx.root.make_node(uns)
            try:
                Logs.debug('msvs: Touching %s' % uns.abspath())
                uns.write('')
            except:
                pass

        if ctx.options.execsolution:
            ctx.launch_dir = Context.top_dir # force a build for the whole project (invalid cwd when called by visual studio)
            modtime = time.mktime(datetime.datetime.now().timetuple())
            lock('.lastbuildstate', '.unsuccessfulbuild')
            old(ctx)
            lock('.unsuccessfulbuild', '.lastbuildstate')
            lbs = ctx.options.execsolution.replace('.sln', '.lastbuildstate')
            lbs = ctx.root.make_node(lbs)
            atime = time.mktime(datetime.datetime.now().timetuple())
            os.utime(lbs.abspath(), (atime, modtime))
        else:
            old(ctx)
    BuildContext.execute = override_build_state

