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
from waflib.Configure import conf, Logs

# being a _host file, this means that these settings apply to any build at all that is
# being done from this kind of host

@conf
def load_linux_x64_host_settings(conf):
    """
    Setup any environment settings you want to apply globally any time the host doing the building is linux x64
    """
    v = conf.env

    bin64 = conf.get_output_folders('linux_x64', 'profile')[0]
    azcg_dir = bin64.make_node('azcg').abspath()

    v['MOVECOMMAND'] = 'mv -f'
    v['TONULL'] = '> /dev/null'
    v['CODE_GENERATOR_EXECUTABLE'] = 'AzCodeGenerator'
    v['CODE_GENERATOR_PATH'] = [ azcg_dir ]
    v['CODE_GENERATOR_PYTHON_PATHS'] = ['Tools/Python/2.7.11/linux_x64/lib/python2.7', 'Tools/Python/2.7.11/linux_x64/lib/python2.7/lib-dynload', 'Code/SDKs/markupsafe/x64', 'Code/SDKs/jinja2/x64']
    v['CODE_GENERATOR_PYTHON_DEBUG_PATHS'] = ['Tools/Python/2.7.11/linux_x64/lib/python2.7', 'Tools/Python/2.7.11/linux_x64/lib/python2.7/lib-dynload', 'Code/SDKs/markupsafe/x64', 'Code/SDKs/jinja2/x64']
    v['CODE_GENERATOR_PYTHON_HOME'] = 'Tools/Python/2.7.11/linux_x64'
    v['CODE_GENERATOR_PYTHON_HOME_DEBUG'] = 'Tools/Python/2.7.11/linux_x64'
