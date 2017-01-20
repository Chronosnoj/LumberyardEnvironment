#!/usr/bin/env python
# encoding: utf-8
# Thomas Nagy, 2006-2015 (ita)

# Modifications copyright Amazon.com, Inc. or its affiliates.

"""

Tool Description
================

This tool helps with finding Qt5 tools and libraries,
and also provides syntactic sugar for using Qt5 tools.

The following snippet illustrates the tool usage::

    def options(opt):
        opt.load('compiler_cxx qt5')

    def configure(conf):
        conf.load('compiler_cxx qt5')

    def build(bld):
        bld(
            features = ['qt5','cxx','cxxprogram'],
            uselib   = ['QTCORE','QTGUI','QTOPENGL','QTSVG'],
            source   = 'main.cpp textures.qrc aboutDialog.ui',
            target   = 'window',
        )

Here, the UI description and resource files will be processed
to generate code.

Usage
=====

Load the "qt5" tool.

You also need to edit your sources accordingly:

- the normal way of doing things is to have your C++ files
  include the .moc file.
  This is regarded as the best practice (and provides much faster
  compilations).
  It also implies that the include paths have beenset properly.

- to have the include paths added automatically, use the following::

     from waflib.TaskGen import feature, before_method, after_method
     @feature('cxx')
     @after_method('process_source')
     @before_method('apply_incpaths')
     def add_includes_paths(self):
        incs = set(self.to_list(getattr(self, 'includes', '')))
        for x in self.compiled_tasks:
            incs.add(x.inputs[0].parent.path_from(self.path))
        self.includes = list(incs)

Note: another tool provides Qt processing that does not require
.moc includes, see 'playground/slow_qt/'.

A few options (--qt{dir,bin,...}) and environment variables
(QT5_{ROOT,DIR,MOC,UIC,XCOMPILE}) allow finer tuning of the tool,
tool path selection, etc; please read the source for more info.

"""

try:
    from xml.sax import make_parser
    from xml.sax.handler import ContentHandler
except ImportError:
    has_xml = False
    ContentHandler = object
else:
    has_xml = True

import os, sys, re
from waflib.Tools import cxx
from waflib import Task, Utils, Options, Errors, Context
from waflib.TaskGen import feature, after_method, extension, before_method
from waflib.Configure import conf
from waflib import Logs
from generate_uber_files import UBER_HEADER_COMMENT
import branch_spec

MOC_H = ['.h', '.hpp', '.hxx', '.hh']
"""
File extensions associated to the .moc files
"""

EXT_RCC = ['.qrc']
"""
File extension for the resource (.qrc) files
"""

EXT_UI  = ['.ui']
"""
File extension for the user interface (.ui) files
"""

EXT_QT5 = ['.cpp', '.cc', '.cxx', '.C']
"""
File extensions of C++ files that may require a .moc processing
"""

QT5_LIBS = '''
qtmain
Qt5Bluetooth
Qt5CLucene
Qt5Concurrent
Qt5Core
Qt5DBus
Qt5Declarative
Qt5DesignerComponents
Qt5Designer
Qt5Gui
Qt5Help
Qt5MultimediaQuick_p
Qt5Multimedia
Qt5MultimediaWidgets
Qt5Network
Qt5Nfc
Qt5OpenGL
Qt5Positioning
Qt5PrintSupport
Qt5Qml
Qt5QuickParticles
Qt5Quick
Qt5QuickTest
Qt5Script
Qt5ScriptTools
Qt5Sensors
Qt5SerialPort
Qt5Sql
Qt5Svg
Qt5Test
Qt5WebKit
Qt5WebKitWidgets
Qt5Widgets
Qt5WinExtras
Qt5X11Extras
Qt5XmlPatterns
Qt5Xml'''


# Search pattern to find the required #include <*.moc> lines in the source code to identify the header files that need
# moc'ing.  The path of the moc file must be relative to the current project root
INCLUDE_MOC_RE = re.compile(r'\s*\#include\s+[\"<](.*.moc)[\">]',flags=re.MULTILINE)
INCLUDE_SRC_RE = re.compile(r'\s*\#include\s+[\"<](.*.(cpp|cxx|cc))[\">]',flags=re.MULTILINE)

# Flag to shorten the generated paths used for the qt5 generated files under the working folder.  If true, that means
# each project cannot use any QT generated artifacts that do no exist within its project boundaries.  If we want to
# allow QT artifacts that are shared across different project folders (which is not ideal), then set this flag to False
RESTRICT_BINTEMP_QT_PATH=True

# Derive a specific moc_files.<idx> folder name based on the base bldnode and idx
def get_target_qt5_root(bld_node, target_name, idx):
    base_qt_node = bld_node.make_node('qt5/{}.{}'.format(target_name,idx))
    return base_qt_node

# Change a target node from a changed extension to one marked as QT code generated
def change_target_qt5_node(bld_node, project_path, target_name, target_node,idx):

    if RESTRICT_BINTEMP_QT_PATH:
        relpath_target = target_node.relpath()
        relpath_project = project_path.relpath()
        restricted_path = relpath_target.replace(relpath_project,'')
        target_node_subdir = os.path.dirname(restricted_path)
    else:
        target_node_subdir = target_node.bld_dir()

    # Change the output target to the specific moc file folder
    output_qt_dir = get_target_qt5_root(bld_node, target_name, idx).make_node(target_node_subdir)
    output_qt_dir.mkdir()

    output_qt_node = output_qt_dir.make_node(target_node.name)
    return output_qt_node


class qxx(Task.classes['cxx']):
    """
    Each C++ file can have zero or several .moc files to create.
    They are known only when the files are scanned (preprocessor)
    To avoid scanning the c++ files each time (parsing C/C++), the results
    are retrieved from the task cache (bld.node_deps/bld.raw_deps).
    The moc tasks are also created *dynamically* during the build.
    """

    def __init__(self, *k, **kw):
        Task.Task.__init__(self, *k, **kw)

        if 'qt5' in self.generator.features and self.env.QMAKE:
            # If QT5 is enabled, then signal that moc scanning is needed
            self.moc_done = 0
        else:
            # Otherwise, signal that moc scanning can be skipped
            self.moc_done = 1

        self.dep_moc_files = {}


    def runnable_status(self):
        """
        Compute the task signature to make sure the scanner was executed. Create the
        moc tasks by using :py:meth:`waflib.Tools.qt5.qxx.add_moc_tasks` (if necessary),
        then postpone the task execution (there is no need to recompute the task signature).
        """
        if self.moc_done:
            # If there were moc tasks that were spawned by this task, then make sure that the generated moc files
            # actually exist.  There appears to be an issue with incredibuild and highly parallelized builds where
            # even though the moc tasks reported that it was complete, the actual moc files have not been transfered
            # to the target location
            if len(self.dep_moc_files)>0:
                for dep_moc_file_path in iter(self.dep_moc_files):
                    # Make sure that the path exists
                    if not os.path.exists(dep_moc_file_path.abspath()):
                        return Task.ASK_LATER
                    # Even if the path exists, check to see if we can load it
                    if not self.dep_moc_files[dep_moc_file_path]:
                        try:
                            file_content = dep_moc_file_path.read()
                            file_content_len = len(file_content)
                            del file_content
                            self.dep_moc_files[dep_moc_file_path] = True
                            if file_content_len == 0:
                                return Task.ASK_LATER
                        except:
                            return Task.ASK_LATER

            return Task.Task.runnable_status(self)
        else:
            for t in self.run_after:
                if not t.hasrun:
                    return Task.ASK_LATER
            # Do not create tasks for project_generation builds
            self.add_moc_tasks()
            # At this point, the moc task should be done, recycle and try the status check again
            self.moc_done = 1

            return self.runnable_status()

    def create_moc_task(self, h_node, m_node):
        """
        If several libraries use the same classes, it is possible that moc will run several times (Issue 1318)
        It is not possible to change the file names, but we can assume that the moc transformation will be identical,
        and the moc tasks can be shared in a global cache.

        The defines passed to moc will then depend on task generator order. If this is not acceptable, then
        use the tool slow_qt5 instead (and enjoy the slow builds... :-( )
        """

        cache_key = '{}.{}'.format(h_node.abspath(),self.generator.idx)

        try:
            moc_cache = self.generator.bld.moc_cache
        except AttributeError:
            moc_cache = self.generator.bld.moc_cache = {}

        try:
            return moc_cache[cache_key]
        except KeyError:

            target_node = change_target_qt5_node(self.generator.bld.bldnode,
                                                 self.generator.path,
                                                 self.generator.name,
                                                 m_node,
                                                 self.generator.idx)

            tsk = moc_cache[cache_key] = Task.classes['moc'](env=self.env, generator=self.generator)
            tsk.set_inputs(h_node)
            tsk.set_outputs(target_node)

            self.dep_moc_files[target_node] = False

            if self.generator:
                self.generator.tasks.append(tsk)

            # direct injection in the build phase (safe because called from the main thread)
            gen = self.generator.bld.producer
            gen.outstanding.insert(0, tsk)
            gen.total += 1

            return tsk

        else:
            # remove the signature, it must be recomputed with the moc task
            delattr(self, 'cache_sig')

    def moc_h_ext(self):
        try:
            ext = Options.options.qt_header_ext.split()
        except AttributeError:
            pass
        if not ext:
            ext = MOC_H
        return ext

    def add_moc_tasks(self):

        node = self.inputs[0]
        src_scan = node.read()

        # Determine if this is an uber file to see if we need to go one level deeper
        if src_scan.startswith(UBER_HEADER_COMMENT):
            # This is an uber file, handle uber files differently
            self.add_moc_task_uber(node,src_scan)
        else:
            # Process the source file (for mocs)
            self.add_moc_tasks_for_node(node,src_scan)

        del src_scan #free up the text as soon as possible

    def scan_node_contents_for_moc_tasks(self,node_contents):

        base_node = self.generator.path
        include_moc_node_rel_paths = INCLUDE_MOC_RE.findall(node_contents)
        moctasks = []

        for include_moc_node_rel_path in include_moc_node_rel_paths:

            base_name = include_moc_node_rel_path[:-4]

            # We are only allowing to include mocing header files that are relative to the project folder
            header_node = None
            for moc_ext in self.moc_h_ext():
                header_node = base_node.find_node('{}{}'.format(base_name,moc_ext))
                if header_node:
                    break
            if header_node:
                moc_node = header_node.change_ext('.moc')

            if not header_node:
                raise Errors.WafError('No source found for {} which is a moc file'.format(base_name))

            # create the moc task
            task = self.create_moc_task(header_node, moc_node)
            moctasks.append(task)

        return moctasks

    def add_moc_task_uber(self, node, node_contents):
        '''
        Handle uber files by grepping for all the includes of source files and performing the moc scanning there
        '''
        moctasks = []
        include_source_rel_paths = INCLUDE_SRC_RE.findall(node_contents)
        for include_source_rel_path,include_source_extension in include_source_rel_paths:
            source_node = node.parent.find_node(include_source_rel_path)
            if source_node is not None:
                source_node_contents = source_node.read()
                moctasks += self.scan_node_contents_for_moc_tasks(source_node_contents)

                del source_node_contents #free up the text as soon as possible

        # simple scheduler dependency: run the moc task before others
        self.run_after.update(set(moctasks))
        self.moc_done = 1


    def add_moc_tasks_for_node(self, node, node_contents):
        '''
        Create the moc tasks greping the source file for all the #includes
        '''

        moctasks = self.scan_node_contents_for_moc_tasks(node_contents)

        # simple scheduler dependency: run the moc task before others
        self.run_after.update(set(moctasks))
        self.moc_done = 1

class trans_update(Task.Task):
    """Update a .ts files from a list of C++ files"""
    run_str = '${QT_LUPDATE} ${SRC} -ts ${TGT}'
    color   = 'BLUE'
Task.update_outputs(trans_update)

class XMLHandler(ContentHandler):
    """
    Parser for *.qrc* files
    """
    def __init__(self):
        self.buf = []
        self.files = []
    def startElement(self, name, attrs):
        if name == 'file':
            self.buf = []
    def endElement(self, name):
        if name == 'file':
            self.files.append(str(''.join(self.buf)))
    def characters(self, cars):
        self.buf.append(cars)

@extension(*EXT_RCC)
def create_rcc_task(self, node):
    "Create rcc and cxx tasks for *.qrc* files"

    # Do not create tasks for project_generation builds
    if self.env['PLATFORM'] == 'project_generator':
        return None
    # Do not perform any task if QMAKE is not installed
    if not self.env.QMAKE:
        return None

    # For QRC Processing, we cannot make the generated rcc file from the qrc source as a separate compile unit
    # when creating static libs.  It appears that MSVC will optimize the required static methods required to
    # initialize the resources for the static lib.  In order to work around this, the generated file from the
    # qrc will need to be created as a header and included into a cpp that is consumed by the app/shared library
    # that is linking against it
    is_static_lib = 'stlib' == getattr(self,'_type','')

    if is_static_lib:
        rcnode = change_target_qt5_node(self.bld.bldnode,
                                        self.path,
                                        self.name,
                                        node.parent.find_or_declare('rcc_%s.h' % node.name[:-4]),
                                        self.idx)

        qrc_task = self.create_task('rcc', node, rcnode)
        return qrc_task
    else:
        rcnode = change_target_qt5_node(self.bld.bldnode,
                                        self.path,
                                        self.name,
                                        node.change_ext('_rc.cpp'), self.idx)

        qrc_task = self.create_task('rcc', node, rcnode)
        cpptask = self.create_task('cxx', rcnode, rcnode.change_ext('.o'))
        cpptask.dep_nodes.append(node)
        try:
            self.compiled_tasks.append(cpptask)
        except AttributeError:
            self.compiled_tasks = [cpptask]
        return cpptask

@extension(*EXT_UI)
def create_uic_task(self, node):
    "hook for uic tasks"

    # Do not create tasks for project_generation builds
    if self.env['PLATFORM'] == 'project_generator':
        return None
    if not self.env.QMAKE:
        return None

    uictask = self.create_task('ui5', node)

    target_node = change_target_qt5_node(self.bld.bldnode,
                                         self.path,
                                         self.name,
                                         node.parent.find_or_declare(self.env['ui_PATTERN'] % node.name[:-3]),
                                         self.idx)
    uictask.outputs = [target_node]

@extension('.ts')
def add_lang(self, node):
    """add all the .ts file into self.lang"""
    self.lang = self.to_list(getattr(self, 'lang', [])) + [node]

@feature('qt5')
@before_method('apply_incpaths')
def apply_qt5_includes(self):

    # Make sure the QT is enabled, otherwise whatever module is using this feature will fail
    if not self.env.QMAKE:
        return

    base_moc_node = get_target_qt5_root(self.bld.bldnode,
                                        self.name,
                                        self.idx)
    if not hasattr(self, 'includes'):
        self.includes = []
    if RESTRICT_BINTEMP_QT_PATH:
        self.includes.append(base_moc_node)
    else:
        self.includes.append(base_moc_node.make_node(self.path.relpath()))


@feature('qt5')
@after_method('apply_link')
def apply_qt5(self):
    """
    Add MOC_FLAGS which may be necessary for moc::

        def build(bld):
            bld.program(features='qt5', source='main.cpp', target='app', use='QTCORE')

    The additional parameters are:

    :param lang: list of translation files (\*.ts) to process
    :type lang: list of :py:class:`waflib.Node.Node` or string without the .ts extension
    :param update: whether to process the C++ files to update the \*.ts files (use **waf --translate**)
    :type update: bool
    :param langname: if given, transform the \*.ts files into a .qrc files to include in the binary file
    :type langname: :py:class:`waflib.Node.Node` or string without the .qrc extension
    """
    # Make sure the QT is enabled, otherwise whatever module is using this feature will fail
    if not self.env.QMAKE:
        return

    if getattr(self, 'lang', None):
        qmtasks = []
        for x in self.to_list(self.lang):
            if isinstance(x, str):
                x = self.path.find_resource(x + '.ts')
            qmtasks.append(self.create_task('ts2qm', x, x.change_ext('.qm')))

        if getattr(self, 'update', None) and Options.options.trans_qt5:
            cxxnodes = [a.inputs[0] for a in self.compiled_tasks] + [
                a.inputs[0] for a in self.tasks if getattr(a, 'inputs', None) and a.inputs[0].name.endswith('.ui')]
            for x in qmtasks:
                self.create_task('trans_update', cxxnodes, x.inputs)

        if getattr(self, 'langname', None):
            qmnodes = [x.outputs[0] for x in qmtasks]
            rcnode = self.langname
            if isinstance(rcnode, str):
                rcnode = self.path.find_or_declare(rcnode + '.qrc')
            t = self.create_task('qm2rcc', qmnodes, rcnode)
            k = create_rcc_task(self, t.outputs[0])

            if k:
                self.link_task.inputs.append(k.outputs[0])

    lst = []
    for flag in self.to_list(self.env['CXXFLAGS']):
        if len(flag) < 2: continue
        f = flag[0:2]
        if f in ('-D', '-I', '/D', '/I'):
            if f[0] == '/':
                lst.append('-' + flag[1:])
            else:
                lst.append(flag)

    if len(self.env['DEFINES']) > 0:
        for defined_value in self.env['DEFINES']:
            lst.append( '-D'+defined_value )

    # Apply additional QT defines for all MOCing
    additional_flags = ['-DQT_LARGEFILE_SUPPORT',
                        '-DQT_DLL',
                        '-DQT_CORE_LIB',
                        '-DQT_GUI_LIB']

    for additional_flag in  additional_flags:
        if not lst.__contains__(additional_flag):
            lst.append(additional_flag)


    # Instruct MOC.exe to not generate an #include statement to the header file in the output.
    # This will require that all files that include a .moc file must also include the .h file as well
    lst.append('-i')

    self.env.append_value('MOC_FLAGS', lst)


@extension(*EXT_QT5)
def cxx_hook(self, node):
    """
    Re-map C++ file extensions to the :py:class:`waflib.Tools.qt5.qxx` task.
    """
    return self.create_compiled_task('qxx', node)


# QT tasks involve code generation, so we need to also check if the generated code is still there
class QtTask(Task.Task):

    def runnable_status(self):

        missing_output = False
        for output in self.outputs:
            if not os.path.exists(output.abspath()):
                missing_output = True
                break

        if missing_output:
            for t in self.run_after:
                if not t.hasrun:
                    return Task.ASK_LATER
            return Task.RUN_ME

        status = Task.Task.runnable_status(self)
        return status


class rcc(QtTask):
    """
    Process *.qrc* files
    """
    color   = 'BLUE'
    run_str = '${QT_RCC} -name ${tsk.rcname()} ${SRC[0].abspath()} ${RCC_ST} -o ${TGT}'
    ext_in = ['.qrc']
    ext_out = ['.cpp']

    def rcname(self):
        return os.path.splitext(self.inputs[0].name)[0]

    def scan(self):
        """Parse the *.qrc* files"""
        if not has_xml:
            Logs.error('no xml support was found, the rcc dependencies will be incomplete!')
            return ([], [])

        parser = make_parser()
        curHandler = XMLHandler()
        parser.setContentHandler(curHandler)
        fi = open(self.inputs[0].abspath(), 'r')
        try:
            parser.parse(fi)
        finally:
            fi.close()

        nodes = []
        names = []
        root = self.inputs[0].parent
        for x in curHandler.files:
            nd = root.find_resource(x)
            if nd: nodes.append(nd)
            else: names.append(x)
        return (nodes, names)

class moc(QtTask):
    """
    Create *.moc* files
    """
    color   = 'BLUE'

    run_str = '"${MOC}" ${MOC_FLAGS} ${SRC} > ${TGT}_moctmp && ${MOVECOMMAND} ${TGT}_moctmp ${TGT} ${TONULL}'


class ui5(QtTask):
    """
    Process *.ui* files
    """
    color   = 'BLUE'
    run_str = '${QT_UIC} ${SRC} -o ${TGT}'
    ext_in = ['.ui']
    ext_out = ['.h']

class ts2qm(QtTask):
    """
    Create *.qm* files from *.ts* files
    """
    color   = 'BLUE'
    run_str = '${QT_LRELEASE} ${QT_LRELEASE_FLAGS} ${SRC} -qm ${TGT}'

class qm2rcc(QtTask):
    """
    Transform *.qm* files into *.rc* files
    """
    color = 'BLUE'
    after = 'ts2qm'

    def run(self):
        """Create a qrc file including the inputs"""
        txt = '\n'.join(['<file>%s</file>' % k.path_from(self.outputs[0].parent) for k in self.inputs])
        code = '<!DOCTYPE RCC><RCC version="1.0">\n<qresource>\n%s\n</qresource>\n</RCC>' % txt
        self.outputs[0].write(code)

def configure(self):
    """
    Besides the configuration options, the environment variable QT5_ROOT may be used
    to give the location of the qt5 libraries (absolute path).

    The detection will use the program *pkg-config* through :py:func:`waflib.Tools.config_c.check_cfg`
    """

    # If we cannot find the QT5 library, then do not continue looking for the rest of the QT properties
    if not self.find_qt5_binaries():
        Logs.warn('[WARNING] Unable to find the appropriate QT library.  Make sure you have QT installed and you selected the '
                  'appropriate options to build the Lumberyard engine, otherwise you will not be able to compile the QT dependent '
                  'components.')
        return

    self.set_qt5_libs_to_check()
    self.set_qt5_defines()
    self.find_qt5_libraries()
    self.add_qt5_rpath()
    self.simplify_qt5_libs()

@conf
def find_qt5_binaries(self):

    env = self.env

    opt = Options.options
    qtdir = getattr(opt, 'qtdir', '')
    qtbin = getattr(opt, 'qtbin', '')

    if not qtdir:
        qtdir = os.path.normpath(self.CreateRootRelativePath('Code/Sandbox/SDKs/Qt'))

    paths = []

    if qtdir:
        qtbin = os.path.join(qtdir, 'bin')

    # the qt directory has been given from QT5_ROOT - deduce the qt binary path
    if not qtdir:
        qtdir = os.environ.get('QT5_ROOT', '')
        qtbin = os.environ.get('QT5_BIN', None) or os.path.join(qtdir, 'bin')

    if qtbin:
        paths = [qtbin]

    # no qtdir, look in the path and in /usr/local/Trolltech
    if not qtdir:
        paths = os.environ.get('PATH', '').split(os.pathsep)
        paths.append('/usr/share/qt5/bin/')
        try:
            lst = Utils.listdir('/usr/local/Trolltech/')
        except OSError:
            pass
        else:
            if lst:
                lst.sort()
                lst.reverse()

                # keep the highest version
                qtdir = '/usr/local/Trolltech/%s/' % lst[0]
                qtbin = os.path.join(qtdir, 'bin')
                paths.append(qtbin)

    # at the end, try to find qmake in the paths given
    # keep the one with the highest version
    cand = None
    prev_ver = ['5', '0', '0']
    for qmk in ('qmake-qt5', 'qmake5', 'qmake'):
        try:
            qmake = self.find_program(qmk, path_list=paths)
        except self.errors.ConfigurationError:
            pass
        else:
            try:
                version = self.cmd_and_log([qmake] + ['-query', 'QT_VERSION']).strip()
            except self.errors.WafError:
                pass
            else:
                if version:
                    new_ver = version.split('.')
                    if new_ver > prev_ver:
                        cand = qmake
                        prev_ver = new_ver

    # qmake could not be found easily, rely on qtchooser
    if not cand:
        try:
            self.find_program('qtchooser')
        except self.errors.ConfigurationError:
            pass
        else:
            cmd = [self.env.QTCHOOSER] + ['-qt=5', '-run-tool=qmake']
            try:
                version = self.cmd_and_log(cmd + ['-query', 'QT_VERSION'])
            except self.errors.WafError:
                pass
            else:
                cand = cmd

    if cand:
        self.env.QMAKE = cand
    else:
        # If we cannot find qmake, we will assume that QT is not available or a selected option
        return False

    self.env.QT_INSTALL_BINS = qtbin = self.cmd_and_log([self.env.QMAKE] + ['-query', 'QT_INSTALL_BINS']).strip() + os.sep
    paths.insert(0, qtbin)

    def find_bin(lst, var):
        if var in env:
            return
        for f in lst:
            try:
                ret = self.find_program(f, path_list=paths)
            except self.errors.ConfigurationError:
                pass
            else:
                env[var]=ret
                break

    find_bin(['uic-qt5', 'uic'], 'QT_UIC')
    if not env.QT_UIC:
        # If we find qmake but not the uic compiler, then the QT installation is corrupt/invalid
        self.fatal('Detected an invalid/corrupt version of QT, please check your installation')

    self.start_msg('Checking for uic version')
    uicver = self.cmd_and_log([env.QT_UIC] + ['-version'], output=Context.BOTH)
    uicver = ''.join(uicver).strip()
    uicver = uicver.replace('Qt User Interface Compiler ','').replace('User Interface Compiler for Qt', '')
    self.end_msg(uicver)
    if uicver.find(' 3.') != -1 or uicver.find(' 4.') != -1:
        self.fatal('this uic compiler is for qt3 or qt5, add uic for qt5 to your path')

    find_bin(['moc-qt5', 'moc'], 'QT_MOC')
    find_bin(['rcc-qt5', 'rcc'], 'QT_RCC')
    find_bin(['lrelease-qt5', 'lrelease'], 'QT_LRELEASE')
    find_bin(['lupdate-qt5', 'lupdate'], 'QT_LUPDATE')

    env['UIC_ST'] = '%s -o %s'
    env['MOC_ST'] = '-o'
    env['ui_PATTERN'] = 'ui_%s.h'
    env['QT_LRELEASE_FLAGS'] = ['-silent']
    env.MOCCPPPATH_ST = '-I%s'
    env.MOCDEFINES_ST = '-D%s'

    return True

@conf
def find_qt5_libraries(self):
    qtlibs = getattr(Options.options, 'qtlibs', None) or os.environ.get("QT5_LIBDIR", None)
    if not qtlibs:
        try:
            qtlibs = self.cmd_and_log([self.env.QMAKE] + ['-query', 'QT_INSTALL_LIBS']).strip()
        except Errors.WafError:
            qtdir = self.cmd_and_log([self.env.QMAKE] + ['-query', 'QT_INSTALL_PREFIX']).strip() + os.sep
            qtlibs = os.path.join(qtdir, 'lib')
    self.msg('Found the Qt5 libraries in', qtlibs)


    qtincludes =  os.environ.get("QT5_INCLUDES", None) or self.cmd_and_log([self.env.QMAKE] + ['-query', 'QT_INSTALL_HEADERS']).strip()
    env = self.env
    if not 'PKG_CONFIG_PATH' in os.environ:
        os.environ['PKG_CONFIG_PATH'] = '%s:%s/pkgconfig:/usr/lib/qt5/lib/pkgconfig:/opt/qt5/lib/pkgconfig:/usr/lib/qt5/lib:/opt/qt5/lib' % (qtlibs, qtlibs)

    if Utils.unversioned_sys_platform() == "darwin":
        if qtlibs:
            env.append_unique('FRAMEWORKPATH',qtlibs)

    # Keep track of platforms that were checked (there is no need to do a multiple report)
    checked_darwin = False
    checked_linux = False
    checked_win_x64 = False

    validated_platforms = branch_spec.get_validated_platforms(self)
    for validated_platform in validated_platforms:

        is_platform_darwin = validated_platform in (['darwin_x64', 'ios', 'appletv'])
        is_platform_linux = validated_platform in (['linux_x64_gcc'])
        is_platform_win_x64 = validated_platform.startswith('win_x64')

        for i in self.qt5_vars:
            uselib = i.upper()

            # Platform is darwin_x64 / mac
            if is_platform_darwin:

                # QT for darwin does not have '5' in the name, so we need to remove it
                darwin_adjusted_name = i.replace('Qt5','Qt')

                # Since at least qt 4.7.3 each library locates in separate directory
                frameworkName = darwin_adjusted_name + ".framework"
                qtDynamicLib = os.path.join(qtlibs, frameworkName, darwin_adjusted_name)
                if os.path.exists(qtDynamicLib):
                    env.append_unique('FRAMEWORK_{}_{}'.format(validated_platform,uselib), darwin_adjusted_name)
                    if not checked_darwin:
                        self.msg('Checking for %s' % i, qtDynamicLib, 'GREEN')
                else:
                    if not checked_darwin:
                        self.msg('Checking for %s' % i, False, 'YELLOW')
                env.append_unique('INCLUDES_{}_{}'.format(validated_platform,uselib), os.path.join(qtlibs, frameworkName, 'Headers'))

                # Detect the debug versions of the library
                uselib_debug = i.upper() + "D"
                darwin_adjusted_name_debug = '{}_debug'.format(darwin_adjusted_name)
                qtDynamicLib_debug = os.path.join(qtlibs, frameworkName, darwin_adjusted_name_debug)
                if os.path.exists(qtDynamicLib_debug):
                    env.append_unique('FRAMEWORK_{}_{}'.format(validated_platform, uselib_debug), darwin_adjusted_name)
                    if not checked_darwin:
                        self.msg('Checking for %s_debug' % i, qtDynamicLib_debug, 'GREEN')
                else:
                    if not checked_darwin:
                        self.msg('Checking for %s_debug' % i, False, 'YELLOW')
                env.append_unique('INCLUDES_{}_{}'.format(validated_platform,uselib_debug), os.path.join(qtlibs, frameworkName, 'Headers'))

            # Platform is linux+gcc
            elif is_platform_linux:

                qtDynamicLib = os.path.join(qtlibs, "lib" + i + ".so")
                qtStaticLib = os.path.join(qtlibs, "lib" + i + ".a")
                if os.path.exists(qtDynamicLib):
                    env.append_unique('LIB_{}_{}'.format(validated_platform,uselib), i)
                    if not checked_linux:
                        self.msg('Checking for %s' % i, qtDynamicLib, 'GREEN')
                elif os.path.exists(qtStaticLib):
                    env.append_unique('LIB_{}_{}'.format(validated_platform,uselib), i)
                    if not checked_linux:
                        self.msg('Checking for %s' % i, qtStaticLib, 'GREEN')
                else:
                    if not checked_linux:
                        self.msg('Checking for %s' % i, False, 'YELLOW')

                env.append_unique('LIBPATH_{}_{}'.format(validated_platform,uselib), qtlibs)
                env.append_unique('INCLUDES_{}_{}'.format(validated_platform,uselib), qtincludes)
                env.append_unique('INCLUDES_{}_{}'.format(validated_platform,uselib), os.path.join(qtincludes, i))

            # Platform is win_x64
            elif is_platform_win_x64:
                # Release library names are like QtCore5
                for k in ("lib%s.a", "lib%s5.a", "%s.lib", "%s5.lib"):
                    lib = os.path.join(qtlibs, k % i)
                    if os.path.exists(lib):
                        env.append_unique('LIB_{}_{}'.format(validated_platform,uselib), i + k[k.find("%s") + 2 : k.find('.')])
                        if not checked_win_x64:
                            self.msg('Checking for %s' % i, lib, 'GREEN')
                        break
                else:
                    if not checked_win_x64:
                        self.msg('Checking for %s' % i, False, 'YELLOW')

                env.append_unique('LIBPATH_{}_{}'.format(validated_platform,uselib), qtlibs)
                env.append_unique('INCLUDES_{}_{}'.format(validated_platform,uselib), qtincludes)
                env.append_unique('INCLUDES_{}_{}'.format(validated_platform,uselib), os.path.join(qtincludes, i.replace('Qt5', 'Qt')))

                # Debug library names are like QtCore5d
                uselib = i.upper() + "D"
                for k in ("lib%sd.a", "lib%sd5.a", "%sd.lib", "%sd5.lib"):
                    lib = os.path.join(qtlibs, k % i)
                    if os.path.exists(lib):
                        env.append_unique('LIB_{}_{}'.format(validated_platform,uselib), i + k[k.find("%s") + 2 : k.find('.')])
                        if not checked_win_x64:
                            self.msg('Checking for %s' % i, lib, 'GREEN')
                        break
                else:
                    if not checked_win_x64:
                        self.msg('Checking for %s' % i, False, 'YELLOW')

                env.append_unique('LIBPATH_{}_{}'.format(validated_platform,uselib), qtlibs)
                env.append_unique('INCLUDES_{}_{}'.format(validated_platform,uselib), qtincludes)
                env.append_unique('INCLUDES_{}_{}'.format(validated_platform,uselib), os.path.join(qtincludes, i.replace('Qt5', 'Qt')))
            else:
                # The current target platform is not supported for QT5
                Logs.debug('lumberyard: QT5 detection not supported for platform {}'.format(validated_platform))
                pass

        if is_platform_darwin:
            checked_darwin = True
        elif is_platform_linux:
            checked_linux = True
        elif is_platform_win_x64:
            checked_win_x64 = True

@conf
def simplify_qt5_libs(self):
    # the libpaths make really long command-lines
    # remove the qtcore ones from qtgui, etc
    env = self.env
    def process_lib(vars_, coreval):
        validated_platforms = branch_spec.get_validated_platforms(self)
        for validated_platform in validated_platforms:
            for d in vars_:
                var = d.upper()
                if var == 'QTCORE':
                    continue

                value = env['LIBPATH_{}_{}'.format(validated_platform, var)]
                if value:
                    core = env[coreval]
                    accu = []
                    for lib in value:
                        if lib in core:
                            continue
                        accu.append(lib)
                    env['LIBPATH_{}_{}'.format(validated_platform, var)] = accu

    process_lib(self.qt5_vars,       'LIBPATH_QTCORE')
    process_lib(self.qt5_vars_debug, 'LIBPATH_QTCORE_DEBUG')

@conf
def add_qt5_rpath(self):
    # rpath if wanted
    env = self.env
    if getattr(Options.options, 'want_rpath', False):
        def process_rpath(vars_, coreval):

            validated_platforms = branch_spec.get_validated_platforms(self)
            for validated_platform in validated_platforms:
                for d in vars_:
                    var = d.upper()
                    value = env['LIBPATH_{}_{}'.format(validated_platform, var)]
                    if value:
                        core = env[coreval]
                        accu = []
                        for lib in value:
                            if var != 'QTCORE':
                                if lib in core:
                                    continue
                            accu.append('-Wl,--rpath='+lib)
                        env['RPATH_{}_{}'.format(validated_platform, var)] = accu
        process_rpath(self.qt5_vars,       'LIBPATH_QTCORE')
        process_rpath(self.qt5_vars_debug, 'LIBPATH_QTCORE_DEBUG')

@conf
def set_qt5_libs_to_check(self):
    if not hasattr(self, 'qt5_vars'):
        self.qt5_vars = QT5_LIBS
    self.qt5_vars = Utils.to_list(self.qt5_vars)
    if not hasattr(self, 'qt5_vars_debug'):
        self.qt5_vars_debug = [a + '_debug' for a in self.qt5_vars]
    self.qt5_vars_debug = Utils.to_list(self.qt5_vars_debug)

@conf
def set_qt5_defines(self):
    if sys.platform != 'win32':
        return

    validated_platforms = branch_spec.get_validated_platforms(self)
    for validated_platform in validated_platforms:
        for x in self.qt5_vars:
            y=x.replace('Qt5', 'Qt')[2:].upper()
            self.env.append_unique('DEFINES_{}_{}'.format(validated_platform,x.upper()), 'QT_%s_LIB' % y)
            self.env.append_unique('DEFINES_{}_{}_DEBUG'.format(validated_platform,x.upper()), 'QT_%s_LIB' % y)

def options(opt):
    """
    Command-line options
    """
    opt.add_option('--want-rpath', action='store_true', default=False, dest='want_rpath', help='enable the rpath for qt libraries')

    opt.add_option('--header-ext',
        type='string',
        default='',
        help='header extension for moc files',
        dest='qt_header_ext')

    for i in 'qtdir qtbin qtlibs'.split():
        opt.add_option('--'+i, type='string', default='', dest=i)

    opt.add_option('--translate', action="store_true", help="collect translation strings", dest="trans_qt5", default=False)

