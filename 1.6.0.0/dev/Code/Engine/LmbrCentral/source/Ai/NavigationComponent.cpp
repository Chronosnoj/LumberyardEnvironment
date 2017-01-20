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
#include "NavigationComponent.h"

#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <MathConversion.h>
#include <AzCore/UnitTest/UnitTest.h>
#include <IPathfinder.h>
#include <AzCore/EBus/ScriptBinder.h>

namespace LmbrCentral
{
    AZ_SCRIPTABLE_EBUS(
        NavigationComponentRequestBus,
        NavigationComponentRequestBus,
        "{39957A75-F420-4B9C-B52A-B90B15ED6A3F}",
        "{03729E81-9535-466B-A4D5-280E5B106B3E}",
        AZ_SCRIPTABLE_EBUS_EVENT_RESULT(PathfindRequest::NavigationRequestId, PathfindResponse::kInvalidRequestId, FindPathToEntity, AZ::EntityId)
        AZ_SCRIPTABLE_EBUS_EVENT(Stop, PathfindRequest::NavigationRequestId)
        )

    AZ_SCRIPTABLE_EBUS(
        NavigationComponentNotificationBus,
        NavigationComponentNotificationBus,
        "{B14A4634-DED7-45B0-887B-7EF6684A9F03}",
        "{6D060202-06BA-470E-8F6B-E1982360C752}",
        AZ_SCRIPTABLE_EBUS_EVENT(OnSearchingForPath, PathfindRequest::NavigationRequestId)
        AZ_SCRIPTABLE_EBUS_EVENT(OnTraversalStarted, PathfindRequest::NavigationRequestId)
        AZ_SCRIPTABLE_EBUS_EVENT(OnTraversalInProgress, PathfindRequest::NavigationRequestId, float)
        AZ_SCRIPTABLE_EBUS_EVENT(OnTraversalComplete, PathfindRequest::NavigationRequestId)
        AZ_SCRIPTABLE_EBUS_EVENT(OnTraversalCancelled, PathfindRequest::NavigationRequestId)
        )

    PathfindRequest::NavigationRequestId PathfindResponse::s_nextRequestId = kInvalidRequestId;

    //////////////////////////////////////////////////////////////////////////

    void NavigationComponent::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);

        if (serializeContext)
        {
            serializeContext->Class<NavigationComponent, AZ::Component>()
                ->Version(1)
                ->Field("Agent Type", &NavigationComponent::m_agentType)
                ->Field("Agent Radius", &NavigationComponent::m_agentRadius)
                ->Field("Look Ahead Distance", &NavigationComponent::m_lookAheadDistance)
                ->Field("Arrival Distance Threshold", &NavigationComponent::m_arrivalDistanceThreshold)
                ->Field("Repath Threshold", &NavigationComponent::m_repathThreshold);

            AZ::EditContext* editContext = serializeContext->GetEditContext();

            if (editContext)
            {
                editContext->Class<NavigationComponent>(
                    "Navigation", "Allows an Entity to do basic Navigation and Pathfinding")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::Category, "AI")
                        ->Attribute(AZ::Edit::Attributes::Icon, "Editor/Icons/Components/Navigation.png")
                        ->Attribute(AZ::Edit::Attributes::ViewportIcon, "Editor/Icons/Components/Viewport/Navigation.png")
                        ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("Game"))
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(0, &NavigationComponent::m_agentType, "Agent Type",
                    "Describes the type of the Entity for navigation purposes. ")
                    ->DataElement(0, &NavigationComponent::m_agentRadius, "Agent Radius",
                    "Radius of this Navigation Agent")
                    ->DataElement(0, &NavigationComponent::m_lookAheadDistance, "Look Ahead Distance",
                    "Describes the delta between the points that an entity walks over while following a given path")
                    ->DataElement(0, &NavigationComponent::m_arrivalDistanceThreshold,
                    "Arrival Distance Threshold", "Describes the distance from the end point that an entity needs to be before its movement is to be stopped and considered complete")
                    ->DataElement(0, &NavigationComponent::m_repathThreshold,
                    "Repath Threshold", "Describes the distance its previously known location that a target entity needs to move before a new path is calculated");
            }
        }

        AZ::ScriptContext* script = azrtti_cast<AZ::ScriptContext*>(context);
        if (script)
        {
            ScriptableEBus_NavigationComponentRequestBus::Reflect(script);
            ScriptableEBus_NavigationComponentNotificationBus::Reflect(script);
        }
    }

    //////////////////////////////////////////////////////////////////////////

    //////////////////////////////////////////////////////////////////////////
    // Pathfind response
    //! @param navComponent The navigation component being serviced by this response
    PathfindResponse::PathfindResponse()
        : m_requestId(kInvalidRequestId)
        , m_pathfinderRequestId(kInvalidRequestId)
        , m_responseStatus(PathfindResponse::Status::Uninitialized)
        , m_navigationComponent(nullptr)
    {
    }

    void PathfindResponse::SetupForNewRequest(NavigationComponent* ownerComponent, const PathfindRequest& request)
    {
        m_navigationComponent = ownerComponent;
        m_request = request;
        m_requestId = ++s_nextRequestId;
        m_currentDestination = request.GetDestinationLocation();

        // Reset State information
        m_pathfinderRequestId = kInvalidRequestId;
        m_currentPath.reset();

        // Disconnect from any notifications from earlier requests
        AZ::TransformNotificationBus::Handler::BusDisconnect();

        // If this request is to follow a moving entity then connect to the transform notification bus for the target
        if (m_request.HasTargetEntity())
        {
            AZ::TransformNotificationBus::Handler::BusConnect(m_request.GetTargetEntityId());
        }

        m_responseStatus = Status::Initialized;
    }


    void PathfindResponse::OnTransformChanged(const AZ::Transform& /*local*/, const AZ::Transform& world)
    {
        if (!m_navigationComponent)
        {
            return;
        }

        auto delta = (world.GetPosition() - GetCurrentDestination()).GetLength();

        if (delta > m_navigationComponent->m_repathThreshold)
        {
            m_currentDestination = world.GetPosition();
            SetPathfinderRequestId(m_navigationComponent->RequestPath());
        }
    }

    //////////////////////////////////////////////////////////////////////////

    NavigationComponent::NavigationComponent()
        : m_agentType("MediumSizedCharacters")
        , m_agentRadius(4.f)
        , m_lookAheadDistance(0.1f)
        , m_arrivalDistanceThreshold(0.25f)
        , m_repathThreshold(1.f)
    {
    }

    //////////////////////////////////////////////////////////////////////////
    // AZ::Component interface implementation
    void NavigationComponent::Init()
    {
        m_lastResponseCache.SetOwningComponent(this);
        m_agentTypeId = gEnv->pAISystem->GetNavigationSystem()->GetAgentTypeID(m_agentType.c_str());
    }

    void NavigationComponent::Activate()
    {
        NavigationComponentRequestBus::Handler::BusConnect(m_entity->GetId());
        AZ::TransformNotificationBus::Handler::BusConnect(m_entity->GetId());

        EBUS_EVENT_ID_RESULT(m_entityTransform, m_entity->GetId(), AZ::TransformBus, GetWorldTM);
    }

    void NavigationComponent::Deactivate()
    {
        NavigationComponentRequestBus::Handler::BusDisconnect();
        AZ::TransformNotificationBus::Handler::BusDisconnect();
        Reset();
    }
    //////////////////////////////////////////////////////////////////////////

    //////////////////////////////////////////////////////////////////////////
    // NavigationComponentRequestBus::Handler interface implementation

    PathfindRequest::NavigationRequestId NavigationComponent::FindPath(const PathfindRequest& request)
    {
        // If neither the position nor the destination has been set
        if (!(request.HasTargetEntity() || request.HasTargetLocation()))
        {
            // Return an invalid id to indicate that the request is bad
            return PathfindResponse::kInvalidRequestId;
        }

        // Reset the Navigation component to deal with a new pathfind request
        Reset();

        m_lastResponseCache.SetupForNewRequest(this, request);

        // Request for a path
        PathfindResponse::PathfinderRequestId pathfinderRequestID = RequestPath();

        if (pathfinderRequestID != MNM::Constants::eQueuedPathID_InvalidID)
        {
            m_lastResponseCache.SetPathfinderRequestId(pathfinderRequestID);
        }
        else
        {
            m_lastResponseCache.SetPathfinderRequestId(PathfindResponse::kInvalidRequestId);

            m_lastResponseCache.SetStatus(PathfindResponse::Status::TraversalCancelled);

            // Inform every listener on this entity about the "Traversal cancelled" event
            EBUS_EVENT_ID(m_entity->GetId(), NavigationComponentNotificationBus,
                OnTraversalCancelled, m_lastResponseCache.GetRequestId());

            return PathfindResponse::kInvalidRequestId;
        }

        // Indicate that the path is being looked for
        m_lastResponseCache.SetStatus(PathfindResponse::Status::SearchingForPath);

        // Inform every listener on this entity about the "Searching For Path" event
        EBUS_EVENT_ID(m_entity->GetId(), NavigationComponentNotificationBus,
            OnSearchingForPath, m_lastResponseCache.GetRequestId());

        return m_lastResponseCache.GetRequestId();
    }

    PathfindResponse::PathfinderRequestId NavigationComponent::RequestPath()
    {
        // Create a new pathfind request
        MNMPathRequest pathfinderRequest;

        // 1. Set the current entity's position as the start location
        pathfinderRequest.startLocation = AZVec3ToLYVec3(m_entityTransform.GetPosition());

        // 2. Set the requested destination
        pathfinderRequest.endLocation = AZVec3ToLYVec3(m_lastResponseCache.GetCurrentDestination());

        // 3. Set the type of the Navigation agent
        pathfinderRequest.agentTypeID = m_agentTypeId;

        // 4. Set the callback
        pathfinderRequest.resultCallback = functor(*this, &NavigationComponent::OnPathResult);

        // 5. Request the path.
        IMNMPathfinder* pathFinder = gEnv->pAISystem->GetMNMPathfinder();
        return pathFinder->RequestPathTo(this, pathfinderRequest);
    }

    void NavigationComponent::OnPathResult(const MNM::QueuedPathID& pathfinderRequestId, MNMPathRequestResult& result)
    {
        // If the pathfinding result is for the latest pathfinding request (Otherwise ignore)
        if (pathfinderRequestId == m_lastResponseCache.GetPathfinderRequestId())
        {
            if (result.HasPathBeenFound() &&
                (m_lastResponseCache.GetRequestId() != PathfindResponse::kInvalidRequestId))
            {
                INavPath* calculatedPath = result.pPath->Clone();
                m_lastResponseCache.SetCurrentPath(AZStd::shared_ptr<INavPath>(calculatedPath));

                // If this request was in fact looking for a path (and this isn't just a path update request)
                if (m_lastResponseCache.GetStatus() == PathfindResponse::Status::SearchingForPath)
                {
                    // Set the status of this request
                    m_lastResponseCache.SetStatus(PathfindResponse::Status::PathFound);

                    // Inform every listener on this entity that a path has been found
                    bool shouldPathBeTraversed = true;

                    EBUS_EVENT_ID_RESULT(shouldPathBeTraversed, m_entity->GetId(),
                        NavigationComponentNotificationBus,
                        OnPathFound, m_lastResponseCache.GetRequestId(), m_lastResponseCache.GetCurrentPath());

                    if (shouldPathBeTraversed)
                    {
                        // Connect to tick bus
                        AZ::TickBus::Handler::BusConnect();

                        //  Set the status of this request
                        m_lastResponseCache.SetStatus(PathfindResponse::Status::TraversalStarted);

                        // Inform every listener on this entity that traversal is in progress
                        EBUS_EVENT_ID(m_entity->GetId(), NavigationComponentNotificationBus,
                            OnTraversalStarted, m_lastResponseCache.GetRequestId());
                    }
                    else
                    {
                        // Set the status of this request
                        m_lastResponseCache.SetStatus(PathfindResponse::Status::TraversalCancelled);

                        // Inform every listener on this entity that a path could not be found
                        EBUS_EVENT_ID(m_entity->GetId(), NavigationComponentNotificationBus,
                            OnTraversalCancelled, m_lastResponseCache.GetRequestId());
                    }
                }
            }
            else
            {
                // Set the status of this request
                m_lastResponseCache.SetStatus(PathfindResponse::Status::TraversalCancelled);

                // Inform every listener on this entity that a path could not be found
                EBUS_EVENT_ID(m_entity->GetId(), NavigationComponentNotificationBus,
                    OnTraversalCancelled, m_lastResponseCache.GetRequestId());
            }
        }
    }

    void NavigationComponent::Stop(PathfindRequest::NavigationRequestId requestId)
    {
        if ((m_lastResponseCache.GetRequestId() == requestId)
            && requestId != PathfindResponse::kInvalidRequestId)
        {
            Reset();
        }
    }

    //////////////////////////////////////////////////////////////////////////
    // AZ::TickBus::Handler implementation

    void NavigationComponent::OnTick(float deltaTime, AZ::ScriptTimePoint time)
    {
        // If there isn't a valid path
        if (!m_lastResponseCache.GetCurrentPath())
        {
            // Come back next frame
            return;
        }

        // Function parameters
        bool isResolvingSticking;
        Vec3 direction, outPathDirection, outPathAheadDirection, outPathAheadPosition;
        float outDistanceToEnd, outDistanceToPath;
        const Vec3 velocity = ZERO;

        const bool pathUpdated = m_lastResponseCache.GetCurrentPath()->UpdateAndSteerAlongPath(
                direction,
                outDistanceToEnd,
                outDistanceToPath,
                isResolvingSticking,
                outPathDirection,
                outPathAheadDirection,
                outPathAheadPosition,
                AZVec3ToLYVec3(m_entityTransform.GetPosition()),
                velocity,
                m_lookAheadDistance,
                m_agentRadius,
                deltaTime,
                false, false);

        if (pathUpdated)
        {
            // Set the position of the entity
            // Currently this is being done by setting the transform directly, this will not be so
            // after the Physics component comes on line
            // LY-TODO : Replace with Physics component
            AZ::Transform entityTransform = AZ::Transform::CreateTranslation(LYVec3ToAZVec3(outPathAheadPosition));
            EBUS_EVENT_ID(m_entity->GetId(), AZ::TransformBus, SetWorldTM, entityTransform);

            m_lastResponseCache.SetStatus(PathfindResponse::Status::TraversalInProgress);

            // Inform every listener on this entity that the path has been finished
            EBUS_EVENT_ID(m_entity->GetId(), NavigationComponentNotificationBus,
                OnTraversalInProgress, m_lastResponseCache.GetRequestId(), outDistanceToEnd);
        }

        if (outDistanceToEnd <=
            (m_lastResponseCache.GetRequest().HasOverrideArrivalDistance() ? m_lastResponseCache.GetRequest().GetArrivalDistanceThreshold() : m_arrivalDistanceThreshold))
        {
            // Set the status of this request
            m_lastResponseCache.SetStatus(PathfindResponse::Status::TraversalComplete);

            // Inform every listener on this entity that the path has been finished
            EBUS_EVENT_ID(m_entity->GetId(), NavigationComponentNotificationBus,
                OnTraversalComplete, m_lastResponseCache.GetRequestId());

            // Reset the pathfinding component
            Reset();
        }
    }

    //////////////////////////////////////////////////////////////////////////
    // AZ::TransformNotificationBus::Handler implementation
    // If the transform on the entity has changed
    void NavigationComponent::OnTransformChanged(const AZ::Transform& /*local*/, const AZ::Transform& world)
    {
        m_entityTransform = world;
    }

    //////////////////////////////////////////////////////////////////////////
    // NavigationComponent implementation

    void NavigationComponent::Reset()
    {
        PathfindResponse::Status lastResponseStatus = m_lastResponseCache.GetStatus();

        // If there is already a Request being serviced
        if (lastResponseStatus > PathfindResponse::Status::Initialized
            && lastResponseStatus < PathfindResponse::Status::TraversalComplete)
        {
            // If the pathfinding request was still being serviced by the pathfinder
            if (lastResponseStatus >= PathfindResponse::Status::SearchingForPath
                && lastResponseStatus <= PathfindResponse::Status::TraversalInProgress)
            {
                // and If the request was a valid one
                if (m_lastResponseCache.GetRequestId() != PathfindResponse::kInvalidRequestId)
                {
                    // Cancel that request with the pathfinder
                    IMNMPathfinder* pathFinder = gEnv->pAISystem->GetMNMPathfinder();
                    pathFinder->CancelPathRequest(m_lastResponseCache.GetPathfinderRequestId());
                }
            }

            // Indicate that traversal on this request was cancelled
            m_lastResponseCache.SetStatus(PathfindResponse::Status::TraversalCancelled);

            // Inform every listener on this entity that traversal was cancelled
            EBUS_EVENT_ID(m_entity->GetId(), NavigationComponentNotificationBus,
                OnTraversalCancelled, m_lastResponseCache.GetRequestId());
        }

        // Disconnect from tick bus
        AZ::TickBus::Handler::BusDisconnect();
    }

    PathfindRequest::NavigationRequestId NavigationComponent::FindPathToEntity(AZ::EntityId targetEntityId)
    {
        PathfindRequest request;
        request.SetTargetEntityId(targetEntityId);
        return FindPath(request);
    }
} // namespace LmbrCentral
