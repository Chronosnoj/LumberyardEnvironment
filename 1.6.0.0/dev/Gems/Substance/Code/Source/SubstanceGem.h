/** @file SubstanceGem.h
	@brief Header for the Substance Gem Implementation.
	@author Josh Coyne - Allegorithmic (josh.coyne@allegorithmic.com)
	@date 09-14-2015
	@copyright Allegorithmic. All rights reserved.
*/
#ifndef GEM_46AED0DF_955D_4582_9583_0B4D2422A727_CODE_SOURCE_SUBSTANCEGEM_H
#define GEM_46AED0DF_955D_4582_9583_0B4D2422A727_CODE_SOURCE_SUBSTANCEGEM_H
#pragma once

#include <IGem.h>
#include "Substance/SubstanceBus.h"
#if defined(USE_SUBSTANCE)
#include "SubstanceAPI.h"

struct CTextureLoadHandler_Substance;
#endif // USE_SUBSTANCE


class SubstanceGem
	: public CryHooksModule
	, public SubstanceRequestBus::Handler
{
public:
	AZ_RTTI(SubstanceGem, "{4BCD80A7-A8C7-47A1-96A8-A6474D898E7B}", CryHooksModule);

	SubstanceGem();
	~SubstanceGem() override;

	void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam) override;

#if defined(USE_SUBSTANCE)
    void RegisterConsole();
	void PostGameInitialize();

	virtual int GetMinimumOutputSize() const override;
	virtual int GetMaximumOutputSize() const override;

	virtual IProceduralMaterial* GetMaterialFromPath(const char* path, bool bForceLoad) const override;
	virtual IGraphInstance* GetGraphInstance(GraphInstanceID graphInstanceID) const override;

	virtual void QueueRender(IGraphInstance* pGraphInstance) override;
	virtual ProceduralMaterialRenderUID RenderASync() override;
	virtual void RenderSync() override;
	virtual bool HasRenderCompleted(ProceduralMaterialRenderUID uid) const override;

	virtual ProceduralMaterialRenderUID RenderEditorPreviewASync() override;
	virtual void RenderEditorPreviewSync() override;

	virtual bool CreateProceduralMaterial(const char* sbsarPath, const char* smtlPath) override;
	virtual bool SaveProceduralMaterial(IProceduralMaterial* pMaterial, const char* path) override;
	virtual void RemoveProceduralMaterial(IProceduralMaterial* pMaterial) override;

	virtual ISubstanceLibAPI* GetSubstanceLibAPI() const override;

private:
	void RegisterTextureHandler();
	void UnregisterTextureHandler();

	bool LoadEngineLibrary();

	void*             m_SubstanceLib;
	ISubstanceLibAPI* m_SubstanceLibAPI;
	CSubstanceAPI     m_SubstanceAPI;
	CTextureLoadHandler_Substance* m_TextureLoadHandler;
#endif // USE_SUBSTANCE
};

#endif//GEM_46AED0DF_955D_4582_9583_0B4D2422A727_CODE_SOURCE_SUBSTANCEGEM_H