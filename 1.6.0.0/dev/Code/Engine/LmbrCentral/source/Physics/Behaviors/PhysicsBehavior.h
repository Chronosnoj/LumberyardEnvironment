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

#include <AzCore/RTTI/RTTI.h>
#include <AzCore/Component/EntityId.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <physinterface.h>

namespace LmbrCentral
{
    /*!
     * Interface for a specific type of physics behavior.
     * Concrete implementations would be: Rigid Body, Static, Character, etc.
     * The PhysicsComponent owns a PhysicsBehavior instance and users must
     * create the desired behavior type on the component.
     * Games can add their own behavior types for the PhysicsComponent to use.
     *
     * PhysicsBehaviors could have been implemented as fellow components on
     * the same entity as a PhysicsComponent. This would have arguably been
     * a cleaner and more modular approach. That approach was not taken
     * so that users would not be required to add multiple components
     * to an entity just to get "physics" working.
     */
    class PhysicsBehavior
    {
    public:
        AZ_RTTI(PhysicsBehavior, "{13E81FC7-EA39-4457-9990-35F5D9CBBC44}");
        virtual ~PhysicsBehavior() = default;

        //! Called during entity initialization.
        //! The entity is not yet fully Activated at this time.
        virtual void InitPhysicsBehavior(AZ::EntityId entityId) = 0;

        static void Reflect(AZ::ReflectContext* context)
        {
            AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
            if (serializeContext)
            {
                serializeContext->Class<PhysicsBehavior>()
                    ->Version(1)
                    ->SerializerForEmptyClass()
                ;

                AZ::EditContext* editContext = serializeContext->GetEditContext();
                if (editContext)
                {
                    editContext->Class<PhysicsBehavior>(
                        "Physics Behavior", "Base class for physics behaviors.")
                    ;
                }
            }
        }
    };

    /*!
     * A physics behavior specific to CryPhysics.
     */
    class CryPhysicsBehavior
        : public PhysicsBehavior
    {
    public:
        AZ_RTTI(CryPhysicsBehavior, "{7BFB46A3-DBEE-431B-AD6F-1205560BEB25}", PhysicsBehavior);
        ~CryPhysicsBehavior() override = default;

        virtual pe_type GetPhysicsType() const = 0;

        //! Allows specific behaviors to handle transform changes.
        virtual bool HandleTransformChange(const AZ::Transform&) { return false; }

        //! Customize the newly-birthed IPhysicalEntity.
        //! Return whether configuration was successful.
        //! If false is returned then physics will be disabled for this entity.
        virtual bool ConfigurePhysicalEntity(IPhysicalEntity& physicalEntity) = 0;

        static void Reflect(AZ::ReflectContext* context)
        {
            AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
            if (serializeContext)
            {
                serializeContext->Class<CryPhysicsBehavior, PhysicsBehavior>()
                    ->Version(1)
                    ->SerializerForEmptyClass()
                ;

                AZ::EditContext* editContext = serializeContext->GetEditContext();
                if (editContext)
                {
                    editContext->Class<CryPhysicsBehavior>(
                        "Cry Physics Behavior", "Base class for CryEngine physics behaviors.")
                    ;
                }
            }
        }
    };
} // namespace LmbrCentral
