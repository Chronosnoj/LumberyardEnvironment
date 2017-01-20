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

#include "Precompiled.h"
#include "EditorFlowGraphComponent.h"

#include <AzCore/EBus/EBus.h>
#include <AzCore/std/parallel/mutex.h>
#include <FlowGraphInformation.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Serialization/Utils.h>
#include <AzCore/XML/rapidxml.h>
#include <AzCore/XML/rapidxml_print.h>
#include <AzCore/IO/TextStreamWriters.h>
#include <AzToolsFramework/API/ToolsApplicationAPI.h>
#include <AzToolsFramework/API/HyperGraphBus.h>
#include "FlowGraphComponent.h"

#include "../../CryEngine/CryAction/FlowSystem/FlowData.h"

#include "Include/EditorCoreAPI.h"
#include "../../../Editor/IEditor.h"
#include "../../../Editor/IconManager.h"

class CEntityObject;

namespace LmbrCentral
{
    bool EditorFlowGraphWrapperDataConverter(AZ::SerializeContext& context, AZ::SerializeContext::DataElementNode& classElement);

    class EditorFlowGraphComponentSerializationEvents
        : public AZ::SerializeContext::IEventHandler
    {
        void OnReadBegin(void* classPtr) override
        {
            EditorFlowGraphComponent* component = reinterpret_cast<EditorFlowGraphComponent*>(classPtr);
            for (FlowGraphWrapper& wrapper : component->m_flowGraphs)
            {
                wrapper.OnBeforeSave();
            }
        }
    };

    void EditorFlowGraphComponent::DisplayEntity(bool& handled)
    {
        // Draw extra visualization when not selected.
        if (!IsSelected())
        {
            handled = true;
        }
    }

    void EditorFlowGraphComponent::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);

        FlowGraphWrapper::Reflect(context);

        if (serializeContext)
        {
            serializeContext->Class<EditorFlowGraphComponent, EditorComponentBase>()
                ->Version(2)
                ->EventHandler<EditorFlowGraphComponentSerializationEvents>()
                ->Field("FlowGraphs", &EditorFlowGraphComponent::m_flowGraphs);

            AZ::EditContext* editContext = serializeContext->GetEditContext();

            if (editContext)
            {
                editContext->Class<EditorFlowGraphComponent>(
                    "Flow Graph", "Allows an Entity to own a FlowGraph")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::Category, "Scripting")
                        ->Attribute(AZ::Edit::Attributes::Icon, "Editor/Icons/Components/FlowGraph.png")
                        ->Attribute(AZ::Edit::Attributes::ViewportIcon, "Editor/Icons/Components/Viewport/Flowgraph.png")
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                        ->Attribute(AZ::Edit::Attributes::Visibility, AZ_CRC("PropertyVisibility_Hide"));
            }
        }
    }

    void EditorFlowGraphComponent::Init()
    {
        EditorComponentBase::Init();

        // Create each FlowGraph from its XML representation.
        for (FlowGraphWrapper& flowgraphWrapper : m_flowGraphs)
        {
            if (!flowgraphWrapper.m_flowGraph)
            {
                if (flowgraphWrapper.m_flowGraphXML.empty())
                {
                    flowgraphWrapper.m_flowGraphXML = flowgraphWrapper.m_serializedFlowGraph.GenerateLegacyXml(GetEntityId());
                }

                flowgraphWrapper.m_flowGraph = FlowGraphUtility::CreateFlowGraphFromXML(GetEntityId(), flowgraphWrapper.m_flowGraphXML);
                flowgraphWrapper.m_flowGraphName = flowgraphWrapper.m_serializedFlowGraph.m_name;
            }
        }
    }

    void EditorFlowGraphComponent::Activate()
    {
        EditorComponentBase::Activate();
        FlowGraphEditorRequestsBus::Handler::BusConnect(FlowEntityId(GetEntityId()));
        AzFramework::EntityDebugDisplayEventBus::Handler::BusConnect(GetEntityId());

        AZ::ComponentApplicationEventBus::Handler::BusConnect();

        for (FlowGraphWrapper& flowGraphWrapper : m_flowGraphs)
        {
            if (!flowGraphWrapper.m_isReady && flowGraphWrapper.m_flowGraph)
            {
                EBUS_EVENT(FlowGraphNotificationBus, ComponentFlowGraphLoaded,
                    flowGraphWrapper.m_flowGraph.get(),
                    GetEntityId(),
                    flowGraphWrapper.m_flowGraphName,
                    flowGraphWrapper.m_flowGraphXML,
                    false);

                EBUS_EVENT(AzToolsFramework::HyperGraphRequestBus, RegisterHyperGraphEntityListener, flowGraphWrapper.m_flowGraph.get(), &flowGraphWrapper);
                flowGraphWrapper.m_isReady = true;
            }
        }
    }

    void EditorFlowGraphComponent::Deactivate()
    {
        AZ::ComponentApplicationEventBus::Handler::BusDisconnect();

        FlowGraphEditorRequestsBus::Handler::BusDisconnect();
        EditorComponentBase::Deactivate();

        AzFramework::EntityDebugDisplayEventBus::Handler::BusDisconnect();
    }

    EditorFlowGraphComponent::~EditorFlowGraphComponent()
    {
        m_flowGraphs.clear();
    }

    void EditorFlowGraphComponent::BuildGameEntity(AZ::Entity* gameEntity)
    {
        FlowGraphComponent* flowGraphComponent = gameEntity->CreateComponent<FlowGraphComponent>();

        if (flowGraphComponent)
        {
            for (FlowGraphWrapper& flowGraphWrapper : m_flowGraphs)
            {
                // Update FG serialization data before passing to game component.
                flowGraphWrapper.OnBeforeSave();

                FlowGraphComponent::FlowGraphData flowGraphData;
                flowGraphData.m_flowGraphData = flowGraphWrapper.m_serializedFlowGraph;
                flowGraphComponent->AddFlowGraphData(flowGraphData);
            }
        }
    }

    //////////////////////////////////////////////////////////////////////////

    void EditorFlowGraphComponent::GetFlowGraphs(AZStd::vector<AZStd::pair<AZStd::string, IFlowGraph*> >& flowGraphs)
    {
        flowGraphs.clear();
        for (auto& flowGraph : m_flowGraphs)
        {
            AZStd::string hyperGraphName;
            EBUS_EVENT_RESULT(hyperGraphName, AzToolsFramework::HyperGraphRequestBus, GetHyperGraphName, flowGraph.m_flowGraph.get());
            AZ_Error("EditorFlowGraphComponent", !hyperGraphName.empty(), "Failed to find HyperGraph version of FlowGraph\n");
            flowGraphs.push_back(AZStd::make_pair<AZStd::string, IFlowGraph*>(hyperGraphName.c_str(), flowGraph.m_flowGraph));
        }
    }

    void EditorFlowGraphComponent::OpenFlowGraphView(IFlowGraph* flowGraph)
    {
        EBUS_EVENT(AzToolsFramework::HyperGraphRequestBus, OpenHyperGraphView, flowGraph);
    }

    IFlowGraph* EditorFlowGraphComponent::AddFlowGraph(const AZStd::string& flowGraphName)
    {
        m_flowGraphs.emplace_back(FlowGraphWrapper(FlowEntityId(GetEntityId()), flowGraphName));
        SetDirty();
        return m_flowGraphs.back().m_flowGraph;
    }

    void EditorFlowGraphComponent::RemoveFlowGraph(IFlowGraph* flowGraph, bool shouldPromptUser)
    {
        if (shouldPromptUser)
        {
            QMessageBox mb(QMessageBox::Question, "Remove Flow Graph", QString("Are you sure?"), QMessageBox::Yes | QMessageBox::No);
            if (mb.exec() != QMessageBox::Yes)
            {
                return;
            }
        }
        for (auto& flowgraph : m_flowGraphs)
        {
            if (flowgraph.m_flowGraph.get() == flowGraph)
            {
                m_flowGraphs.remove(flowgraph);
                break;
            }
        }

        SetDirty();

        // If we have removed all flowgraphs, we'll remove the component as well.
        if (m_flowGraphs.empty())
        {
            EBUS_EVENT(AzToolsFramework::ToolsApplicationRequests::Bus, RemoveComponentFromEntity, GetEntityId(), GetId());
        }
    }

    /////////////////////////////////////////////////////////////////////////

    void FlowGraphWrapper::OnNameChanged(const char* pName)
    {
        EBUS_EVENT(AzToolsFramework::HyperGraphRequestBus, SetHyperGraphGroupName, m_flowGraph.get(), pName);
    }

    FlowGraphWrapper::FlowGraphWrapper()
    {
    }

    FlowGraphWrapper::FlowGraphWrapper(const AZ::EntityId& id, const AZStd::string& flowGraphName)
    {
        m_flowGraph = FlowGraphUtility::CreateFlowGraphForEntity(id);

        EBUS_EVENT(FlowGraphNotificationBus, ComponentFlowGraphAdded,
            m_flowGraph,
            id,
            flowGraphName.c_str());

        m_flowGraphName = flowGraphName;

        m_isReady = true;
    }

    FlowGraphWrapper::FlowGraphWrapper(FlowGraphWrapper&& rhs)
    {
        if (this == &rhs)
        {
            return;
        }

        m_flowGraph = rhs.m_flowGraph;
        m_flowGraphName = AZStd::move(rhs.m_flowGraphName);
        m_isReady = rhs.m_isReady;

        EBUS_EVENT(AzToolsFramework::HyperGraphRequestBus, RegisterHyperGraphEntityListener, m_flowGraph.get(), this);

        rhs.m_flowGraph = nullptr;
    }

    FlowGraphWrapper::~FlowGraphWrapper()
    {
        if (m_flowGraph)
        {
            EBUS_EVENT(AzToolsFramework::HyperGraphRequestBus, UnregisterHyperGraphEntityListener, m_flowGraph.get(), this);

            EBUS_EVENT(FlowGraphNotificationBus, ComponentFlowGraphRemoved, m_flowGraph);
            m_flowGraph->Uninitialize();

            EBUS_EVENT(AzToolsFramework::HyperGraphRequestBus, ReleaseHyperGraph, m_flowGraph.get());
        }
    }

    void FlowGraphWrapper::SetEditorFlowGraphEntity(const AZ::EntityId& id)
    {
        EBUS_EVENT(AzToolsFramework::HyperGraphRequestBus, SetHyperGraphEntity, m_flowGraph.get(), id);
    }

    void FlowGraphWrapper::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);

        if (serializeContext)
        {
            serializeContext->Class<FlowGraphWrapper>()
                ->Version(2, &EditorFlowGraphWrapperDataConverter)
                ->Field("SerializedFlowGraph", &FlowGraphWrapper::m_serializedFlowGraph)
            ;
        }
    }

    void FlowGraphWrapper::OnBeforeSave()
    {
        if (m_flowGraph)
        {
            EBUS_EVENT(AzToolsFramework::HyperGraphRequestBus, BuildSerializedFlowGraph, m_flowGraph.get(), m_serializedFlowGraph);
            m_serializedFlowGraph.m_name = m_flowGraphName;
        }
    }

    AZStd::mutex s_flowGraphDataConverterMutex;

    bool EditorFlowGraphWrapperDataConverter(AZ::SerializeContext& context, AZ::SerializeContext::DataElementNode& classElement)
    {
        // Flow graph editor facilities are not thread-safe. Because data conversion requires we interact
        // with it, and from arbitrary job threads, locking is necessary for safety.
        AZStd::lock_guard<AZStd::mutex> lock(s_flowGraphDataConverterMutex);

        AZStd::string flowGraphName;

        int index = classElement.FindElement(AZ_CRC("FlowGraphName"));
        if (index >= 0)
        {
            flowGraphName.resize(classElement.GetSubElement(index).GetRawDataElement().m_byteStream.GetLength());
            classElement.GetSubElement(index).GetRawDataElement().m_byteStream.Read(flowGraphName.size(), flowGraphName.begin());
            classElement.GetSubElement(index).GetRawDataElement().m_byteStream.Seek(0, AZ::IO::GenericStream::ST_SEEK_BEGIN);

            classElement.RemoveElement(index);
        }

        index = classElement.FindElement(AZ_CRC("NodeInfoMap"));
        if (index >= 0)
        {
            classElement.RemoveElement(index);
        }

        index = classElement.FindElement(AZ_CRC("OwningEntityId"));
        if (index >= 0)
        {
            classElement.RemoveElement(index);
        }

        if (!flowGraphName.empty())
        {
            AZStd::string xmlBuffer;

            // Parse the old Xml data that was stored as raw stream (formerly a custom serializer).
            AZ::IO::GenericStream* stream = classElement.GetRawDataElement().m_stream;
            if (stream)
            {
                xmlBuffer.resize(stream->GetLength());
                stream->Seek(0, AZ::IO::GenericStream::ST_SEEK_BEGIN);
                stream->Read(xmlBuffer.size(), xmlBuffer.begin());
                stream->Seek(0, AZ::IO::GenericStream::ST_SEEK_BEGIN);
            }

            // Wipe the old XML buffer.
            classElement.GetRawDataElement().m_buffer.clear();
            classElement.GetRawDataElement().m_dataSize = 0;

            if (!xmlBuffer.empty())
            {
                // Create a temporary graph from the Xml data, which is necessary for building the serialized structure.
                IFlowGraphPtr tempGraph = FlowGraphUtility::CreateFlowGraphFromXML(AZ::EntityId(), xmlBuffer);
                if (tempGraph)
                {
                    EBUS_EVENT(FlowGraphNotificationBus, ComponentFlowGraphLoaded,
                        tempGraph.get(),
                        AZ::EntityId(),
                        flowGraphName,
                        xmlBuffer,
                        true /* temporary graph, don't trigger FG editor UI refresh */);

                    SerializedFlowGraph graph;
                    EBUS_EVENT(AzToolsFramework::HyperGraphRequestBus, BuildSerializedFlowGraph, tempGraph.get(), graph);

                    EBUS_EVENT(FlowGraphNotificationBus, ComponentFlowGraphRemoved, tempGraph);
                    tempGraph->Uninitialize();
                    tempGraph = nullptr;

                    const int graphIndex = classElement.AddElement<SerializedFlowGraph>(context, "SerializedFlowGraph");
                    if (graphIndex >= 0)
                    {
                        AZ::SerializeContext::DataElementNode& graphNode = classElement.GetSubElement(graphIndex);
                        return graphNode.SetData(context, graph);
                    }
                }
            }
        }

        return false;
    }
} // namespace LmbrCentral
