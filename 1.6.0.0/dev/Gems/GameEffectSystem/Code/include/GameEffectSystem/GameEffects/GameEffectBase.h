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
#ifndef _GAMEEFFECTBASE_H_
#define _GAMEEFFECTBASE_H_

#include <GameEffectSystem/GameEffects/IGameEffect.h>
#include <GameEffectSystem/GameEffectsSystemDefines.h>
#include "TypeLibrary.h"
#include <IActorSystem.h>
#include <ICryAnimation.h>
#include <IAnimatedCharacter.h>

// Forward declares
struct SGameEffectParams;

//==================================================================================================
// Name: Flag macros
// Desc: Flag macros to make code more readable
// Author: James Chilvers
//==================================================================================================
#define SET_FLAG(currentFlags, flag, state) ((state) ? (currentFlags |= flag) : (currentFlags &= ~flag));
#define IS_FLAG_SET(currentFlags, flag) ((currentFlags & flag) ? true : false)
//--------------------------------------------------------------------------------------------------

//==================================================================================================
// Name: CGameEffect
// Desc: Game effect - Ideal for handling a specific visual game feature
// Author: James Chilvers
//==================================================================================================
class CGameEffect
    : public IGameEffect
{
    DECLARE_TYPE(CGameEffect, IGameEffect); // Exposes this type for SoftCoding

public:
    CGameEffect();
    virtual ~CGameEffect();

    void Initialize(const SGameEffectParams* gameEffectParams = NULL) override;
    void Release() override;
    void Update(float frameTime) override;

    void SetActive(bool isActive) override;

    void SetFlag(uint32 flag, bool state) override { SET_FLAG(m_flags, flag, state); }
    bool IsFlagSet(uint32 flag) const override { return IS_FLAG_SET(m_flags, flag); }
    uint32 GetFlags() const override { return m_flags; }
    void SetFlags(uint32 flags) override { m_flags = flags; }

    void GetMemoryUsage(ICrySizer* pSizer) const override { pSizer->AddObject(this, sizeof(*this)); }

    void LoadData(IItemParamsNode* params) override { }
    void UnloadData() override { }

protected:
    // General helper functions
    void SpawnParticlesOnSkeleton(IEntity* pEntity, IParticleEmitter* pParticleEmitter,
        uint32 numParticles, float maxHeightScale = 1.0f) const;
    void SetMaterialOnEntity(IMaterial* pMaterial, EntityId entityId,
        PodArray<int>* pBodyAttachmentIndexArray);
    void SetMaterialOnEntity(IMaterial* pMaterial, EntityId entityId,
        PodArray<string>* bodyAttachmentNameArray);

    // General data functions
    static void ReadAttachmentNames(const IItemParamsNode* paramNode, PodArray<string>* attachmentNameArray);
    void InitAttachmentIndexArray(PodArray<int>* attachmentIndexArray,
        PodArray<string>* pAttachmentNameArray, EntityId entityId) const;
    static IMaterial* LoadMaterial(const char* pMaterialName);
    static IParticleEffect* LoadParticleEffect(const char* pParticleEffectName);

    static bool IsEntity3rdPerson(EntityId entityId);

private:
    IGameEffect* Next() const override { return m_next; }
    IGameEffect* Prev() const override { return m_prev; }
    void SetNext(IGameEffect* newNext) override { m_next = newNext; }
    void SetPrev(IGameEffect* newPrev) override { m_prev = newPrev; }

    IGameEffect* m_prev;
    IGameEffect* m_next;
    uint16 m_flags;
    IGameEffectSystem* m_gameEffectSystem = nullptr;

#if DEBUG_GAME_FX_SYSTEM
    CryFixedStringT<32> m_debugName;
#endif
}; //-----------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: CGameEffect
// Desc: Constructor
//--------------------------------------------------------------------------------------------------
inline CGameEffect::CGameEffect()
{
    m_prev = NULL;
    m_next = NULL;
    m_flags = 0;
    EBUS_EVENT_RESULT(m_gameEffectSystem, GameEffectSystemRequestBus, GetIGameEffectSystem);
} //------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: ~CGameEffect
// Desc: Destructor
//--------------------------------------------------------------------------------------------------
inline CGameEffect::~CGameEffect()
{
#if DEBUG_GAME_FX_SYSTEM
    // Output message if effect hasn't been released before being deleted
    const bool bEffectIsReleased =
        (m_flags & GAME_EFFECT_RELEASED) ||     // -> Needs to be released before deleted
        !(m_flags & GAME_EFFECT_INITIALISED) || // -> Except when not initialised
        (gEnv->IsEditor()); // -> Or the editor (memory safely released by editor)
    if (!bEffectIsReleased)
    {
        string dbgMessage = m_debugName + " being destroyed without being released first";
        FX_ASSERT_MESSAGE(bEffectIsReleased, dbgMessage.c_str());
    }
#endif

    if (m_gameEffectSystem)
    {
        // -> Effect should have been released and been unregistered, but to avoid
        //    crashes call unregister here too
        m_gameEffectSystem->UnRegisterEffect(this);
    }
} //------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: Initialise
// Desc: Initializes game effect
//--------------------------------------------------------------------------------------------------
inline void CGameEffect::Initialize(const SGameEffectParams* gameEffectParams)
{
#if DEBUG_GAME_FX_SYSTEM
    m_debugName = GetName(); // Store name so it can be accessed in destructor and debugging
#endif

    if (!IsFlagSet(GAME_EFFECT_INITIALISED))
    {
        SGameEffectParams params;
        if (gameEffectParams)
        {
            params = *gameEffectParams;
        }

        SetFlag(GAME_EFFECT_AUTO_UPDATES_WHEN_ACTIVE, params.autoUpdatesWhenActive);
        SetFlag(GAME_EFFECT_AUTO_UPDATES_WHEN_NOT_ACTIVE, params.autoUpdatesWhenNotActive);
        SetFlag(GAME_EFFECT_AUTO_RELEASE, params.autoRelease);
        SetFlag(GAME_EFFECT_AUTO_DELETE, params.autoDelete);

        m_gameEffectSystem->RegisterEffect(this);

        SetFlag(GAME_EFFECT_INITIALISED, true);
        SetFlag(GAME_EFFECT_RELEASED, false);
    }
} //------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: Release
// Desc: Releases game effect
//--------------------------------------------------------------------------------------------------
inline void CGameEffect::Release()
{
    SetFlag(GAME_EFFECT_RELEASING, true);
    if (IsFlagSet(GAME_EFFECT_ACTIVE))
    {
        SetActive(false);
    }
    m_gameEffectSystem->UnRegisterEffect(this);
    SetFlag(GAME_EFFECT_INITIALISED, false);
    SetFlag(GAME_EFFECT_RELEASING, false);
    SetFlag(GAME_EFFECT_RELEASED, true);
} //------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: Update
// Desc: Updates game effect
//--------------------------------------------------------------------------------------------------
inline void CGameEffect::Update(float frameTime)
{
    FX_ASSERT_MESSAGE(IsFlagSet(GAME_EFFECT_INITIALISED),
        "Effect being updated without being initialised first");
    FX_ASSERT_MESSAGE((IsFlagSet(GAME_EFFECT_RELEASED) == false),
        "Effect being updated after being released");
} //------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: SetActive
// Desc: Sets active status
//--------------------------------------------------------------------------------------------------
inline void CGameEffect::SetActive(bool isActive)
{
    FX_ASSERT_MESSAGE(IsFlagSet(GAME_EFFECT_INITIALISED),
        "Effect changing active status without being initialised first");
    FX_ASSERT_MESSAGE((IsFlagSet(GAME_EFFECT_RELEASED) == false),
        "Effect changing active status after being released");

    SetFlag(GAME_EFFECT_ACTIVE, isActive);
    m_gameEffectSystem->RegisterEffect(this); // Re-register effect with game effects system
} //------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: SpawnParticlesOnSkeleton
// Desc: Spawn particles on Skeleton
//--------------------------------------------------------------------------------------------------
inline void CGameEffect::SpawnParticlesOnSkeleton(IEntity* pEntity, IParticleEmitter* pParticleEmitter,
    uint32 numParticles, float maxHeightScale) const
{
    if ((pEntity) && (numParticles > 0) && (pParticleEmitter) && (maxHeightScale > 0.0f))
    {
        ICharacterInstance* pCharacter = pEntity->GetCharacter(0);
        if (pCharacter)
        {
            IDefaultSkeleton& rIDefaultSkeleton = pCharacter->GetIDefaultSkeleton();
            ISkeletonPose* pPose = pCharacter->GetISkeletonPose();
            if (pPose)
            {
                Vec3 animPos;
                Quat animRot;

                IActor* pActor = gEnv->pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pEntity->GetId());
                if (pActor) // First try to get animation data
                {
                    QuatT animLoc = pActor->GetAnimatedCharacter()->GetAnimLocation();
                    animPos = animLoc.t;
                    animRot = animLoc.q;
                }
                else // If no actor, then use entity data
                {
                    animPos = pEntity->GetWorldPos();
                    animRot = pEntity->GetWorldRotation();
                }

                animRot.Invert();

                AABB bbox;
                pEntity->GetLocalBounds(bbox);
                float bbHeight = bbox.max.z - bbox.min.z;
                // Avoid division by 0
                if (bbHeight == 0)
                {
                    bbHeight = 0.0001f;
                }

                const uint32 numJoints = rIDefaultSkeleton.GetJointCount();

                while (numParticles > 0)
                {
                    int id = cry_random<int>(0, numJoints);
                    int parentId = rIDefaultSkeleton.GetJointParentIDByID(id);

                    if (parentId > 0)
                    {
                        QuatT boneQuat = pPose->GetAbsJointByID(id);
                        QuatT parentBoneQuat = pPose->GetAbsJointByID(parentId);
                        float lerpScale = cry_frand();

                        QuatTS loc(IDENTITY);
                        loc.t = LERP(boneQuat.t, parentBoneQuat.t, lerpScale);

                        float heightScale = ((loc.t.z - bbox.min.z) / bbHeight);
                        if (heightScale < maxHeightScale)
                        {
                            loc.t = loc.t * animRot;
                            loc.t = loc.t + animPos;

                            pParticleEmitter->EmitParticle(NULL, NULL, &loc);
                        }
                    }
                }
            }
        }
    }
} //------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: SetMaterialOnEntity
// Desc: Sets material on entity
//--------------------------------------------------------------------------------------------------
inline void CGameEffect::SetMaterialOnEntity(IMaterial* pMaterial, EntityId entityId, PodArray<int>* pBodyAttachmentIndexArray)
{
    if (pMaterial && pBodyAttachmentIndexArray)
    {
        IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entityId);
        if (pEntity)
        {
            SEntitySlotInfo slotInfo;
            bool gotSlotInfo = pEntity->GetSlotInfo(0, slotInfo);
            if (gotSlotInfo && slotInfo.pCharacter)
            {
                IAttachmentManager* pAttachmentManager =
                    slotInfo.pCharacter->GetIAttachmentManager();
                if (pAttachmentManager)
                {
                    int attachmentCount = pBodyAttachmentIndexArray->size();
                    for (int i = 0; i < attachmentCount; i++)
                    {
                        IAttachment* attachment = pAttachmentManager->GetInterfaceByIndex(
                                (*pBodyAttachmentIndexArray)[i]);

                        if (attachment)
                        {
                            IAttachmentObject* attachmentObj = attachment->GetIAttachmentObject();
                            if (attachmentObj)
                            {
                                attachmentObj->SetReplacementMaterial(pMaterial);
                            }
                        }
                    }
                }
            }
        }
    }
} //------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: SetMaterialOnEntity
// Desc: Sets material on entity
//--------------------------------------------------------------------------------------------------
inline void CGameEffect::SetMaterialOnEntity(IMaterial* pMaterial, EntityId entityId, PodArray<string>* bodyAttachmentNameArray)
{
    if (pMaterial && bodyAttachmentNameArray)
    {
        IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entityId);
        if (pEntity)
        {
            SEntitySlotInfo slotInfo;
            bool gotSlotInfo = pEntity->GetSlotInfo(0, slotInfo);
            if (gotSlotInfo && slotInfo.pCharacter)
            {
                IAttachmentManager* pAttachmentManager =
                    slotInfo.pCharacter->GetIAttachmentManager();
                if (pAttachmentManager)
                {
                    int attachmentCount = bodyAttachmentNameArray->size();
                    for (int i = 0; i < attachmentCount; i++)
                    {
                        IAttachment* attachment = pAttachmentManager->GetInterfaceByName(
                                ((*bodyAttachmentNameArray)[i]).c_str());

                        if (attachment)
                        {
                            IAttachmentObject* attachmentObj = attachment->GetIAttachmentObject();
                            if (attachmentObj)
                            {
                                attachmentObj->SetReplacementMaterial(pMaterial);
                            }
                        }
                    }
                }
            }
        }
    }
} //------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: ReadAttachmentNames
// Desc: Reads attachment names
//--------------------------------------------------------------------------------------------------
inline void CGameEffect::ReadAttachmentNames(const IItemParamsNode* pParamNode, PodArray<string>* pAttachmentNameArray)
{
    if (pParamNode && pAttachmentNameArray)
    {
        int attachmentCount = pParamNode->GetChildCount();
        pAttachmentNameArray->resize(attachmentCount);
        for (int i = 0; i < attachmentCount; i++)
        {
            const IItemParamsNode* attachmentNode = pParamNode->GetChild(i);
            if (attachmentNode)
            {
                (*pAttachmentNameArray)[i] = attachmentNode->GetAttribute("name");
            }
        }
    }
} //------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: InitAttachmentIndexArray
// Desc: Initialises attachment index array
//--------------------------------------------------------------------------------------------------
inline void CGameEffect::InitAttachmentIndexArray(PodArray<int>* attachmentIndexArray,
    PodArray<string>* pAttachmentNameArray, EntityId entityId) const
{
    if (attachmentIndexArray && pAttachmentNameArray && (entityId != 0))
    {
        if (attachmentIndexArray->size() == 0)
        {
            // Store attachment index array
            const int attachmentNameCount = pAttachmentNameArray->size();

            SEntitySlotInfo slotInfo;
            IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entityId);
            if (pEntity)
            {
                bool gotEntitySlotInfo = pEntity->GetSlotInfo(0, slotInfo);
                if (gotEntitySlotInfo)
                {
                    attachmentIndexArray->reserve(attachmentNameCount);

                    if (slotInfo.pCharacter)
                    {
                        IAttachmentManager* attachmentManager =
                            slotInfo.pCharacter->GetIAttachmentManager();
                        if (attachmentManager)
                        {
                            const int attachmentCount = attachmentManager->GetAttachmentCount();

                            for (int n = 0; n < attachmentNameCount; n++)
                            {
                                for (int a = 0; a < attachmentCount; a++)
                                {
                                    IAttachment* attachment =
                                        attachmentManager->GetInterfaceByIndex(a);
                                    if (attachment)
                                    {
                                        const char* currentAttachmentName = attachment->GetName();
                                        if (strcmp(currentAttachmentName,
                                                (*pAttachmentNameArray)[n].c_str()) == 0)
                                        {
                                            attachmentIndexArray->push_back(a);
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
} //------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: LoadMaterial
// Desc: Loads and calls AddRef on material
//--------------------------------------------------------------------------------------------------
inline IMaterial* CGameEffect::LoadMaterial(const char* pMaterialName)
{
    IMaterial* pMaterial = NULL;
    I3DEngine* p3DEngine = gEnv->p3DEngine;
    if (pMaterialName && p3DEngine)
    {
        IMaterialManager* pMaterialManager = p3DEngine->GetMaterialManager();
        if (pMaterialManager)
        {
            pMaterial = pMaterialManager->LoadMaterial(pMaterialName);
            if (pMaterial)
            {
                pMaterial->AddRef();
            }
        }
    }
    return pMaterial;
} //------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: LoadParticleEffect
// Desc: Loads and calls AddRef on a particle effect
//--------------------------------------------------------------------------------------------------
inline IParticleEffect* CGameEffect::LoadParticleEffect(const char* pParticleEffectName)
{
    IParticleEffect* pParticleEffect = NULL;
    IParticleManager* pParticleManager = gEnv->pParticleManager;

    if (pParticleEffectName && pParticleManager)
    {
        pParticleEffect = gEnv->pParticleManager->FindEffect(pParticleEffectName);
        if (pParticleEffect)
        {
            pParticleEffect->AddRef();
        }
    }

    return pParticleEffect;
} //------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: IsEntity3rdPerson
// Desc: Returns 3rd person state
//--------------------------------------------------------------------------------------------------
inline bool CGameEffect::IsEntity3rdPerson(EntityId entityId)
{
    bool bIs3rdPerson = true;

    IGame* pGame = gEnv->pGame;
    if (pGame)
    {
        IGameFramework* pGameFramework = pGame->GetIGameFramework();
        if (pGameFramework)
        {
            EntityId clientEntityId = pGameFramework->GetClientActorId();
            if (clientEntityId == entityId)
            {
                bIs3rdPerson = pGameFramework->GetClientActor()->IsThirdPerson();
            }
        }
    }

    return bIs3rdPerson;
} //------------------------------------------------------------------------------------------------

#endif//_GAMEEFFECTBASE_H_
