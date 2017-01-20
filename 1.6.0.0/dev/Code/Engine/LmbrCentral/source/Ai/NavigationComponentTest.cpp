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

#include <AzFramework/Components/TransformComponent.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Component/Entity.h>
#include <AzCore/UnitTest/UnitTest.h>
#include <AzCore/std/containers/queue.h>

namespace LmbrCentral
{
    /**
    * Data used by Navigation Component unit tests
    */
    class NavigationComponentUnitTestData
    {
    private:

        /**
        * Sets an entity up to the state where it can be used in the test
        */
        void InitializeEntity(AZ::Entity* entityToBeInitialized);

    public:

        NavigationComponentUnitTestData();

        //! Test entity 1 will be tracked to do the testing
        AZ::Entity* m_entity1;

        //! Test entity 2 will be not be tracked but is used to facilitate testing
        AZ::Entity* m_entity2;

        //! Origin for reference
        const AZ::Vector3 m_origin;

        using ResponseRecord = AZStd::pair<PathfindRequest::NavigationRequestId, PathfindResponse::Status>;

        //! queue to verify the order of operations on all responses obtained
        AZStd::queue<ResponseRecord> m_responseQueue;

        ~NavigationComponentUnitTestData();
    };

    /**
    * Basic navigation test,
    * Case 1 : Move an entity originally on the navmesh to another point on the navmesh
    * Case 2 : Move an entity originally on the navmesh to another point on the navmesh (Do this twice in quick succession)
    * Case 3 : Move an entity originally on the navmesh to another Entity on the navmesh
    * Case 4 : Move an entity originally on the navmesh to another Moving Entity on the navmesh
    */
    class BasicNavigationComponentUnitTest
        : private NavigationComponentNotificationBus::MultiHandler
    {
    public:

        enum SupportedTests
        {
            Invalid,
            EntityToPoint,
            EntityToPointTwice,
            EntityToEntity,
            EntityToMovingEntity,
            StopTest
        };

        //! tester object created whenever a new test is to be executed
        static AZStd::shared_ptr<BasicNavigationComponentUnitTest> m_tester;

        //! Executes the test in question
        void RunTest(SupportedTests);

        BasicNavigationComponentUnitTest();

    private:

        //! Data used in this test
        NavigationComponentUnitTestData m_unitTestData;

        //////////////////////////////////////////////////////////////////////////
        // Navigation component notification bus implementation
        bool OnPathFound(PathfindRequest::NavigationRequestId requestId, AZStd::shared_ptr<const INavPath> currentPath) override;

        void OnTraversalStarted(PathfindRequest::NavigationRequestId requestId) override;

        void OnTraversalComplete(PathfindRequest::NavigationRequestId requestId) override;

        void OnTraversalCancelled(PathfindRequest::NavigationRequestId requestId) override;

        void OnTraversalInProgress(PathfindRequest::NavigationRequestId requestId, float distanceRemaining) override;
        //////////////////////////////////////////////////////////////////////////
    };

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // BasicNavigationComponentUnitTest

    BasicNavigationComponentUnitTest::BasicNavigationComponentUnitTest()
    {
        NavigationComponentNotificationBus::MultiHandler::BusConnect(m_unitTestData.m_entity1->GetId());

        NavigationComponentNotificationBus::MultiHandler::BusConnect(m_unitTestData.m_entity2->GetId());
    }

    void BasicNavigationComponentUnitTest::RunTest(SupportedTests testCase)
    {
        AZ::Vector3 entityPosition = m_unitTestData.m_origin + AZ::Vector3(5, 5, 33);
        EBUS_EVENT_ID(m_unitTestData.m_entity1->GetId(), AZ::TransformBus, SetWorldTM, AZ::Transform::CreateTranslation(entityPosition));

        PathfindRequest request;

        PathfindRequest::NavigationRequestId requestId;
        switch (testCase)
        {
        case SupportedTests::EntityToPoint:
        {
            // Create a new navigation request
            request.SetDestinationLocation(AZ::Vector3(15, 15, 33));
            break;
        }
        case SupportedTests::EntityToPointTwice:
        {
            // Create a new navigation request
            request.SetDestinationLocation(AZ::Vector3(15, 15, 33));

            // Send request to Nav component
            EBUS_EVENT_ID_RESULT(requestId, m_unitTestData.m_entity1->GetId(), NavigationComponentRequestBus, FindPath, request);

            Sleep(1000);

            // Create a new navigation request
            request.SetDestinationLocation(AZ::Vector3(15, 20, 33));
            break;
        }
        case SupportedTests::EntityToEntity:
        {
            entityPosition = m_unitTestData.m_origin + AZ::Vector3(AZ::Vector3(15, 15, 33));
            EBUS_EVENT_ID(m_unitTestData.m_entity2->GetId(), AZ::TransformBus, SetWorldTM, AZ::Transform::CreateTranslation(entityPosition));
            request.SetTargetEntityId(m_unitTestData.m_entity2->GetId());
            break;
        }
        case SupportedTests::EntityToMovingEntity:
        {
            entityPosition = m_unitTestData.m_origin + AZ::Vector3(AZ::Vector3(10, 10, 33));
            EBUS_EVENT_ID(m_unitTestData.m_entity2->GetId(), AZ::TransformBus, SetWorldTM, AZ::Transform::CreateTranslation(entityPosition));
            request.SetTargetEntityId(m_unitTestData.m_entity2->GetId());

            PathfindRequest requestToMoveSecondEntity;
            requestToMoveSecondEntity.SetDestinationLocation(m_unitTestData.m_origin + AZ::Vector3(20, 20, 33));
            EBUS_EVENT_ID(m_unitTestData.m_entity2->GetId(), NavigationComponentRequestBus, FindPath, requestToMoveSecondEntity);
            break;
        }
        }

        // Send request to Nav component
        EBUS_EVENT_ID_RESULT(requestId, m_unitTestData.m_entity1->GetId(), NavigationComponentRequestBus, FindPath, request);

        // Make sure a valid request id was obtained
        AZ_TEST_ASSERT(requestId != 0);

        // Create a response record
        m_unitTestData.m_responseQueue.push(AZStd::make_pair(requestId, PathfindResponse::Status::SearchingForPath));
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // NavigationComponentNotificationBus::Handler

    bool BasicNavigationComponentUnitTest::OnPathFound(PathfindRequest::NavigationRequestId requestId, AZStd::shared_ptr<const INavPath> currentPath)
    {
        // To allow for non tracked entity "m_testEntity2" to use the same Event handlers
        if (m_unitTestData.m_responseQueue.front().first == requestId)
        {
            // Make certain that only one response has been pushed
            AZ_TEST_ASSERT(m_unitTestData.m_responseQueue.size() == 1);

            // Get the last response
            NavigationComponentUnitTestData::ResponseRecord assumedSearchingForPath = m_unitTestData.m_responseQueue.front();
            m_unitTestData.m_responseQueue.pop();


            // Verify the order of operations
            AZ_TEST_ASSERT(assumedSearchingForPath.second == PathfindResponse::Status::SearchingForPath);

            // Regenerate the Queue
            m_unitTestData.m_responseQueue.push(assumedSearchingForPath);
            m_unitTestData.m_responseQueue.push(AZStd::make_pair(requestId, PathfindResponse::Status::PathFound));
        }
        // Return true to indicate assent for path traversal
        return true;
    }

    void BasicNavigationComponentUnitTest::OnTraversalStarted(PathfindRequest::NavigationRequestId requestId)
    {
        // To allow for non tracked entity "m_testEntity2" to use the same Event handlers
        if (m_unitTestData.m_responseQueue.front().first == requestId)
        {
            // Make certain that only two responses have been pushed
            AZ_TEST_ASSERT(m_unitTestData.m_responseQueue.size() == 2);

            // Get the last two responses
            NavigationComponentUnitTestData::ResponseRecord assumedSearchingForPath = m_unitTestData.m_responseQueue.front();
            m_unitTestData.m_responseQueue.pop();

            NavigationComponentUnitTestData::ResponseRecord assumedPathFound = m_unitTestData.m_responseQueue.front();
            m_unitTestData.m_responseQueue.pop();

            // Check the request id's of all responses in queue
            AZ_TEST_ASSERT(assumedSearchingForPath.first == requestId);
            AZ_TEST_ASSERT(assumedPathFound.first == requestId);

            // Verify the order of operations
            AZ_TEST_ASSERT(assumedSearchingForPath.second == PathfindResponse::Status::SearchingForPath);
            AZ_TEST_ASSERT(assumedPathFound.second == PathfindResponse::Status::PathFound);

            // Regenerate the Queue
            m_unitTestData.m_responseQueue.push(assumedSearchingForPath);
            m_unitTestData.m_responseQueue.push(assumedPathFound);
            m_unitTestData.m_responseQueue.push(AZStd::make_pair(requestId, PathfindResponse::Status::TraversalStarted));
        }
    }

    void BasicNavigationComponentUnitTest::OnTraversalInProgress(PathfindRequest::NavigationRequestId requestId, float distanceRemaining)
    {
        // To allow for non tracked entity "m_testEntity2" to use the same Event handlers
        if (m_unitTestData.m_responseQueue.front().first == requestId)
        {
            gEnv->pLog->LogToConsole("Distance left in treversal %f", distanceRemaining);
        }
    }

    void BasicNavigationComponentUnitTest::OnTraversalComplete(PathfindRequest::NavigationRequestId requestId)
    {
        // To allow for non tracked entity "m_testEntity2" to use the same Event handlers
        if (m_unitTestData.m_responseQueue.front().first == requestId)
        {
            // Make certain that only three responses have been pushed
            AZ_TEST_ASSERT(m_unitTestData.m_responseQueue.size() == 3);

            // Get the last three responses
            NavigationComponentUnitTestData::ResponseRecord assumedSearchingForPath = m_unitTestData.m_responseQueue.front();
            m_unitTestData.m_responseQueue.pop();

            NavigationComponentUnitTestData::ResponseRecord assumedPathFound = m_unitTestData.m_responseQueue.front();
            m_unitTestData.m_responseQueue.pop();

            NavigationComponentUnitTestData::ResponseRecord assumedTraversalStarted = m_unitTestData.m_responseQueue.front();
            m_unitTestData.m_responseQueue.pop();

            // Check the request id's of all responses in queue
            AZ_TEST_ASSERT(assumedSearchingForPath.first == requestId);
            AZ_TEST_ASSERT(assumedPathFound.first == requestId);
            AZ_TEST_ASSERT(assumedTraversalStarted.first == requestId);

            // Verify the order of operations
            AZ_TEST_ASSERT(assumedSearchingForPath.second == PathfindResponse::Status::SearchingForPath);
            AZ_TEST_ASSERT(assumedPathFound.second == PathfindResponse::Status::PathFound);
            AZ_TEST_ASSERT(assumedTraversalStarted.second == PathfindResponse::Status::TraversalStarted);

            // Regenerate the Queue
            m_unitTestData.m_responseQueue.push(assumedSearchingForPath);
            m_unitTestData.m_responseQueue.push(assumedPathFound);
            m_unitTestData.m_responseQueue.push(assumedTraversalStarted);
            m_unitTestData.m_responseQueue.push(AZStd::make_pair(requestId, PathfindResponse::Status::TraversalComplete));

            gEnv->pLog->LogToConsole("Test complete");
        }
    }

    void BasicNavigationComponentUnitTest::OnTraversalCancelled(PathfindRequest::NavigationRequestId /*requestId*/)
    {
        gEnv->pLog->LogToConsole("Traversal Cancelled");
    }

    //////////////////////////////////////////////////////////////////////////

    /**
    * Sets an entity up to the state where it can be used in the test
    */
    void NavigationComponentUnitTestData::InitializeEntity(AZ::Entity* entityToBeInitialized)
    {
        // Set the entity id
        AZ::EntityId newId = AZ::Entity::MakeId();
        entityToBeInitialized->SetId(newId);
        AZ_TEST_ASSERT(entityToBeInitialized->GetId() == newId);

        // Create a transform component
        AzFramework::TransformComponent* transformComponent = new AzFramework::TransformComponent();

        AZ_TEST_ASSERT(transformComponent != nullptr);
        AZ_TEST_ASSERT(transformComponent->GetEntity() == nullptr);

        bool result = entityToBeInitialized->AddComponent(transformComponent);
        AZ_TEST_ASSERT(result);
        AZ_TEST_ASSERT(transformComponent->GetEntity() != nullptr);

        // Create a navigation component
        NavigationComponent* navComponent = new NavigationComponent();
        AZ_TEST_ASSERT(navComponent != nullptr);
        AZ_TEST_ASSERT(navComponent->GetEntity() == nullptr);

        result = entityToBeInitialized->AddComponent(navComponent);
        AZ_TEST_ASSERT(result);
        AZ_TEST_ASSERT(navComponent->GetEntity() != nullptr);

        // init entity
        entityToBeInitialized->Init();
        AZ_TEST_ASSERT(navComponent->GetEntity() == entityToBeInitialized);
        AZ_TEST_ASSERT(entityToBeInitialized->GetState() == AZ::Entity::ES_INIT);

        // activate entity
        entityToBeInitialized->Activate();
        AZ_TEST_ASSERT(entityToBeInitialized->GetState() == AZ::Entity::ES_ACTIVE);
    }

    NavigationComponentUnitTestData::NavigationComponentUnitTestData()
        : m_origin(AZ::Vector3::CreateZero())
    {
        // Create a test entity
        m_entity1 = aznew AZ::Entity("Navigation Test Entity 1");
        AZ_TEST_ASSERT(m_entity1->GetState() == AZ::Entity::ES_CONSTRUCTED);

        InitializeEntity(m_entity1);

        // Create another test entity
        m_entity2 = aznew AZ::Entity("Navigation Test Entity 2");
        AZ_TEST_ASSERT(m_entity2->GetState() == AZ::Entity::ES_CONSTRUCTED);

        InitializeEntity(m_entity2);
    }

    NavigationComponentUnitTestData::~NavigationComponentUnitTestData()
    {
        m_entity1->Deactivate();
        m_entity2->Deactivate();

        delete m_entity1;
        delete m_entity2;
    }
} // namespace LmbrCentral
