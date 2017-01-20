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

#include "StdAfx.h"
#include <FlowSystem/Nodes/FlowBaseNode.h>
#include <Metastream/MetastreamBus.h>
#include <sstream>

class CFlowNode_MetastreamCacheData
    : public CFlowBaseNode < eNCT_Singleton >
{
    enum InputPorts
    {
        InputPorts_Activate,
        InputPorts_Table,
        InputPorts_Key,
        InputPorts_Value,
    };

    enum OutputPorts
    {
        OutputPorts_Out,
        OutputPorts_Error,
    };

public:

    explicit CFlowNode_MetastreamCacheData(IFlowNode::SActivationInfo*)
    {
    }

    ~CFlowNode_MetastreamCacheData() override
    {
    }

    void GetConfiguration(SFlowNodeConfig& config) override
    {
        static const SInputPortConfig inputs[] =
        {
            InputPortConfig_Void("Activate", _HELP("Exposes the value through Metastream")),
            InputPortConfig<string>("Table", _HELP("Table name inside which this key-value pair will be stored")),
            InputPortConfig<string>("Key", _HELP("Key name for this value")),
            InputPortConfig_AnyType("Value", _HELP("Value to expose")),
            { 0 }
        };
        static const SOutputPortConfig outputs[] =
        {
            OutputPortConfig_Void("Out", _HELP("Signalled when done")),
            OutputPortConfig<bool>("Error", _HELP("Signalled if an error has occurred")),
            { 0 }
        };

        config.pInputPorts = inputs;
        config.pOutputPorts = outputs;
        config.sDescription = _HELP("Exposes data through the Metastream Gem");
        config.SetCategory(EFLN_APPROVED);
    }

    void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo) override
    {
        switch (event)
        {
        case eFE_Initialize:
            break;

        case eFE_Activate:
            if (IsPortActive(pActInfo, InputPorts_Activate))
            {
                string table = GetPortString(pActInfo, InputPorts_Table);
                string key = GetPortString(pActInfo, InputPorts_Key);
                string value;

                const TFlowInputData& rawValue = GetPortAny(pActInfo, InputPorts_Value);
                switch (rawValue.GetType())
                {
                case eFDT_Bool:
                case eFDT_Double:
                case eFDT_EntityId:
                case eFDT_Float:
                case eFDT_FlowEntityId:
                case eFDT_Int:
                case eFDT_Pointer:
                {
                    // Convert bools, ints, floats, doubles, etc directly into their string equivalent
                    rawValue.GetValueWithConversion<string>(value);
                    break;
                }
                case eFDT_Vec3:
                {
                    // Construct an array representing the vector
                    Vec3 vecValue;
                    rawValue.GetValueWithConversion<Vec3>(vecValue);
                    std::stringstream jsonValue;
                    jsonValue << "[" << vecValue.x << "," << vecValue.y << "," << vecValue.z << "]";
                    value = jsonValue.str().c_str();
                    break;
                }
                case eFDT_String:
                default: 
                {
                    // Construct a JSON-compliant string by enclosing the value in quotes
                    string strValue;
                    rawValue.GetValueWithConversion<string>(strValue);
                    std::stringstream jsonValue;
                    jsonValue << "\"" << strValue << "\"";
                    value = jsonValue.str().c_str();
                    break;
                }
                }

                // Add the value to Metastream's cache
                EBUS_EVENT(Metastream::MetastreamRequestBus, AddToCache, table.c_str(), key.c_str(), value.c_str());

                // Activate "Out" pin to signal success
                ActivateOutput(pActInfo, OutputPorts_Out, true);
            }

            break;
        }
    }

    void GetMemoryUsage(ICrySizer*) const override
    {
    }
};

REGISTER_FLOW_NODE("Metastream:CacheData", CFlowNode_MetastreamCacheData);