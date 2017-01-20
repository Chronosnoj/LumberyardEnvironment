/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/
// Original file Copyright Crytek GMBH or its affiliates, used under license.

#include "StdAfx.h"
#include "CmdLine.h"
#include "Config.h"


//////////////////////////////////////////////////////////////////////////
static void AddParameterToConfig(Config* config, const char* parameter)
{
    // Split on key/value pair
    const string p = parameter;
    const size_t splitterPos = p.find('=');
    if (splitterPos != string::npos)
    {
        const string key = p.substr(0, splitterPos);
        const string value = p.substr(splitterPos + 1);
        if (!key.empty())
        {
            config->SetKeyValue(eCP_PriorityCmdline, key.c_str(), value.c_str());
        }
    }
    else
    {
        config->SetKeyValue(eCP_PriorityCmdline, p.c_str(), "");
    }
}


//////////////////////////////////////////////////////////////////////////
void CmdLine::Parse(const std::vector<string>& args, Config* config, string& fileSpec)
{
    assert(config);

    fileSpec.clear();

    for (int i = 1; i < (int)args.size(); ++i)
    {
        const char* const parameter = args[i].c_str();
        if (parameter[0] != '-' && parameter[0] != '/')
        {
            if (fileSpec.empty())
            {
                fileSpec = parameter;
            }
        }
        else
        {
            AddParameterToConfig(config, parameter + 1);
        }
    }
}
