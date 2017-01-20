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

#if defined(LMBR_CENTRAL_EDITOR)

#include <STLPortConfig.h>

// STL Port in debug for debug builds
#if defined(_DEBUG)
#  define _STLP_DEBUG 1
#endif

#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN        // Exclude rarely-used stuff from Windows headers
#endif

// Modify the following defines if you have to target a platform prior to the ones specified below.
// Refer to MSDN for the latest info on corresponding values for different platforms.
#ifndef WINVER              // Allow use of features specific to Windows XP or later.
#define WINVER 0x0600       // Change this to the appropriate value to target other versions of Windows.
#endif

#ifndef _WIN32_WINNT        // Allow use of features specific to Windows XP or later.
#define _WIN32_WINNT 0x0600 // Change this to the appropriate value to target other versions of Windows.
#endif

#ifndef _WIN32_WINDOWS      // Allow use of features specific to Windows 98 or later.
#define _WIN32_WINDOWS 0x0410 // Change this to the appropriate value to target Windows Me or later.
#endif

#ifndef _WIN32_IE           // Allow use of features specific to IE 6.0 or later.
#define _WIN32_IE 0x0600    // Change this to the appropriate value to target other versions of IE.
#endif

#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS  // some CString constructors will be explicit
#define _AFXDLL

#include <afxwin.h>         // MFC core and standard components
#include <afxext.h>         // MFC extensions
#ifndef _AFX_NO_OLE_SUPPORT
#include <afxole.h>         // MFC OLE classes
#include <afxodlgs.h>       // MFC OLE dialog classes
#include <afxdisp.h>        // MFC Automation classes
#endif // _AFX_NO_OLE_SUPPORT

#ifndef _AFX_NO_DB_SUPPORT
#include <afxdb.h>          // MFC ODBC database classes
#endif // _AFX_NO_DB_SUPPORT

#ifndef _AFX_NO_DAO_SUPPORT
#include <afxdao.h>         // MFC DAO database classes
#endif // _AFX_NO_DAO_SUPPORT

#ifndef _AFX_NO_OLE_SUPPORT
#include <afxdtctl.h>       // MFC support for Internet Explorer 4 Common Controls
#endif
#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>         // MFC support for Windows Common Controls
#endif // _AFX_NO_AFXCMN_SUPPORT

#pragma warning(disable: 4244) // warning C4244: 'argument' : conversion from 'float' to 'uint8', possible loss of data
#pragma warning(disable: 4800) // 'int' : forcing value to bool 'true' or 'false' (performance warning)
#pragma warning(disable: 4266) // no override available for virtual member function from base 'CObject'; function is hidden
//#pragma warning(disable: 4264) // no override available for virtual member function from base 'CObject'; function is hidden

/////////////////////////////////////////////////////////////////////////////
// Engine
/////////////////////////////////////////////////////////////////////////////
#include <Cry_Math.h>
#include <ISystem.h>
#include <ISerialize.h>
#include <CryName.h>
#include <EditorDefs.h>
#include <EditTool.h>
#include <Resource.h>

/////////////////////////////////////////////////////////////////////////////
// STL
/////////////////////////////////////////////////////////////////////////////
#include <vector>
#include <list>
#include <map>
#include <set>
#include <algorithm>

/////////////////////////////////////////////////////////////////////////////
// Qt
/////////////////////////////////////////////////////////////////////////////
#include <QtCore>
#include <QtGui>
#include <QtWidgets>

#else // defined(LMBR_CENTRAL_EDITOR)

#include <platform.h> // Many CryCommon files require that this is included first.

#endif // defined(LMBR_CENTRAL_EDITOR)
