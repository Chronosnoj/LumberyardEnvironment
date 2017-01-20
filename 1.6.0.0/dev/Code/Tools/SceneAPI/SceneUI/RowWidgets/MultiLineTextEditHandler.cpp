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

#include <SceneAPI/SceneUI/RowWidgets/MultiLineTextEditHandler.h>
#include <AzToolsFramework/Debug/TraceContext.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace UI
        {
            AZ_CLASS_ALLOCATOR_IMPL(MultiLineTextEditHandler, SystemAllocator, 0)
            MultiLineTextEditHandler* MultiLineTextEditHandler::s_instance = nullptr;

            QWidget* MultiLineTextEditHandler::CreateGUI(QWidget* parent)
            {
                GrowTextEdit* textEdit = aznew GrowTextEdit(parent);
                connect(textEdit, &GrowTextEdit::textChanged, this, 
                    [textEdit]()
                    {
                        EBUS_EVENT(AzToolsFramework::PropertyEditorGUIMessages::Bus, RequestWrite, textEdit);
                    }
                );

                return textEdit;
            }

            u32 MultiLineTextEditHandler::GetHandlerName() const
            {
                return AZ_CRC("MultiLineEdit", 0xf5d93777);
            }

            bool MultiLineTextEditHandler::AutoDelete() const
            {
                return false;
            }
                
            void MultiLineTextEditHandler::ConsumeAttribute(GrowTextEdit* GUI, u32 attrib, AzToolsFramework::PropertyAttributeReader* attrValue, const char* debugName)
            {
                AZ_TraceContext("Attribute name", debugName);

                AZStd::string placeholderText;
                if (attrib == AZ_CRC("PlaceholderText", 0xa23ec278) && attrValue->Read<AZStd::string>(placeholderText))
                {
                    GUI->setPlaceholderText(placeholderText.c_str());
                }
            }

            void MultiLineTextEditHandler::WriteGUIValuesIntoProperty(size_t /*index*/, GrowTextEdit* GUI, property_t& instance, AzToolsFramework::InstanceDataNode* /*node*/)
            {
                instance = GUI->GetText();
            }

            bool MultiLineTextEditHandler::ReadValuesIntoGUI(size_t /*index*/, GrowTextEdit* GUI, const property_t& instance, AzToolsFramework::InstanceDataNode* /*node*/)
            {
                GUI->SetText(instance);
                return true;
            }

            void MultiLineTextEditHandler::Register()
            {
                if (!s_instance)
                {
                    s_instance = aznew MultiLineTextEditHandler();
                    EBUS_EVENT(AzToolsFramework::PropertyTypeRegistrationMessages::Bus, RegisterPropertyType, s_instance);
                }
            }

            void MultiLineTextEditHandler::Unregister()
            {
                if (s_instance)
                {
                    EBUS_EVENT(AzToolsFramework::PropertyTypeRegistrationMessages::Bus, UnregisterPropertyType, s_instance);
                    delete s_instance;
                    s_instance = nullptr;
                }
            }
        }
    }
}

#include <RowWidgets/MultiLineTextEditHandler.moc>