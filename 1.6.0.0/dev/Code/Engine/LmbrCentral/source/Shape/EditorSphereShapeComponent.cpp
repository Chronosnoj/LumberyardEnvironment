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
#include "SphereShapeComponent.h"
#include "EditorSphereShapeComponent.h"

#include <AzCore/RTTI/ReflectContext.h>

namespace LmbrCentral
{
    namespace ClassConverters
    {
        static bool DeprecateEditorSphereColliderComponent(AZ::SerializeContext& context, AZ::SerializeContext::DataElementNode& classElement);
    }

    void EditorSphereShapeComponent::Reflect(AZ::ReflectContext* context)
    {
        auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (serializeContext)
        {
            // Deprecate: EditorSphereColliderComponent -> EditorSphereShapeComponent
            serializeContext->ClassDeprecate(
                "EditorSphereColliderComponent",
                "{9A12FC39-60D2-4237-AC79-11FEDFEDB851}",
                &ClassConverters::DeprecateEditorSphereColliderComponent
                );

            serializeContext->Class<EditorSphereShapeComponent, EditorComponentBase>()
                ->Version(1)
                ->Field("Configuration", &EditorSphereShapeComponent::m_configuration)
                ;

            AZ::EditContext* editContext = serializeContext->GetEditContext();
            if (editContext)
            {
                editContext->Class<EditorSphereShapeComponent>(
                    "Sphere Shape", "Provides spherical shape")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::Category, "Shape")
                        ->Attribute(AZ::Edit::Attributes::Icon, "Editor/Icons/Components/Sphere_Shape.png")
                        ->Attribute(AZ::Edit::Attributes::ViewportIcon, "Editor/Icons/Components/Viewport/Sphere_Shape.png")
                        ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("Game"))
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(0, &EditorSphereShapeComponent::m_configuration, "Configuration", "Sphere Shape Configuration")
                        ->Attribute(AZ::Edit::Attributes::Visibility, AZ_CRC("PropertyVisibility_ShowChildrenOnly"))
                        ;
            }
        }
    }

    void EditorSphereShapeComponent::DrawShape(AzFramework::EntityDebugDisplayRequests* displayContext) const
    {
        displayContext->SetColor(EditorBaseShapeComponent::s_shapeColor);
        displayContext->DrawBall(AZ::Vector3::CreateZero(), m_configuration.GetRadius());
        displayContext->SetColor(EditorBaseShapeComponent::s_shapeWireColor);
        displayContext->DrawWireSphere(AZ::Vector3::CreateZero(), m_configuration.GetRadius());
    }

    void EditorSphereShapeComponent::BuildGameEntity(AZ::Entity* gameEntity)
    {
        auto component = gameEntity->CreateComponent<SphereShapeComponent>();
        if (component)
        {
            component->m_configuration = m_configuration;
        }
    }


    namespace ClassConverters
    {
        static bool DeprecateEditorSphereColliderComponent(AZ::SerializeContext& context, AZ::SerializeContext::DataElementNode& classElement)
        {
            /*
            Old:
            <Class name="EditorSphereColliderComponent" field="element" version="1" type="{9A12FC39-60D2-4237-AC79-11FEDFEDB851}">
             <Class name="SphereColliderConfiguration" field="Configuration" version="1" type="{0319AE62-3355-4C98-873D-3139D0427A53}">
              <Class name="float" field="Radius" value="1.0000000" type="{EA2C3E90-AFBE-44D4-A90D-FAAF79BAF93D}"/>
             </Class>
            </Class>

            New:
            <Class name="EditorSphereShapeComponent" field="element" version="1" type="{2EA56CBF-63C8-41D9-84D5-0EC2BECE748E}">
             <Class name="SphereShapeConfiguration" field="Configuration" version="1" type="{4AADFD75-48A7-4F31-8F30-FE4505F09E35}">
              <Class name="float" field="Radius" value="1.0000000" type="{EA2C3E90-AFBE-44D4-A90D-FAAF79BAF93D}"/>
             </Class>
            </Class>
            */

            // Cache the Configuration
            SphereShapeConfiguration configuration;
            int configIndex = classElement.FindElement(AZ_CRC("Configuration"));
            if (configIndex != -1)
            {
                classElement.GetSubElement(configIndex).GetData<SphereShapeConfiguration>(configuration);
            }

            // Convert to EditorSphereShapeComponent
            bool result = classElement.Convert(context, "{2EA56CBF-63C8-41D9-84D5-0EC2BECE748E}");
            if (result)
            {
                configIndex = classElement.AddElement<SphereShapeConfiguration>(context, "Configuration");
                if (configIndex != -1)
                {
                    classElement.GetSubElement(configIndex).SetData<SphereShapeConfiguration>(context, configuration);
                }
                return true;
            }
            return false;
        }

    } // namespace ClassConverters

} // namespace LmbrCentral
