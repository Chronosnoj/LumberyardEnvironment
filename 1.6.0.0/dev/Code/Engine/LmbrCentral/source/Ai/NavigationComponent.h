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

#include <LmbrCentral/Ai/NavigationComponentBus.h>

// Cry pathfinding system includes
#include <INavigationSystem.h>

// Component factory
#include <AzCore/RTTI/RTTI.h>

// Component utilization
#include <AzCore/Component/Entity.h>
#include <AzCore/Component/Component.h>
#include <AzCore/Component/ComponentBus.h>

// Other buses used
#include <AzCore/Component/TickBus.h>

// Data and containers
#include <AzCore/Math/Crc.h>

namespace LmbrCentral
{
    class NavigationComponent;

    /**
    * Represents the response to any pathfinding request.
    * Stores the original request and the current state
    * along with relevant pathfinding data
    */
    class PathfindResponse
        : private AZ::TransformNotificationBus::Handler
    {
    public:

        enum class Status
        {
            Uninitialized,
            Initialized,
            SearchingForPath,
            PathFound,
            TraversalStarted,
            TraversalInProgress,
            TraversalComplete,
            TraversalCancelled
        };

        using PathfinderRequestId = AZ::u32;

        PathfindResponse();

        void SetOwningComponent(NavigationComponent* navComponent)
        {
            m_navigationComponent = navComponent;
        }

        const PathfindRequest& GetRequest() const
        {
            return m_request;
        }

        PathfindRequest::NavigationRequestId GetRequestId() const
        {
            return m_requestId;
        }

        PathfinderRequestId GetPathfinderRequestId() const
        {
            return m_pathfinderRequestId;
        }

        void SetPathfinderRequestId(PathfinderRequestId pathfinderRequestId)
        {
            m_pathfinderRequestId = pathfinderRequestId;
        }

        const AZ::Vector3& GetCurrentDestination() const
        {
            return m_currentDestination;
        }

        Status GetStatus() const
        {
            return m_responseStatus;
        }

        void SetStatus(Status status)
        {
            m_responseStatus = status;

            // If the traversal was cancelled or completed and the request was following an entity
            if ((status >= Status::TraversalComplete) && m_request.HasTargetEntity())
            {
                // Disconnect from any notifications on the transform bust
                AZ::TransformNotificationBus::Handler::BusDisconnect();
            }
        }

        void SetCurrentPath(AZStd::shared_ptr<INavPath> currentPath)
        {
            m_currentPath = currentPath;
        }

        AZStd::shared_ptr<INavPath> GetCurrentPath() const
        {
            return m_currentPath;
        }

        /**
        * Sets up a response for a newly received request
        */
        void SetupForNewRequest(NavigationComponent* ownerComponent, const PathfindRequest& request);

        //! Invalid request id
        static const PathfindRequest::NavigationRequestId kInvalidRequestId = 0;

        //////////////////////////////////////////////////////////////////////////////////
        // Transform notification bus handler

        /// Called when the local transform of the entity has changed.
        void OnTransformChanged(const AZ::Transform& /*local*/, const AZ::Transform& /*world*/) override;

        //////////////////////////////////////////////////////////////////////////////////

    private:

        //! The request that created this response
        PathfindRequest m_request;

        //////////////////////////////////////////////////////////////////////
        // Response members

        /** Represents the destination that the entity is currently trying to reach.
        * This may be different than the original destination.
        * This change generally happens when the Component Entity is asked to pathfind to
        * another entity that may be moving
        */
        AZ::Vector3 m_currentDestination;

        /** The identifier for this request
        * Does not change for any given request, is used by the requester and other components
        * to identify this pathfinding query uniquely
        */
        PathfindRequest::NavigationRequestId m_requestId;

        /** The identifier used by the pathfinder for queries pertaining to this request
        * May change during the lifetime of any particular request, generally in response to situations that
        * necessitate an update in the path. Following an entity is a prime example, the entity being followed
        * may move and new pathfinding queries may be generated and pushed to the pathfinding system to make sure
        * this entity pathfinds properly. In such a scenario, this id changes.
        */
        PathfinderRequestId m_pathfinderRequestId;

        //! Stores the status of this request
        Status m_responseStatus;

        //! Stores the path obtained as a result of this request
        AZStd::shared_ptr<INavPath> m_currentPath;

        //! Points back to the navigation component that is handling this request / response
        NavigationComponent* m_navigationComponent;

        //////////////////////////////////////////////////////////////////////

        //! the current request id
        static PathfindRequest::NavigationRequestId s_nextRequestId;
    };

    /*!
    * The Navigation component provides basic pathfinding and path following services to an Entity.
    * It serves AI or other Game Logic by accepting navigation commands and dispatching per-frame
    * movement requests to the Physics component in order to follow the calculated path.
    */
    class NavigationComponent
        : public AZ::Component
        , public NavigationComponentRequestBus::Handler
        , public IAIPathAgent
        , public AZ::TickBus::Handler
        , public AZ::TransformNotificationBus::Handler
    {
    public:

        friend void PathfindResponse::OnTransformChanged(const AZ::Transform& /*local*/, const AZ::Transform& /*world*/);

        friend class NavigationComponentFactory;

        AZ_COMPONENT(NavigationComponent, "{92284847-9BB3-4CF0-9017-F7E5CEDF3B7B}")

        NavigationComponent();

        //////////////////////////////////////////////////////////////////////////
        // AZ::Component interface implementation
        void Init() override;
        void Activate() override;
        void Deactivate() override;

        //////////////////////////////////////////////////////////////////////////
        // NavigationComponentRequests::Bus::Handler interface implementation
        PathfindRequest::NavigationRequestId FindPath(const PathfindRequest& request) override;
        PathfindRequest::NavigationRequestId FindPathToEntity(AZ::EntityId targetEntityId) override;
        void Stop(PathfindRequest::NavigationRequestId requestId) override;

        ///////////////////////////////////

        //////////////////////////////////////////////////////////////////////////
        // TickBus
        virtual void OnTick(float deltaTime, AZ::ScriptTimePoint time);
        //////////////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////////////////////
        // Transform notification bus listener

        /// Called when the local transform of the entity has changed. Local transform update always implies world transform change too.
        void OnTransformChanged(const AZ::Transform& /*local*/, const AZ::Transform& /*world*/) override;

        //////////////////////////////////////////////////////////////////////////////////

    private:

        // Nav Component settings

        /**
        * Describes the "type" of the Entity for navigation purposes.
        * This type is used to select which navmesh this entity will follow in a scenario where multiple navmeshes are available
        */
        AZStd::string m_agentType;

        //! Describes the radius of this entity for navigation purposes
        float m_agentRadius;

        //! Describes the delta between the points that an entity walks over while following a given path
        float m_lookAheadDistance;

        //! Describes the distance from the end point that an entity needs to be before its movement is to be stopped and considered complete
        float m_arrivalDistanceThreshold;

        //! Describes the distance from its previously known location that a target entity needs to move before a new path is calculated
        float m_repathThreshold;

        // Runtime data

        //! Stores the transform of the entity this component is attached to
        AZ::Transform m_entityTransform;

        //! Cache the last response (and request) received by the Navigation Component
        PathfindResponse m_lastResponseCache;

        //! The Navigation Agent Type identifier used by the Navigation system
        NavigationAgentTypeID   m_agentTypeId;

        /**
        * Uses the data in "m_lastResponseCache" to request a path from the pathfinder
        */
        PathfindResponse::PathfinderRequestId RequestPath();

        /**
        * Resets the Navigation Component and prepares it to process a new pathfinding request
        * Also cancels any pathfinding operations in progress
        */
        void Reset();

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
        {
            provided.push_back(AZ_CRC("NavigationService"));
        }

        //////////////////////////////////////////////////////////////////////////
        // This component will require the services of the transform component in
        // the short term and the physics component in the long term
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
        {
            required.push_back(AZ_CRC("TransformService"));
        }

        static void Reflect(AZ::ReflectContext* context);

    protected:

        void OnPathResult(const MNM::QueuedPathID& requestId, MNMPathRequestResult& result);

        //// IAIPathAgent
        IEntity* GetPathAgentEntity() const override { return nullptr; }
        const char* GetPathAgentName() const override { return m_entity->GetName().c_str(); }
        void GetPathAgentNavigationBlockers(NavigationBlockers&, const ::PathfindRequest*) override {}
        AZ::u16 GetPathAgentType() const override { return AIOBJECT_ACTOR; }
        Vec3 GetPathAgentPos() const override { return Vec3(); }
        float GetPathAgentPassRadius() const override { return 0.f; }
        Vec3 GetPathAgentVelocity() const override { return ZERO; }
        AZ::u32 GetPathAgentLastNavNode() const override { return 0; }
        void SetPathAgentLastNavNode(unsigned int) override {}
        void SetPathToFollow(const char*) override {}
        void SetPathAttributeToFollow(bool) override {}
        void SetPFBlockerRadius(int, float) override {}
        ETriState CanTargetPointBeReached(CTargetPointRequest&) override { return eTS_maybe; }
        bool UseTargetPointRequest(const CTargetPointRequest&) override { return false; }
        bool GetValidPositionNearby(const Vec3&, Vec3&) const override { return false; }
        bool GetTeleportPosition(Vec3&) const override { return false; }
        class IPathFollower* GetPathFollower() const override { return nullptr; }
        bool IsPointValidForAgent(const Vec3&, AZ::u32) const { return true; };
        const AgentMovementAbility& GetPathAgentMovementAbility() const
        {
            static AgentMovementAbility sTemp;
            return sTemp;
        }
        //// ~IAIPathAgent
    };
} // namespace LmbrCentral
