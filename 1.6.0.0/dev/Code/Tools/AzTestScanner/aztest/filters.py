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
import json
import logging as lg
import os
import re
logger = lg.getLogger(__name__)


class FileApprover:
    """Class for compiling and validating the set of files that are allowed to be tested"""
    whitelist = None
    blacklist = None

    def __init__(self, whitelist_type="none", whitelist_file=None, blacklist_file=None, include_gems=False,
                 include_projects=False):
        self.make_whitelist(whitelist_type, whitelist_file, include_gems, include_projects)
        self.make_blacklist(blacklist_file)

    def make_whitelist(self, whitelist_type, whitelist_file, include_gems, include_projects):
        if whitelist_type == "none":
            self.whitelist = None
            return
        elif whitelist_type == "file" and whitelist_file:
            self.whitelist = self.get_patterns_from_file(os.path.abspath(whitelist_file))
        else:
            spec_file = whitelist_file if whitelist_file else self.get_default_spec_file()
            self.whitelist = self.get_whitelisted_modules_from_spec(os.path.abspath(spec_file))
        if include_projects:
            projects = self.get_project_names()
            self.whitelist |= set(projects)
        if include_gems:
            projects = self.get_project_names()
            self.whitelist |= set(self.get_gems_for_projects(projects))

    def make_blacklist(self, blacklist_file):
        if blacklist_file:
            self.blacklist = self.get_patterns_from_file(os.path.abspath(blacklist_file))
        else:
            self.blacklist = None

    def is_approved(self, file_name):
        return self.is_whitelisted(file_name) and not self.is_blacklisted(file_name)

    def is_whitelisted(self, file_name):
        return True if not self.whitelist else self.is_in_list(file_name, self.whitelist)

    def is_blacklisted(self, file_name):
        return False if not self.blacklist else self.is_in_list(file_name, self.blacklist)

    @staticmethod
    def get_default_spec_file():
        return os.path.abspath(os.path.join("_WAF_", "specs", "all.json"))

    @staticmethod
    def get_user_settings():
        return os.path.abspath(os.path.join("_WAF_", "user_settings.options"))

    def get_project_names(self):
        user_settings = self.get_user_settings()
        if not os.path.exists(user_settings):
            return None  # could not find user_settings.options, so return nothing
        projects = []
        with open(user_settings, 'r') as f:
            for line in f:
                if line.startswith("enabled_game_projects"):
                    projects = line.strip().replace(' ', '').split('=')[1].split(',')
        logger.info("Adding projects to whitelist: {}".format(", ".join(projects)))
        return projects

    @staticmethod
    def get_gems_for_projects(projects):
        gems = set()
        for project in projects:
            project_gems = os.path.join(project, "gems.json")
            if not os.path.exists(project_gems):
                continue  # skip projects that don't have a gems.json file
            with open(project_gems, 'r') as f:
                gems_json = json.load(f)
                for gem in gems_json["Gems"]:
                    gem_fullname = "Gem." + gem["_comment"] + "." + gem["Uuid"] + ".v" + gem["Version"]
                    gems.add(gem_fullname)
        logger.info("Adding gems to whitelist: {}".format(", ".join(gems)))
        return gems

    @staticmethod
    def get_whitelisted_modules_from_spec(spec_file):
        """Returns set of modules from spec_file if spec_file is valid, otherwise returns None"""
        if not os.path.exists(spec_file):
            logger.warning("Invalid spec file path: {}".format(spec_file))
            return None
        logger.info("Using spec file for whitelist: {}".format(spec_file))
        with open(spec_file, 'r') as f:
            specs = json.load(f)
        modules = {module for key in specs if key.endswith("modules") for module in specs[key]}
        return modules

    @staticmethod
    def get_patterns_from_file(pattern_file):
        """Returns set of patterns from pattern_file is pattern_file is valid, otherwise returns None"""
        if not os.path.exists(pattern_file):
            logger.warning("Invalid module/pattern file path: {}".format(pattern_file))
            return None
        logger.info("Using module/pattern file: {}".format(pattern_file))
        with open(pattern_file, 'r') as f:
            modules = set((line.strip() for line in f))
        return modules

    @staticmethod
    def is_in_list(file_name, patterns):
        for pattern in patterns:
            full_pattern = r"^.*\\{}(\.dll|\.exe|\.dylib)?$".format(pattern)
            match = re.match(full_pattern, file_name)
            if match:
                return True
        return False