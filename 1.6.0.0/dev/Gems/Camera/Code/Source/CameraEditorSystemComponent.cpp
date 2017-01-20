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
#include "CameraEditorSystemComponent.h"
#include "CameraComponent.h"

#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Math/Vector2.h>
#include <AzCore/Serialization/SerializeContext.h>

#include <AzToolsFramework/Entity/EditorEntityContextBus.h>

#include <Cry_Geo.h>
#include <IEditor.h>
#include <ViewManager.h>
#include <GameEngine.h>
#include <Include/IObjectManager.h>
#include <MathConversion.h>
#include <Objects/BaseObject.h>

namespace Camera
{
    void CameraEditorSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<CameraEditorSystemComponent, AZ::Component>()
                ->Version(1)
                ->SerializerForEmptyClass()
            ;

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<CameraEditorSystemComponent>(
                    "LmbrCentral", "Coordinates initialization of systems within LmbrCentral")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::Category, "Game")
                        ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("System"))
                    ;
            }
        }
    }

    void CameraEditorSystemComponent::Activate()
    {
        AzToolsFramework::EditorEvents::Bus::Handler::BusConnect();
    }

    void CameraEditorSystemComponent::Deactivate()
    {
        AzToolsFramework::EditorEvents::Bus::Handler::BusDisconnect();
    }

    void CameraEditorSystemComponent::PopulateEditorGlobalContextMenu(QMenu* menu, const AZ::Vector2&, int flags)
    {
        IEditor* editor;
        EBUS_EVENT_RESULT(editor, AzToolsFramework::EditorRequests::Bus, GetEditor);

        CGameEngine* gameEngine = editor->GetGameEngine();
        if (!gameEngine || !gameEngine->IsLevelLoaded())
        {
            return;
        }

        if (!(flags & AzToolsFramework::EditorEvents::eECMF_HIDE_ENTITY_CREATION))
        {
            QAction* action = menu->addAction(QObject::tr("Create Camera Entity from viewport"));
            QObject::connect(action, &QAction::triggered, [this, editor] {
                // Create new entity
                AZ::Entity* newEntity;
                EBUS_EVENT_RESULT(newEntity, AzToolsFramework::EditorEntityContextRequestBus, CreateEditorEntity, "Camera");

                // Add CameraComponent
                AZ::SerializeContext* serialize;
                EBUS_EVENT_RESULT(serialize, AZ::ComponentApplicationBus, GetSerializeContext);
                const auto cameraTypeInfo = serialize->FindClassData(azrtti_typeid<CameraComponent>());
                AZ_Assert(cameraTypeInfo, "CameraComponent not registered with SerializeContext");
                EBUS_EVENT(AzToolsFramework::ToolsApplicationRequests::Bus, AddComponentToEntity, newEntity->GetId(), *cameraTypeInfo);

                // Set transform to that of the viewport
                EBUS_EVENT_ID(newEntity->GetId(), AZ::TransformBus, SetWorldTM,
                    LYTransformToAZTransform(editor->GetViewManager()->GetSelectedViewport()->GetViewTM())
                );
            });
            menu->addSeparator();
        }
    }
}
