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
import subprocess

from waflib import Logs, Utils, Options
from cry_utils import WAF_EXECUTABLE

IB_REGISTRY_PATH = "Software\\Wow6432Node\\Xoreax\\Incredibuild\\Builder"


########################################################################################################
# Validate the Incredibuild Registry settings
def internal_validate_incredibuild_registry_settings(ctx):

    """ Helper function to verify the correct incredibuild settings """
    if Utils.unversioned_sys_platform() != 'win32':
        # Check windows registry only
        return False
        
    import _winreg

    if not ctx.is_option_true('use_incredibuild'):
        # No need to check IB settings if there is no IB
        return False

    allow_reg_updated = ctx.is_option_true('auto_update_incredibuild_settings') and \
                        not ctx.is_option_true('internal_dont_check_recursive_execution') and \
                        not Options.options.execsolution

    # Open the incredibuild settings registry key to validate if IB is installed properly
    try:
        ib_settings_read_only = _winreg.OpenKey(_winreg.HKEY_LOCAL_MACHINE, IB_REGISTRY_PATH, 0, _winreg.KEY_READ)
    except:
        Logs.debug('lumberyard: Cannot open registry entry "HKEY_LOCAL_MACHINE\\{}"'.format(IB_REGISTRY_PATH))
        Logs.warn('[WARNING] Incredibuild does not appear to be correctly installed on your machine.  Disabling Incredibuild.')
        return False

    def _read_ib_reg_setting(reg_key, setting_name, setting_path, expected_value):
        try:
            reg_data, reg_type = _winreg.QueryValueEx(reg_key, setting_name)
            return reg_data == expected_value
        except:
            Logs.debug('lumberyard: Cannot find a registry entry for "HKEY_LOCAL_MACHINE\\{}\\{}"'.format(setting_path,setting_name))
            return False

    def _write_ib_reg_setting(reg_key, setting_name, setting_path, value):
        try:
            _winreg.SetValueEx(reg_key, setting_name, 0, _winreg.REG_SZ, str(value))
            return True
        except WindowsError as e:
            Logs.warn('lumberyard: Unable write to HKEY_LOCAL_MACHINE\\{}\\{} : {}'.format(setting_path,setting_name,e.strerror))
            return False

    valid_ib_reg_key_values = [('PdbForwardingMode', '0'),
                               ('MaxConcurrentPDBs', '0'),
                               ('AllowDoubleTargets', '0')]

    is_ib_ready = True
    for settings_name, expected_value in valid_ib_reg_key_values:
        if is_ib_ready and not _read_ib_reg_setting(ib_settings_read_only, settings_name, IB_REGISTRY_PATH, expected_value):
            is_ib_ready = False

    # If we are IB ready, short-circuit out
    if is_ib_ready:
        return True

    # If we need updates, check if we have 'auto auto-update-incredibuild-settings' set or not
    if not allow_reg_updated:
        Logs.warn('[WARNING] The required settings for incredibuild is not properly configured. ')
        if not ctx.is_option_true('auto_update_incredibuild_settings'):
            Logs.warn("[WARNING]: Set the '--auto-update-incredibuild-settings' to True if you want to attempt to automatically update the settings")
        return False

    # if auto-update-incredibuild-settings is true, then attempt to update the values automatically
    try:
        ib_settings_writing = _winreg.OpenKey(_winreg.HKEY_LOCAL_MACHINE, IB_REGISTRY_PATH, 0,  _winreg.KEY_SET_VALUE |  _winreg.KEY_READ)
    except:
        Logs.warn('[WARNING] Cannot access a registry entry "HKEY_LOCAL_MACHINE\\{}" for writing.'.format(IB_REGISTRY_PATH))
        Logs.warn('[WARNING] Please run "{0}" as an administrator or change the value to "0" in the registry to ensure a correct operation of WAF'.format(WAF_EXECUTABLE) )
        return False

    # Once we get the key, attempt to update the values
    is_ib_updated = True
    for settings_name, set_value in valid_ib_reg_key_values:
        if is_ib_updated and not _write_ib_reg_setting(ib_settings_writing, settings_name, IB_REGISTRY_PATH, set_value):
            is_ib_updated = False
    if not is_ib_updated:
        Logs.warn('[WARNING] Unable to update registry settings for incredibuild')
        return False

    Logs.info('[INFO] Registry values updated for incredibuild')
    return True


########################################################################################################
def internal_use_incredibuild(ctx, section_name, option_name, value, verification_fn):
    """ If Incredibuild should be used, check for required packages """

    # GUI
    if not ctx.is_option_true('console_mode'):
        return ctx.gui_get_attribute(section_name, option_name, value)

    if not value or value != 'True':
        return value
    if not Utils.unversioned_sys_platform() == 'win32':
        return value

    _incredibuild_disclaimer(ctx)
    ctx.start_msg('Incredibuild Licence Check')
    (res, warning, error) = verification_fn(ctx, option_name, value)
    if not res:
        if warning:
            Logs.warn(warning)
        if error:
            ctx.end_msg(error, color='YELLOW')
        return 'False'

    ctx.end_msg('ok')
    return value


########################################################################################################
def _incredibuild_disclaimer(ctx):
    """ Helper function to show a disclaimer over incredibuild before asking for settings """
    if getattr(ctx, 'incredibuild_disclaimer_shown', False):
        return
    Logs.info('\nWAF is using Incredibuild for distributed Builds')
    Logs.info('To be able to compile with WAF, various licenses are required:')
    Logs.info('The "IncrediBuild for Make && Build Tools Package"   is always needed')
    Logs.info('The "IncrediBuild for PlayStation Package"          is needed for PS4 Builds')
    Logs.info('The "IncrediBuild for Xbox One Package"             is needed for Xbox One Builds')
    Logs.info('If some packages are missing, please ask IT')
    Logs.info('to assign the needed ones to your machine')

    ctx.incredibuild_disclaimer_shown = True


########################################################################################################
def internal_verify_incredibuild_license(licence_name, platform_name):
    """ Helper function to check if user has a incredibuild licence """
    try:
        result = subprocess.check_output(['xgconsole.exe', '/QUERYLICENSE'])
    except:
        error = '[ERROR] Incredibuild not found on system'
        return False, "", error

    if not licence_name in result:
        error = '[ERROR] Incredibuild on "%s" Disabled - Missing IB licence: "%s"' % (platform_name, licence_name)
        return False, "", error

    return True,"", ""

