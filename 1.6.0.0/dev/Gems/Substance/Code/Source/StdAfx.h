/** @file StdAfx.h
	@brief Precompiled Header
	@author Josh Coyne - Allegorithmic (josh.coyne@allegorithmic.com)
	@date 09-14-2015
	@copyright Allegorithmic. All rights reserved.
*/
#ifndef GEM_46AED0DF_955D_4582_9583_0B4D2422A727_CODE_SOURCE_STDAFX_H
#define GEM_46AED0DF_955D_4582_9583_0B4D2422A727_CODE_SOURCE_STDAFX_H
#pragma once

#define eCryModule eCryM_Game

#include <platform.h>

#ifdef USE_SUBSTANCE
#include <CryWindows.h> // needed for UINT defines used by CImageExtensionHelper
#include <CryName.h>
#include <I3DEngine.h>
#include <ISerialize.h>
#include <IGem.h>
#define GEMSUBSTANCE_EXPORTS

#if defined (GEMSUBSTANCE_EXPORTS)
#define GEMSUBSTANCE_API DLL_EXPORT
#else
#define GEMSUBSTANCE_API DLL_IMPORT
#endif

#include "Substance/IProceduralMaterial.h"

//CVars
extern int substance_coreCount;
extern int substance_memoryBudget;

#define DIMOF(x) (sizeof(x)/sizeof(x[0]))
#endif

#endif//GEM_46AED0DF_955D_4582_9583_0B4D2422A727_CODE_SOURCE_STDAFX_H

