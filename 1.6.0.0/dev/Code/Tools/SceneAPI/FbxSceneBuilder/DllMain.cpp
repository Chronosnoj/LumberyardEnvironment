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

#if !defined(AZ_MONOLITHIC_BUILD)

#include <AzCore/Module/Environment.h>
#include <SceneAPI/FbxSceneBuilder/FbxImportRequestHandler.h>

static AZ::SceneAPI::FbxSceneImporter::FbxImportRequestHandler* fbxImporter = nullptr;

extern "C" AZ_DLL_EXPORT void InitializeDynamicModule(void* env)
{
    AZ::Environment::Attach(static_cast<AZ::EnvironmentInstance>(env));
    fbxImporter = aznew AZ::SceneAPI::FbxSceneImporter::FbxImportRequestHandler();
}

extern "C" AZ_DLL_EXPORT void UninitializeDynamicModule()
{
    delete fbxImporter;
    fbxImporter = nullptr;

    AZ::Environment::Detach();
}

#endif // !defined(AZ_MONOLITHIC_BUILD)
