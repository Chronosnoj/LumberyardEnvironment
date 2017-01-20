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

import sys
from waflib import Context, Utils, Logs
from waflib.Configure import conf
import sys
import os


def exec_command(cmd, **kw):
    subprocess = Utils.subprocess

    if isinstance(cmd, list):
        cmd = ' '.join(cmd)

    try:
        # warning: may deadlock with a lot of output (subprocess limitation)
        kw['stdout'] = kw['stderr'] = subprocess.PIPE
        p = subprocess.Popen(cmd, **kw)
        (out, err) = p.communicate()
        if out:
            out = out.decode(sys.stdout.encoding or 'iso8859-1')
        if err:
            err = err.decode(sys.stdout.encoding or 'iso8859-1')
        return p.returncode, out, err
    except OSError as e:
        return [-1, "OS Error", '%s - %s' % (str(e.errno), e.strerror)]
    except:
        return [-1, "Exception",  sys.exc_info()[0]]

@conf
def run_bootstrap_tool(self,ly_params,setup_assistant_third_party_override):
    bootstrap_script = None
    if (sys.platform == 'win32'):
        bootstrap_script = self.srcnode.find_resource('Code/Tools/AZInstaller/bootstrap.bat')
    elif (sys.platform.startswith('linux')):
        bootstrap_script = self.srcnode.find_resource('Code/Tools/AZInstaller/bootstrap_linux.sh')
    elif (sys.platform.startswith('darwin')):
        bootstrap_script = self.srcnode.find_resource('Code/Tools/AZInstaller/bootstrap_mac.sh')
    else:
        self.fatal('unknown platform in bootstrap tool')

    # if you have the source code to the Lumberyard Setup Assistant, attempt to compile it.
    # this will also be WAF eventually
    if (bootstrap_script is not None):
        Logs.info('Compiling Lumberyard Setup Assistant...')
        exit_code, out, err = exec_command('"' + bootstrap_script.abspath() + '"', shell=True)
        if exit_code != 0:
            failmessage = '\n'.join(['Lumberyard Setup Assistant compilation failed! Output:', str(out), str(err)])
            print(failmessage)
            self.fatal(failmessage)

    Logs.info('Running SetupAssistant.exe ...')

    if (sys.platform.startswith('linux')):
        ly_launcher = self.CreateRootRelativePath('BinLinux64/SetupAssistantBatch')
    else:
        ly_launcher = self.CreateRootRelativePath('Bin64/SetupAssistantBatch')
        
    if ly_params is None:
        ly_cmd = ['"' + ly_launcher + '"', '--none', '--enablecapability compilegame', '--enablecapability compileengine', '--enablecapability compilesandbox', '--no-modify-environment']
    else:
        ly_cmd = ['"' + ly_launcher + '"' , ly_params]

    if setup_assistant_third_party_override is not None and len(setup_assistant_third_party_override.strip())>0:
        if os.path.exists(setup_assistant_third_party_override):
            set_3p_folder = '--3rdpartypath \"{}\"'.format(setup_assistant_third_party_override)
            ly_cmd.append(set_3p_folder)
        else:
            Logs.warn('[WARN] 3rd Party Bootstrap option (--3rdpartypath=\"{}\") is not a valid path.  Ignoring.'.format(setup_assistant_third_party_override))

    exec_command(ly_cmd)
    exit_code, out, err = exec_command(ly_cmd, shell=True)
    if exit_code != 0:
        self.fatal('\n'.join(['Lumberyard Setup Assistant failed to run! Output:', str(out), str(err)]))

    Logs.info(out.strip())


