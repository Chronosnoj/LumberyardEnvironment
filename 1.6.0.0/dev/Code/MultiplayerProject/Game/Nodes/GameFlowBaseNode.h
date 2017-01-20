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

#include <IGame.h>
#include <IGameFramework.h>
#include <IGameObjectSystem.h>
#include <IGameObject.h>
#include <IPlatformOS.h>
#include <IActorSystem.h>
#include <IFlowSystem.h>

//////////////////////////////////////////////////////////////////////////
// Enum used for templating node base class
//////////////////////////////////////////////////////////////////////////
enum ENodeCloneType
{
    eNCT_Singleton,     // node has only one instance; not cloned
    eNCT_Instanced,     // new instance of the node will be created
    eNCT_Cloned         // node clone method will be called
};


//////////////////////////////////////////////////////////////////////////
// Auto registration for flow nodes. Handles both instanced and
//  singleton nodes
//////////////////////////////////////////////////////////////////////////
class CGameAutoRegFlowNodeBase
    : public IFlowNodeFactory
{
public:
    CGameAutoRegFlowNodeBase(const char* sClassName)
    {
        m_sClassName = sClassName;
        m_pNext = 0;
        if (!m_pLast)
        {
            m_pFirst = this;
        }
        else
        {
            m_pLast->m_pNext = this;
        }

        m_pLast = this;
    }

    virtual ~CGameAutoRegFlowNodeBase()
    {
        CGameAutoRegFlowNodeBase* node = m_pFirst;
        CGameAutoRegFlowNodeBase* prev = NULL;
        while (node && node != this)
        {
            prev = node;
            node = node->m_pNext;
        }

        assert(node);
        if (node)
        {
            if (prev)
            {
                prev->m_pNext = m_pNext;
            }
            else
            {
                m_pFirst = m_pNext;
            }

            if (m_pLast == this)
            {
                m_pLast = prev;
            }
        }
    }

    void AddRef() {}
    void Release() {}

    void Reset() {}

    virtual void GetMemoryUsage(ICrySizer* s) const
    {
        SIZER_SUBCOMPONENT_NAME(s, "CGameAutoRegFlowNode");
        //s->Add(*this); // static object
    }

    //////////////////////////////////////////////////////////////////////////
    const char* m_sClassName;
    CGameAutoRegFlowNodeBase* m_pNext;
    static CGameAutoRegFlowNodeBase* m_pFirst;
    static CGameAutoRegFlowNodeBase* m_pLast;
    //////////////////////////////////////////////////////////////////////////
};

template <class T>
class CGameAutoRegFlowNode
    : public CGameAutoRegFlowNodeBase
{
public:
    CGameAutoRegFlowNode(const char* sClassName)
        : CGameAutoRegFlowNodeBase(sClassName)
    {
    }

    IFlowNodePtr Create(IFlowNode::SActivationInfo* pActInfo)
    {
        PREFAST_SUPPRESS_WARNING (6326)
        if (T::myCloneType == eNCT_Singleton)
        {
            if (!m_pInstance.get())
            {
                m_pInstance = new T(pActInfo);
            }

            return m_pInstance;
        }
        else if (T::myCloneType == eNCT_Instanced)
        {
            return new T(pActInfo);
        }
        else if (T::myCloneType == eNCT_Cloned)
        {
            if (m_pInstance.get())
            {
                return m_pInstance->Clone(pActInfo);
            }
            else
            {
                m_pInstance = new T(pActInfo);
                return m_pInstance;
            }
        }
        else
        {
            assert(false);
        }
    }

private:
    IFlowNodePtr m_pInstance;   // only applies for singleton nodes
};

#if defined(WIN32) && defined(AZ_MONOLITHIC_BUILD)
#define CRY_EXPORT_STATIC_LINK_VARIABLE(Var)                \
    extern "C" { int lib_func_##Var() { return (int)&Var; } \
    }                                                       \
    __pragma(message("#pragma comment(linker,\"/include:_lib_func_"#Var "\")"))
#else
#define CRY_EXPORT_STATIC_LINK_VARIABLE(Var)
#endif

//////////////////////////////////////////////////////////////////////////
// Use this define to register a new flow node class. Handles
//  both instanced and singleton nodes.
// Ex. REGISTER_FLOW_NODE( "Delay",CFlowDelayNode )
//////////////////////////////////////////////////////////////////////////
#define REGISTER_FLOW_NODE(FlowNodeClassName, FlowNodeClass)                          \
    CGameAutoRegFlowNode<FlowNodeClass> g_AutoReg##FlowNodeClass (FlowNodeClassName); \
    CRY_EXPORT_STATIC_LINK_VARIABLE(g_AutoReg##FlowNodeClass)

#define REGISTER_FLOW_NODE_EX(FlowNodeClassName, FlowNodeClass, RegName)        \
    CGameAutoRegFlowNode<FlowNodeClass> g_AutoReg##RegName (FlowNodeClassName); \
    CRY_EXPORT_STATIC_LINK_VARIABLE(g_AutoReg##RegName)

//////////////////////////////////////////////////////////////////////////
// Internal base class for flow nodes. Node classes shouldn't be
//  derived directly from this, but should derive from CFlowBaseNode<>
//////////////////////////////////////////////////////////////////////////
class CFlowBaseNodeInternal
    : public IFlowNode
{
    template <ENodeCloneType CLONE_TYPE>
    friend class CFlowBaseNode;

private:
    // private ctor/dtor to prevent classes directly derived from this;
    //  the exception is CFlowBaseNode (friended above)
    CFlowBaseNodeInternal() { m_refs = 0; };
    virtual ~CFlowBaseNodeInternal() {}

public:
    //////////////////////////////////////////////////////////////////////////
    // IFlowNode
    virtual void AddRef() { ++m_refs; };
    virtual void Release()
    {
        if (0 >= --m_refs)
        {
            delete this;
        }
    };

    virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) = 0;
    virtual bool SerializeXML(SActivationInfo*, const XmlNodeRef&, bool) { return true; }
    virtual void Serialize(SActivationInfo*, TSerialize ser) {}
    virtual void PostSerialize(SActivationInfo*) {}
    virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo) {};
    //////////////////////////////////////////////////////////////////////////

protected:

    //--------------------------------------------------------------
    // returns true if the input entity is the local player. In single player, when the input entity is NULL, it returns true,  for backward compatibility
    bool InputEntityIsLocalPlayer(const SActivationInfo* const pActInfo) const
    {
        bool bRet = true;

        if (pActInfo->pEntity)
        {
            IActor* pActor = gEnv->pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pActInfo->pEntity->GetId());
            if (pActor != gEnv->pGame->GetIGameFramework()->GetClientActor())
            {
                bRet = false;
            }
        }
        else
        {
            if (gEnv->bMultiplayer)
            {
                bRet = false;
            }
        }

        return bRet;
    }

    // returns the actor associated with the input entity. In single player, it returns the local player if that actor does not exists.
    IActor* GetInputActor(const SActivationInfo* const pActInfo) const
    {
        IActor* pActor = pActInfo->pEntity ? gEnv->pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pActInfo->pEntity->GetId()) : NULL;

        if (!pActor && !gEnv->bMultiplayer)
        {
            pActor = gEnv->pGame->GetIGameFramework()->GetClientActor();
        }

        return pActor;
    }


private:
    int m_refs;
};

//////////////////////////////////////////////////////////////////////////
// Base class that nodes can derive from.
//  eg1: Singleton node:
//  class CSingletonNode : public CFlowBaseNode<eNCT_Singleton>
//
//  eg2: Instanced node:
//  class CInstancedNode : public CFlowBaseNode<eNCT_Instanced>
//
//  Instanced nodes must override Clone (pure virtual in base), while
//  Singleton nodes cannot - will result in a compile error.
//////////////////////////////////////////////////////////////////////////

template <ENodeCloneType CLONE_TYPE>
class CFlowBaseNode
    : public CFlowBaseNodeInternal
{
public:
    CFlowBaseNode()
        : CFlowBaseNodeInternal() {}

    static const int myCloneType = CLONE_TYPE;
};

// specialization for singleton nodes: Clone() is
//  marked so derived classes cannot override it.
template <>
class CFlowBaseNode<eNCT_Singleton>
    : public CFlowBaseNodeInternal
{
public:
    CFlowBaseNode<eNCT_Singleton>() : CFlowBaseNodeInternal()   {
    }

    virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) final { return this; }

    static const int myCloneType = eNCT_Singleton;
};