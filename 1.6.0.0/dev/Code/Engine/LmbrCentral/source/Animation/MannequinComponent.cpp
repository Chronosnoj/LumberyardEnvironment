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

#include <AzCore/EBus/EBus.h>
#include <AzCore/Component/Entity.h>
#include <AzCore/EBus/ScriptBinder.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Math/Transform.h>

#include <AzFramework/StringFunc/StringFunc.h>
#include <LmbrCentral/Rendering/MeshComponentBus.h>

#include <ISystem.h>
#include <ICryMannequin.h>
#include <IGameFramework.h>

#include "MannequinComponent.h"

namespace LmbrCentral
{

    AZ_SCRIPTABLE_EBUS(
        MannequinRequestsBus,
        MannequinRequestsBus,
        "{D5C75D9A-3A87-4BE5-BFAE-C6654C023942}",
        "{132AA48E-73B0-4DFB-A271-C4817B01E74E}",
        AZ_SCRIPTABLE_EBUS_EVENT_RESULT(FragmentRequestId, MannequinRequests::s_invalidRequestId, QueueFragment, int, const char*, const char*, bool)
        AZ_SCRIPTABLE_EBUS_EVENT(PauseAll)
        AZ_SCRIPTABLE_EBUS_EVENT(ResumeAll, IActionController::EResumeFlags)
        AZ_SCRIPTABLE_EBUS_EVENT(SetTag,const char*)
        AZ_SCRIPTABLE_EBUS_EVENT(ClearTag,const char*)
        AZ_SCRIPTABLE_EBUS_EVENT(SetGroupTag,const char*, const char*)
        AZ_SCRIPTABLE_EBUS_EVENT(ClearGroup,const char*)
        AZ_SCRIPTABLE_EBUS_EVENT(SetScopeContext,const char*, const AZ::EntityId&, const char*)
        AZ_SCRIPTABLE_EBUS_EVENT(ClearScopeContext,const char*)
        AZ_SCRIPTABLE_EBUS_EVENT(StopRequest, FragmentRequestId)
        AZ_SCRIPTABLE_EBUS_EVENT_RESULT(IAction::EStatus, IAction::EStatus::None, GetRequestStatus, FragmentRequestId)
        AZ_SCRIPTABLE_EBUS_EVENT(ForceFinishRequest, FragmentRequestId)
        AZ_SCRIPTABLE_EBUS_EVENT(SetRequestSpeedBias, FragmentRequestId,float)
        AZ_SCRIPTABLE_EBUS_EVENT_RESULT(float, -1, GetRequestSpeedBias, FragmentRequestId)
        AZ_SCRIPTABLE_EBUS_EVENT(SetRequestAnimWeight, FragmentRequestId, float)
        AZ_SCRIPTABLE_EBUS_EVENT_RESULT(float, -1, GetRequestAnimWeight, FragmentRequestId)
        )

        FragmentRequestId MannequinComponent::s_nextRequestId = s_invalidRequestId;

    //////////////////////////////////////////////////////////////////////////
    // Helper functions

    const SControllerDef* MannequinComponent::LoadControllerDefinition(const char* filename)
    {
        const SControllerDef* controllerDefinition = nullptr;

        if (filename)
        {
            IMannequin& mannequinSys = gEnv->pGame->GetIGameFramework()->GetMannequinInterface();
            controllerDefinition = mannequinSys.GetAnimationDatabaseManager().LoadControllerDef(filename);
            AZ_Warning("Mannequin component", controllerDefinition, "Controller definition file located at %s could not be loaded", filename);
        }

        AZ_Warning("Mannequin component", filename, "Controller definition file path was invalid");

        return controllerDefinition;
    }

    /**
    * \brief Creates an animation controller for a given mannequin controller definition file (asset reference)
    * \param Mannequin controller definition file asset reference
    * \return The newly created IActionController or null if the controller could not be created
    */
    IActionController* MannequinComponent::CreateAnimationController(const AzFramework::SimpleAssetReference<MannequinControllerDefinitionAsset>& mannequinControllerDef)
    {
        IActionController* controller = nullptr;
        const AZStd::string& assetPath = mannequinControllerDef.GetAssetPath();
        if (!assetPath.empty())
        {
            const SControllerDef* controllerDefinition = MannequinComponent::LoadControllerDefinition(assetPath.c_str());

            if (controllerDefinition)
            {
                IMannequin& mannequinSys = gEnv->pGame->GetIGameFramework()->GetMannequinInterface();
                m_animationContext = new SAnimationContext(*controllerDefinition);
                controller = mannequinSys.CreateActionController(GetEntityId(), *m_animationContext);
            }
        }

        AZ_Warning("Mannequin Component", controller, "Action controller could not be created for the controller definition file located at %s", assetPath.empty() ? "NO FILE PROVIDED" : assetPath.c_str());

        return controller;
    }

    bool MannequinComponent::IsFragmentIdValid(FragmentID id) const
    {
        return (id != FRAGMENT_ID_INVALID) && (id < m_actionController->GetContext().controllerDef.m_fragmentIDs.GetNum());
    }

    bool MannequinComponent::IsTagIdValid(TagID id) const
    {
        return id != TAG_ID_INVALID;
    }

    bool MannequinComponent::IsGroupIdValid(TagGroupID id) const
    {
        return (id != GROUP_ID_NONE) && (id < m_actionController->GetContext().controllerDef.m_tags.GetNum());
    }

    bool MannequinComponent::IsScopeContextIdValid(TagID id) const
    {
        return (id != SCOPE_CONTEXT_ID_INVALID) && (id < m_actionController->GetContext().controllerDef.m_scopeContexts.GetNum());
    }

    //////////////////////////////////////////////////////////////////////////

    //////////////////////////////////////////////////////////////////////////
    // MannequinComponent Implementation

    MannequinComponent::MannequinComponent()
        : m_actionController(nullptr)
    {
    }


    void MannequinComponent::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);

        if (serializeContext)
        {
            serializeContext->Class<MannequinComponent, AZ::Component>()
                ->Version(1)
                ->Field("Controller Definition", &MannequinComponent::m_controllerDefinition);
        }

        AZ::ScriptContext* script = azrtti_cast<AZ::ScriptContext*>(context);
        if (script)
        {
            ScriptableEBus_MannequinRequestsBus::Reflect(script);
        }
    }

    void MannequinComponent::Activate()
    {
        m_actionController = CreateAnimationController(m_controllerDefinition);
        if (m_actionController)
        {
            MannequinRequestsBus::Handler::BusConnect(GetEntityId());
            AZ::TickBus::Handler::BusConnect();
        }

        AZ_Error("Mannequin Component", m_actionController, "Action controller could not be created");
    }

    void MannequinComponent::Deactivate()
    {
        MannequinRequestsBus::Handler::BusDisconnect();
        AZ::TickBus::Handler::BusDisconnect();

        if (m_actionController)
        {
            m_actionController->ClearAllScopeContexts(true);
            m_actionController->Release();
            m_actionController = nullptr;

            delete m_animationContext;
            m_animationContext = nullptr;
        }
    }

    FragmentRequestId MannequinComponent::QueueFragment(int priority, const char* fragmentName, const char* fragTags, bool isPersistent)
    {
        AZ::Crc32 fragmentCrc(fragmentName);
        const FragmentID fragID = m_actionController->GetContext().controllerDef.m_fragmentIDs.Find(fragmentCrc);
        bool isValidFragmentId = IsFragmentIdValid(fragID);

        if (isValidFragmentId)
        {
            return QueueFragmentById(priority, fragID, fragTags, isPersistent);
        }

        AZ_Warning("Mannequin component", isValidFragmentId, "No fragment called %s was found", fragmentName);

        return MannequinRequests::s_invalidRequestId;
    }

    FragmentRequestId MannequinComponent::QueueFragmentById(int priority, FragmentID fragmentId, const char* fragTags, bool isPersistent)
    {
        if (IsFragmentIdValid(fragmentId))
        {
            TagState tagState = TAG_STATE_EMPTY;

            if ((fragTags) && (fragTags[0] != '\0'))
            {
                const CTagDefinition* tagDefinition = m_actionController->GetTagDefinition(fragmentId);

                if (tagDefinition)
                {
                    AZStd::vector<AZStd::string> tags;
                    AzFramework::StringFunc::Tokenize(fragTags, tags, "+", false, false);

                    for (const AZStd::string& fragmentTag : tags)
                    {
                        AZ::Crc32 fragmentTagCrc(fragmentTag.c_str());
                        const TagID tagID = tagDefinition->Find(fragmentTagCrc);
                        bool tagFound = IsTagIdValid(tagID);
                        if (tagFound)
                        {
                            tagDefinition->Set(tagState, tagID, true);
                        }
                        AZ_Warning("Mannequin Component", tagFound, "No Fragment Tag with name %s exists", fragmentTag.c_str());
                    }
                }
            }

            AZ::u32 flags = 0;
            if (isPersistent)
            {
                flags |= IAction::Interruptable;
            }

            IActionPtr action = new TAction<SAnimationContext>(priority, fragmentId, tagState, flags);
            m_actionController->Queue(*action);

            FragmentRequestId nextRequestId = s_nextRequestId++;
            m_currentlyActiveRequests.insert(AZStd::make_pair<FragmentRequestId, IAction*>(nextRequestId, action));
            return nextRequestId;
        }

        return MannequinRequests::s_invalidRequestId;
    }

    void MannequinComponent::PauseAll()
    {
        m_actionController->Pause();
    }
    
    void MannequinComponent::ResumeAll(IActionController::EResumeFlags resumeFlag)
    {
        m_actionController->Resume(resumeFlag);
    }

    TagID MannequinComponent::GetTagIdFromName(const char* name) const
    {
        TagID id = TAG_ID_INVALID;

        if (name)
        {
            id = m_actionController->GetContext().state.GetDef().Find(name);
        }

        return id;
    }

    void MannequinComponent::SetTag(const char* tagName)
    {
        bool isTagValid = (tagName != nullptr);
        if (isTagValid)
        {
            TagID tagId = GetTagIdFromName(tagName);
            isTagValid = IsTagIdValid(tagId);
            if (isTagValid)
            {
                SetTagById(tagId);
            }
        }
        AZ_Warning("Mannequin Component", isTagValid, "Tag name %s was invalid", tagName == nullptr ? "NO NAME" : tagName);
    }

    void MannequinComponent::SetTagById(TagID tagId)
    {
        SetTagState(tagId, true);
    }

    void MannequinComponent::ClearTag(const char* tagName)
    {
        bool isTagValid = (tagName != nullptr);
        if (isTagValid)
        {
            TagID tagId = GetTagIdFromName(tagName);
            isTagValid = IsTagIdValid(tagId);
            if (isTagValid)
            {
                ClearTagById(tagId);
            }
        }
        AZ_Warning("Mannequin Component", isTagValid, "Tag name %s was invalid", tagName == nullptr ? "NO NAME" : tagName);
    }

    void MannequinComponent::ClearTagById(TagID tagId)
    {
        SetTagState(tagId, false);
    }

    void MannequinComponent::SetTagState(TagID tagId, bool newState)
    {
        bool isTagValid = IsTagIdValid(tagId);
        if (isTagValid)
        {
            m_actionController->GetContext().state.Set(tagId, newState);
        }
        AZ_Warning("Mannequin Component", isTagValid, "Tag Id %i is invalid", tagId);
    }

    TagGroupID MannequinComponent::GetTagGroupId(const char* name) const
    {
        TagGroupID id = GROUP_ID_NONE;
        if (name)
        {
            id = m_actionController->GetContext().controllerDef.m_tags.FindGroup(name);
        }
        return id;
    }

    void MannequinComponent::SetGroupTag(const char* groupName, const char* tagName)
    {
        bool isGroupNameValid = (groupName != nullptr);
        if (isGroupNameValid)
        {
            TagGroupID groupId = GetTagGroupId(groupName);
            isGroupNameValid = IsGroupIdValid(groupId);

            if (isGroupNameValid)
            {
                bool isTagNameValid = (tagName != nullptr);
                TagID tagId = GetTagGroupId(tagName);
                isTagNameValid = IsTagIdValid(tagId);

                if (isTagNameValid)
                {
                    SetGroupTagById(groupId, tagId);
                }
                AZ_Warning("Mannequin Component", isTagNameValid, "Tag name %s was invalid", tagName == nullptr ? "NO NAME" : tagName);
            }
        }
        AZ_Warning("Mannequin Component", isGroupNameValid, "Group name %s was invalid", groupName == nullptr ? "NO NAME" : groupName);
    }

    void MannequinComponent::SetGroupTagById(TagGroupID groupId, TagID tagId)
    {
        bool isValidGroupId = IsGroupIdValid(groupId);
        bool isValidTagId = IsTagIdValid(tagId);
        if (isValidGroupId && isValidTagId)
        {
            m_actionController->GetContext().state.SetGroup(groupId, tagId);
        }
        else
        {
            AZ_Warning("Mannequin Component", isValidGroupId, "Group Id %i was invalid", groupId);
            AZ_Warning("Mannequin Component", isValidTagId, "Tag Id %i was invalid", tagId);
        }
    }

    void MannequinComponent::ClearGroup(const char* groupName)
    {
        bool isValidGroup = (groupName != nullptr);

        if (isValidGroup)
        {
            TagGroupID groupId = GetTagGroupId(groupName);
            isValidGroup = IsGroupIdValid(groupId);
            if (isValidGroup)
            {
                ClearGroupById(groupId);
            }
        }
        AZ_Warning("Mannequin Component", isValidGroup, "Group name %s was invalid", groupName == nullptr ? "NO NAME" : groupName);
    }

    void MannequinComponent::ClearGroupById(TagGroupID groupId)
    {
        bool isValidGroupId = IsGroupIdValid(groupId);
        if (isValidGroupId)
        {
            m_actionController->GetContext().state.ClearGroup(groupId);
        }
        AZ_Warning("Mannequin Component", isValidGroupId, "Group Id %i was invalid", groupId);
    }

    void MannequinComponent::SetScopeContext(const char* scopeContextName, const AZ::EntityId& entityId, const char* animationDatabaseName)
    {
        if (m_actionController)
        {
            bool isScopeContextValid = (scopeContextName != nullptr);
            if (isScopeContextValid)
            {
                // Find the scope context attached to the action controller
                const int currentScopeContext = m_actionController->GetContext().controllerDef.m_scopeContexts.Find(scopeContextName);
                isScopeContextValid = IsScopeContextIdValid(currentScopeContext);
                if (isScopeContextValid)
                {
                    SetScopeContextById(currentScopeContext, entityId, animationDatabaseName);
                }
            }
            AZ_Warning("Mannequin Component", isScopeContextValid, "Scope context %s was invalid", (scopeContextName == nullptr) ? "NO NAME" : scopeContextName);
        }
    }

    void MannequinComponent::SetScopeContextById(TagID scopeContextId, const AZ::EntityId& entityId, const char* animationDatabaseName)
    {
        if (m_actionController)
        {
            bool isAnimationDatabaseValid = (animationDatabaseName != nullptr);

            if (isAnimationDatabaseValid)
            {
                bool isScopeContextValid = IsScopeContextIdValid(scopeContextId);
                if (isScopeContextValid)
                {
                    bool isEntityIdValid = entityId.IsValid();
                    if (isEntityIdValid)
                    {
                        IMannequin& mannequinSys = gEnv->pGame->GetIGameFramework()->GetMannequinInterface();

                        // Load the indicated anim database
                        const IAnimationDatabase* animationDatabase = mannequinSys.GetAnimationDatabaseManager().Load(animationDatabaseName);

                        isAnimationDatabaseValid = (animationDatabase != nullptr);
                        if (isAnimationDatabaseValid)
                        {
                            m_actionController->SetScopeContext(scopeContextId, entityId, animationDatabase);
                        }
                    }
                    AZ_Warning("Mannequin Component", isEntityIdValid, "Entity Id %i was invalid", entityId);
                }
                AZ_Warning("Mannequin Component", isScopeContextValid, "Scope context id %i was invalid", scopeContextId);
            }
            AZ_Warning("Mannequin Component", isAnimationDatabaseValid, "Asset database file %s was invalid", (animationDatabaseName == nullptr) ? "NO FILE" : animationDatabaseName);
        }
    }

    void MannequinComponent::ClearScopeContext(const char* scopeContextName)
    {
        if (m_actionController)
        {
            bool isScopeContextValid = (scopeContextName != nullptr);
            if (isScopeContextValid)
            {
                // Find the scope context attached to the action controller
                const int currentScopeContext = m_actionController->GetContext().controllerDef.m_scopeContexts.Find(scopeContextName);
                isScopeContextValid = IsScopeContextIdValid(currentScopeContext);
                if (isScopeContextValid)
                {
                    ClearScopeContextById(currentScopeContext);
                }
            }
            AZ_Warning("Mannequin Component", isScopeContextValid, "Scope context name %s was invalid", scopeContextName ? "NO NAME" : scopeContextName);
        }
    }
    void MannequinComponent::ClearScopeContextById(const TagID scopeContextId)
    {
        bool isScopeContextValid = IsScopeContextIdValid(scopeContextId);
        if (isScopeContextValid)
        {
            m_actionController->ClearScopeContext(scopeContextId);
        }
        AZ_Warning("Mannequin Component", isScopeContextValid, "Scope context id %i was invalid", scopeContextId);
    }


    IActionController* MannequinComponent::GetActionController()
    {
        return m_actionController;
    }

    IActionPtr MannequinComponent::GetActionForRequestId(FragmentRequestId requestId)
    {
        auto activeRequest = m_currentlyActiveRequests.find(requestId);
        if (activeRequest != m_currentlyActiveRequests.end())
        {
            return activeRequest->second;
        }
        else
        {
            return nullptr;
        }
    }

    void MannequinComponent::StopRequest(FragmentRequestId requestId)
    {
        IActionPtr action = GetActionForRequestId(requestId);
        if (action != nullptr)
        {
            action->Stop();
        }

        AZ_Warning("Mannequin Component", action, "No valid action found for request id %i", requestId);
    }

    IAction::EStatus MannequinComponent::GetRequestStatus(FragmentRequestId requestId)
    {
        IActionPtr action = GetActionForRequestId(requestId);
        if (action)
        {
            return action->GetStatus();
        }

        AZ_Warning("Mannequin Component",action,"No valid action found for request id %i", requestId);
        return IAction::EStatus::None;
    }

    void MannequinComponent::ForceFinishRequest(FragmentRequestId requestId)
    {
        IActionPtr action = GetActionForRequestId(requestId);
        if (action)
        {
            action->ForceFinish();
        }

        AZ_Warning("Mannequin Component", action, "No valid action found for request id %i", requestId);
    }

    void MannequinComponent::SetRequestSpeedBias(FragmentRequestId requestId, float speedBias)
    {
        IActionPtr action = GetActionForRequestId(requestId);
        if (action)
        {
            action->SetSpeedBias(speedBias);
        }

        AZ_Warning("Mannequin Component", action, "No valid action found for request id %i", requestId);
    }

    float MannequinComponent::GetRequestSpeedBias(FragmentRequestId requestId)
    {
        IActionPtr action = GetActionForRequestId(requestId);
        if (action)
        {
            return action->GetSpeedBias();
        }

        AZ_Warning("Mannequin Component", action, "No valid action found for request id %i", requestId);
        return -1;
    }
    
    void MannequinComponent::SetRequestAnimWeight(FragmentRequestId requestId, float animWeight)
    {
        IActionPtr action = GetActionForRequestId(requestId);
        if (action)
        {
            action->SetAnimWeight(animWeight);
        }

        AZ_Warning("Mannequin Component", action, "No valid action found for request id %i", requestId);
    }
    
    float MannequinComponent::GetRequestAnimWeight(FragmentRequestId requestId)
    {
        IActionPtr action = GetActionForRequestId(requestId);
        if (action)
        {
            return action->GetAnimWeight();
        }

        AZ_Warning("Mannequin Component", action, "No valid action found for request id %i", requestId);
        return -1;
    }

    void MannequinComponent::OnTick(float deltaTime, AZ::ScriptTimePoint /*time*/)
    {
        if (m_actionController)
        {
            m_actionController->Update(deltaTime);
        }
    }

    //////////////////////////////////////////////////////////////////////////
} // namespace LmbrCentral
