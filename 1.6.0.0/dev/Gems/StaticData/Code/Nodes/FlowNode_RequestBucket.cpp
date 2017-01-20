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
#include <StdAfx.h>
#include <Nodes/FlowNode_RequestBucket.h>
#include <LmbrAWS/ILmbrAWS.h>
#include <LmbrAWS/IAWSClientManager.h>
#include <StaticData/StaticDataBus.h>

using namespace Aws;
using namespace Aws::Lambda;
using namespace Aws::Client;

namespace LmbrAWS
{
    namespace FlowNode_RequestBucketInternal
    {
        static const char* CLASS_TAG = "AWS:StaticData:RequestBucket";
    }

    FlowNode_RequestBucket::FlowNode_RequestBucket(SActivationInfo* activationInfo) : BaseMaglevFlowNode< eNCT_Singleton >(activationInfo)
    {        
    }

    Aws::Vector<SInputPortConfig> FlowNode_RequestBucket::GetInputPorts() const
    {
        static const Aws::Vector<SInputPortConfig> inputPortConfiguration =
        {
            InputPortConfig_Void("RequestBucket", "Request an update for a given bucket or all monitored buckets"),
            InputPortConfig<string>("BucketName", "Name of bucket to request update for (blank for all)"),
        };

        return inputPortConfiguration;
    }

    Aws::Vector<SOutputPortConfig> FlowNode_RequestBucket::GetOutputPorts() const
    {
        static const Aws::Vector<SOutputPortConfig> outputPorts = {
            OutputPortConfig<string>("Finished", _HELP("Finished sending request")),
        };
        return outputPorts;
    }

    void FlowNode_RequestBucket::ProcessEvent_Internal(EFlowEvent event, SActivationInfo* activationInfo)
    {
        if (event == eFE_Activate && IsPortActive(activationInfo, EIP_RequestBucket))
        {
            bool requestSuccess = false;
            auto& bucketName = GetPortString(activationInfo, EIP_BucketName);
            if (bucketName.length())
            {
                EBUS_EVENT_RESULT(requestSuccess, CloudCanvas::StaticData::StaticDataRequestBus, RequestBucket, bucketName.c_str());
            }
            else
            {
                EBUS_EVENT_RESULT(requestSuccess, CloudCanvas::StaticData::StaticDataRequestBus, RequestAllBuckets);
            }

            if (requestSuccess)
            {
                SFlowAddress addr(activationInfo->myID, EOP_Finished, true);
                activationInfo->pGraph->ActivatePort(addr, 0);
                SuccessNotify(activationInfo->pGraph, activationInfo->myID);
            }
            else
            {
                ErrorNotify(activationInfo->pGraph, activationInfo->myID, "Failed to request bucket update");
            }
        }
    }

    REGISTER_CLASS_TAG_AND_FLOW_NODE(FlowNode_RequestBucketInternal::CLASS_TAG, FlowNode_RequestBucket);
}