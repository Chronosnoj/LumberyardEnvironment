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

#include <AzCore/EBus/EBus.h>
#include <AzToolsFramework/Debug/TraceContext.h>
#include <SceneAPI/SceneUI/RowWidgets/NodeTreeSelectionHandler.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace UI
        {
            AZ_CLASS_ALLOCATOR_IMPL(NodeTreeSelectionHandler, SystemAllocator, 0)

            NodeTreeSelectionHandler* NodeTreeSelectionHandler::s_instance = nullptr;

            QWidget* NodeTreeSelectionHandler::CreateGUI(QWidget* parent)
            {
                NodeTreeSelectionWidget* instance = aznew NodeTreeSelectionWidget(parent);
                connect(instance, &NodeTreeSelectionWidget::valueChanged, this,
                    [instance]()
                    {
                        EBUS_EVENT(AzToolsFramework::PropertyEditorGUIMessages::Bus, RequestWrite, instance);
                    });
                return instance;
            }

            u32 NodeTreeSelectionHandler::GetHandlerName() const
            {
                return AZ_CRC("NodeTreeSelection", 0x58649112);
            }

            bool NodeTreeSelectionHandler::AutoDelete() const
            {
                return false;
            }

            bool NodeTreeSelectionHandler::IsDefaultHandler() const
            {
                return true;
            }

            void NodeTreeSelectionHandler::ConsumeAttribute(NodeTreeSelectionWidget* widget, u32 attrib,
                AzToolsFramework::PropertyAttributeReader* attrValue, const char* debugName)
            {
                AZ_TraceContext("Attribute name", debugName);

                if (attrib == AZ_CRC("FilterName", 0xf49ce62e))
                {
                    ConsumeFilterNameAttribute(widget, attrValue);
                }
                else if (attrib == AZ_CRC("FilterType", 0x2661cf01))
                {
                    ConsumeFilterTypeAttribute(widget, attrValue);
                }
            }

            void NodeTreeSelectionHandler::WriteGUIValuesIntoProperty(size_t /*index*/, NodeTreeSelectionWidget* GUI,
                property_t& instance, AzToolsFramework::InstanceDataNode* /*node*/)
            {
                GUI->CopyListTo(instance);
            }

            bool NodeTreeSelectionHandler::ReadValuesIntoGUI(size_t /*index*/, NodeTreeSelectionWidget* GUI, const property_t& instance,
                AzToolsFramework::InstanceDataNode* /*node*/)
            {
                GUI->SetList(instance);
                return false;
            }

            void NodeTreeSelectionHandler::Register()
            {
                if (!s_instance)
                {
                    s_instance = aznew NodeTreeSelectionHandler();
                    EBUS_EVENT(AzToolsFramework::PropertyTypeRegistrationMessages::Bus, RegisterPropertyType, s_instance);
                }
            }

            void NodeTreeSelectionHandler::Unregister()
            {
                if (s_instance)
                {
                    EBUS_EVENT(AzToolsFramework::PropertyTypeRegistrationMessages::Bus, UnregisterPropertyType, s_instance);
                    delete s_instance;
                    s_instance = nullptr;
                }
            }

            void NodeTreeSelectionHandler::ConsumeFilterNameAttribute(NodeTreeSelectionWidget* widget, AzToolsFramework::PropertyAttributeReader* attrValue)
            {
                AZStd::string filterName;
                if (attrValue->Read<AZStd::string>(filterName))
                {
                    widget->SetFilterName(AZStd::move(filterName));
                }
                else
                {
                    AZ_Assert(false, "Failed to read string from 'FilterName' attribute.");
                }
            }

            void NodeTreeSelectionHandler::ConsumeFilterTypeAttribute(NodeTreeSelectionWidget* widget, AzToolsFramework::PropertyAttributeReader* attrValue)
            {
                Uuid filterType;
                if (attrValue->Read<Uuid>(filterType))
                {
                    widget->AddFilterType(filterType);
                }
                else
                {
                    AZ_Assert(false, "Failed to read uuid from 'FilterType' attribute.");
                }
            }
        } // UI
    } // SceneAPI
} // AZ

#include <RowWidgets/NodeTreeSelectionHandler.moc>