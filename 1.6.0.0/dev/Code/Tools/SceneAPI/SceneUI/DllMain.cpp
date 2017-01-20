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

#if !defined(_LIB)

#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/EBus/EBus.h>
#include <AzCore/Module/Environment.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <SceneAPI/SceneUI/GraphMetaInfoHandler.h>
#include <SceneAPI/SceneUI/ManifestMetaInfoHandler.h>
#include <SceneAPI/SceneUI/RowWidgets/HeaderHandler.h>
#include <SceneAPI/SceneUI/RowWidgets/NodeListSelectionHandler.h>
#include <SceneAPI/SceneUI/RowWidgets/NodeTreeSelectionHandler.h>
#include <SceneAPI/SceneUI/RowWidgets/MultiLineTextEditHandler.h>
#include <SceneAPI/SceneUI/RowWidgets/ManifestVectorHandler.h>

static AZ::SceneAPI::UI::GraphMetaInfoHandler* g_graphMetaInfoHandler = nullptr;
static AZ::SceneAPI::UI::ManifestMetaInfoHandler* g_manifestMetaInfoHandler = nullptr;

extern "C" AZ_DLL_EXPORT void InitializeDynamicModule(void* env)
{
    AZ::Environment::Attach(static_cast<AZ::EnvironmentInstance>(env));

    AZ::SceneAPI::UI::HeaderHandler::Register();
    AZ::SceneAPI::UI::NodeListSelectionHandler::Register();
    AZ::SceneAPI::UI::NodeTreeSelectionHandler::Register();
    AZ::SceneAPI::UI::MultiLineTextEditHandler::Register();
    AZ::SceneAPI::UI::ManifestVectorHandler::Register();

    g_graphMetaInfoHandler = aznew AZ::SceneAPI::UI::GraphMetaInfoHandler();
    g_manifestMetaInfoHandler = aznew AZ::SceneAPI::UI::ManifestMetaInfoHandler();
}

extern "C" AZ_DLL_EXPORT void UninitializeDynamicModule()
{
    delete g_manifestMetaInfoHandler;
    g_manifestMetaInfoHandler = nullptr;

    delete g_graphMetaInfoHandler;
    g_graphMetaInfoHandler = nullptr;

    AZ::SceneAPI::UI::NodeTreeSelectionHandler::Unregister();
    AZ::SceneAPI::UI::NodeListSelectionHandler::Unregister();
    AZ::SceneAPI::UI::HeaderHandler::Unregister();
    AZ::SceneAPI::UI::MultiLineTextEditHandler::Unregister();
    AZ::SceneAPI::UI::ManifestVectorHandler::Unregister();

    AZ::Environment::Detach();
}

#endif // !defined(_LIB)
