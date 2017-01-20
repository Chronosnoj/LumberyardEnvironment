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
from collections import defaultdict


class RunnerReturnCodes:
    """ Defines return and error codes from running the test scanner.
    """
    TESTS_SUCCEEDED = 0
    TESTS_FAILED = 1

    INCORRECT_USAGE = 101
    FAILED_TO_LOAD_LIBRARY = 102
    SYMBOL_NOT_FOUND = 103

    UNEXPECTED_EXCEPTION = 900
    NTSTATUS_BREAKPOINT = -2147483645L

    @staticmethod
    def to_string(return_code):
        return defaultdict(lambda: "Unknown return code", {
            RunnerReturnCodes.TESTS_SUCCEEDED: None,
            RunnerReturnCodes.TESTS_FAILED: "Tests Failed",

            RunnerReturnCodes.INCORRECT_USAGE: "Incorrect usage of test runner",
            RunnerReturnCodes.FAILED_TO_LOAD_LIBRARY: "Failed to load library",
            RunnerReturnCodes.SYMBOL_NOT_FOUND: "Symbol not found",

            RunnerReturnCodes.UNEXPECTED_EXCEPTION: "Unexpected Exception while scanning module",
            RunnerReturnCodes.NTSTATUS_BREAKPOINT: "NTSTATUS: STATUS_BREAKPOINT (0x80000003)",
        })[return_code]
