/** @file SubstanceAPI.h
	@brief Header for Substance API Implementation
	@author Josh Coyne - Allegorithmic (josh.coyne@allegorithmic.com)
	@date 09-14-2015
	@copyright Allegorithmic. All rights reserved.
*/
#ifndef _API_SUBSTANCE_IMPLEMENTATION_H_
#define _API_SUBSTANCE_IMPLEMENTATION_H_

#include "ISubstanceAPI.h"

class CSubstanceAPI : public ISubstanceAPI
{
public:
	virtual void AddRefDeviceTexture(IDeviceTexture* pTexture) override;
	virtual void* Alloc(size_t bytes) override;
	virtual int GetCoreCount() override;
	virtual int GetMemoryBudget() override;
	virtual bool IsEditor() const override;
	virtual ISubstanceMaterialXMLData* LoadMaterialXML(const char* path, bool bParametersOnly) override;
	virtual bool LoadTextureXML(const char* path, unsigned int& outputID, const char** material) override;
	virtual void Log(const char* msg, ...) override;
	virtual bool ReadFile(const char* path, IFileContents** pContents) override;
	virtual bool ReadTexture(const char* path, SSubstanceLoadData& loadData) override;
	virtual void ReleaseDeviceTexture(IDeviceTexture* pTexture) override;
	virtual void ReleaseSubstanceLoadData(SSubstanceLoadData& loadData) override;
	virtual void ReloadDeviceTexture(IDeviceTexture* pTexture) override;
	virtual bool WriteFile(const char* path, const char* data, size_t bytes) override;
	virtual bool RemoveFile(const char* path) override;
};

#endif //_API_SUBSTANCE_IMPLEMENTATION_H_
