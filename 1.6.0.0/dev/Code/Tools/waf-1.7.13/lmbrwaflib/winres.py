#!/usr/bin/env python
# encoding: utf-8

# Modifications copyright Amazon.com, Inc. or its affiliates.

# Brant Young, 2007


"Process *.rc* files for C/C++: X{.rc -> [.res|.rc.o]}"

import re, traceback
from waflib import Task, Logs, Utils
from waflib.TaskGen import extension,feature, before_method
from waflib.Tools import c_preproc
from waflib.Configure import conf
import os

@extension('.rc')
def rc_file(self, node):
	"""
	Bind the .rc extension to a winrc task
	"""
	platform = self.bld.env['PLATFORM']
	if not platform.startswith('win'):
		return

	# Add the target name to prevent cases where the same project files are shared for different targets
	if hasattr(self, 'target'):
		target_name = self.target

	obj_ext = '.' + str(self.idx) + '.rc.o'
	if self.env['WINRC_TGT_F'] == '/fo':
		obj_ext = '.' + str(self.idx) + '.' + target_name + '.res'

	rctask = self.create_task('winrc', node, node.change_ext(obj_ext))

	try:
		self.compiled_tasks.append(rctask)
	except AttributeError:
		self.compiled_tasks = [rctask]

@conf
def load_rc_tool(conf):
	"""
	Detect the programs RC or windres, depending on the C/C++ compiler in use
	"""
	v = conf.env

	v['WINRC_TGT_F'] = '/fo'
	v['WINRC_SRC_F'] = ''

	v['WINRCFLAGS'] = [
		'/l0x0409', # Set default language
		'/nologo'	# Hide Logo
	]


RC_FILE_TEMPLATE='''// Microsoft Visual C++ generated resource script.
//
#include "resource.h"

#define APSTUDIO_READONLY_SYMBOLS
/////////////////////////////////////////////////////////////////////////////
//
// Generated from the TEXTINCLUDE 2 resource.
//
#include "afxres.h"

/////////////////////////////////////////////////////////////////////////////
#undef APSTUDIO_READONLY_SYMBOLS

/////////////////////////////////////////////////////////////////////////////
// Russian resources

#if !defined(AFX_RESOURCE_DLL) || defined(AFX_TARG_ENU)
LANGUAGE LANG_ENGLISH, SUBLANG_ENGLISH_US
#pragma code_page(1252)

#ifdef APSTUDIO_INVOKED
/////////////////////////////////////////////////////////////////////////////
//
// TEXTINCLUDE
//

1 TEXTINCLUDE
BEGIN
	"resource.h\\0"
END

2 TEXTINCLUDE
BEGIN
	"#include ""afxres.h""\\r\\n"
	"\\0"
END

3 TEXTINCLUDE
BEGIN
	"\\r\\n"
	"\\0"
END

#endif	  // APSTUDIO_INVOKED

${if hasattr(project, 'has_icons')}
// Icon with lowest ID value placed first to ensure application icon
// remains consistent on all systems.
IDI_ICON				ICON					"${project.icon_name}"
${endif}
#endif	  // English (United States) resources
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
// German (Germany) resources

#if !defined(AFX_RESOURCE_DLL) || defined(AFX_TARG_DEU)
LANGUAGE LANG_GERMAN, SUBLANG_GERMAN
#pragma code_page(1252)

/////////////////////////////////////////////////////////////////////////////
//
// Version
//

VS_VERSION_INFO VERSIONINFO
 FILEVERSION ${project.file_version}
 PRODUCTVERSION ${project.product_version}
 FILEFLAGSMASK 0x17L
#ifdef _DEBUG
 FILEFLAGS 0x1L
#else
 FILEFLAGS 0x0L
#endif
 FILEOS 0x4L
 FILETYPE 0x2L
 FILESUBTYPE 0x0L
BEGIN
	BLOCK "StringFileInfo"
	BEGIN
		BLOCK "000904b0"
		BEGIN
			VALUE "CompanyName", "${project.company_name}"
			VALUE "FileVersion", "${project.file_version}"
			VALUE "LegalCopyright", "${project.copyright}"
			VALUE "ProductName", "${project.product_name}"
			VALUE "ProductVersion", "${project.product_version}"
		END
	END
	BLOCK "VarFileInfo"
	BEGIN
		VALUE "Translation", 0x9, 1200
	END
END

#endif	  // German (Germany) resources
/////////////////////////////////////////////////////////////////////////////



#ifndef APSTUDIO_INVOKED
/////////////////////////////////////////////////////////////////////////////
//
// Generated from the TEXTINCLUDE 3 resource.
//


/////////////////////////////////////////////////////////////////////////////
#endif	  // not APSTUDIO_INVOKED
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

class create_rc_file(Task.Task):
	color = 'YELLOW'

	def get_file_content(self):
		'''
		Return file content. Returns cached value if called multiple times.
		'''

		def generate_file_content():
			tgen = self.generator
			bld = tgen.bld
			project_name = tgen.project_name if hasattr(tgen, 'project_name') else tgen.target

			self.company_name = bld.get_company_name()
			self.copyright = bld.get_copyright(project_name)
			self.product_name = bld.get_product_name(tgen.target, project_name)

			# Set special flags for launcher
			if getattr(tgen, 'is_launcher', False) or getattr(tgen, 'is_dedicated_server', False):
				self.has_icons = True
				self.icon_name = 'GameSDK.ico'
				version_string = bld.get_game_project_version()
			else:
				version_string = bld.get_lumberyard_version()

			self.product_version = version_string
			self.file_version = version_string
			self.file_version = self.file_version.replace('.',',') # Everywhere a ',' is accept except for file version


			template = compile_template(RC_FILE_TEMPLATE)
			self.file_content = template(self)

		# generate file_content
		if not hasattr(self, 'file_content'):
			generate_file_content()

		return self.file_content

	def run(self):
		self.outputs[0].write(self.get_file_content())

	def sig_vars(self):
		'''
		Use the generated file's contents in the task's signature.
		'''
		self.m.update(self.get_file_content())
		return super(create_rc_file, self).sig_vars()

@feature('generate_rc_file')
@before_method('process_source')
def generate_rc_file(self):
	platform = self.bld.env['PLATFORM']
	if not platform.startswith('win'):
		return

	# Generate Tasks to create rc file as well as copy the resources
	rc_task_inputs = []

	# Prevent collisions when multiple targets live in the same folder
	# by adding another folder based on target name
	rc_file_folder = self.target + '_rc'

	# declare target node using 'find_or_declare' so WAF notices if it's missing
	rc_file = self.path.find_or_declare([rc_file_folder, self.target + '.auto_gen.rc'])

	def _copy_rc_resource(file_name):
		source_file = self.resource_path.make_node(file_name)
		target_file = self.path.find_or_declare([rc_file_folder, file_name])
		self.create_task( 'copy_outputs', source_file, target_file )
		rc_task_inputs.append( target_file )

	required_rc_files = ['GameSDK.ico']
	for file in required_rc_files:
		if getattr(self, 'is_launcher', False):
			_copy_rc_resource(file)
		elif getattr(self, 'is_dedicated_server', False):
			_copy_rc_resource(file)

	self.create_task( 'create_rc_file', rc_task_inputs, rc_file )

	# create rc compile task
	self.rc_file(rc_file)


