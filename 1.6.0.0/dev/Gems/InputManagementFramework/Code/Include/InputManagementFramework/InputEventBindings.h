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
#pragma once
#include <AzCore/RTTI/TypeInfo.h>
#include <AzFramework/Asset/SimpleAsset.h>
#include "InputSubComponent.h"
#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/Memory/Memory.h>
#include "InputEventGroup.h"

namespace Input
{
    /*!
    * InputEventBinding asset type configuration.
    * Reflect as: AzFramework::SimpleAssetReference<InputEventBindings>
    * This base class holds a list of InputEventGroups which organizes raw input processors by the 
    * gameplay events they generate, Ex. Held(eKI_Space) -> "Jump"
    */
    class InputEventBindings
    {
    public:
        virtual ~InputEventBindings()
        {
        }

        AZ_CLASS_ALLOCATOR(InputEventBindings, AZ::SystemAllocator, 0);
        AZ_RTTI(InputEventBindings, "{14FFD4A8-AE46-4E23-B45B-6A7C4F787A91}")
       
        static void Reflect(AZ::ReflectContext* reflection)
        {
            AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(reflection);
            if (serializeContext)
            {
                serializeContext->Class<InputEventBindings>()
                    ->Version(1)
                    ->Field("Input Event Groups", &InputEventBindings::m_inputEventGroups);

                AZ::EditContext* editContext = serializeContext->GetEditContext();
                if (editContext)
                {
                    editContext->Class<InputEventBindings>("Input Event Bindings", "Holds InputEventBindings")
                        ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->DataElement(0, &InputEventBindings::m_inputEventGroups, "Input Event Groups", "Input Event Groups");
                }
            }
        }

        void Activate(const AZ::EntityId& sourceId)
        {
            for (InputEventGroup& inputEventHandler : m_inputEventGroups)
            {
                inputEventHandler.Activate(sourceId);
            }
        }

        void Deactivate(const AZ::EntityId& sourceId)
        {
            for (InputEventGroup& inputEventHandler : m_inputEventGroups)
            {
                inputEventHandler.Deactivate(sourceId);
            }
        }

        void Swap(InputEventBindings* other)
        {
            m_inputEventGroups.swap(other->m_inputEventGroups);
        }

    private:
        AZStd::vector<InputEventGroup> m_inputEventGroups;
    };

    class InputEventBindingsAsset
        : public AZ::Data::AssetData
    {
    public:
        AZ_CLASS_ALLOCATOR(InputEventBindingsAsset, AZ::SystemAllocator, 0);
        AZ_RTTI(InputEventBindingsAsset, "{25971C7A-26E2-4D08-A146-2EFCC1C36B0C}", AZ::Data::AssetData);

        InputEventBindings m_bindings;

        static void Reflect(AZ::ReflectContext* context)
        {
            AZ::SerializeContext* serialize = azrtti_cast<AZ::SerializeContext*>(context);
            if (serialize)
            {
                serialize->Class<InputEventBindingsAsset>()
                    ->Field("Bindings", &InputEventBindingsAsset::m_bindings)
                ;

                AZ::EditContext* edit = serialize->GetEditContext();
                if (edit)
                {
                    edit->Class<InputEventBindingsAsset>("Input to Event Bindings Asset", "")
                        ->DataElement(0, &InputEventBindingsAsset::m_bindings, "Bindings", "")
                        ->Attribute(AZ::Edit::Attributes::Visibility, AZ_CRC("PropertyVisibility_ShowChildrenOnly", 0xef428f20))
                    ;
                }
            }
        }
    };
} // namespace Input
