/** @file ProceduralMaterialScanner.h
	@brief Multi threaded file scanner for Procedural Material Files
	@author Josh Coyne - Allegorithmic (josh.coyne@allegorithmic.com)
	@date 09-14-2015
	@copyright Allegorithmic. All rights reserved.
*/
#ifndef SUBSTANCE_PROCEDURALMATERIALEDITORPLUGIN_PROCEDURALMATERIALSCANNER_H
#define SUBSTANCE_PROCEDURALMATERIALEDITORPLUGIN_PROCEDURALMATERIALSCANNER_H
#pragma once

#if defined(USE_SUBSTANCE)

#include <IThreadTask.h>

typedef std::vector<string> TFileVector;

class CProceduralMaterialScanner : public IThreadTask
{
public:
	static CProceduralMaterialScanner *Instance();

	CProceduralMaterialScanner();

	void StartScan();
	void StopScan();

	bool GetLoadedFiles(TFileVector& files);

private:
	void AddFiles();

	static bool ScanUpdateCallback(const CString& msg);

	virtual void OnUpdate();

	virtual void Stop() {}
	virtual struct SThreadTaskInfo* GetTaskInfo() { return &m_TaskInfo; };

private:
	CryCriticalSection m_lock;
	TFileVector m_filesForUser;
	TFileVector m_files;
	bool m_bNewFiles;
	bool m_bDone;

protected:
	SThreadTaskInfo m_TaskInfo;
};

#endif // USE_SUBSTANCE
#endif //SUBSTANCE_PROCEDURALMATERIALEDITORPLUGIN_PROCEDURALMATERIALSCANNER_H
