/** @file ProceduralMaterialEditorPlugin.cpp
	@brief Library hook to register plugin features
	@author Josh Coyne - Allegorithmic (josh.coyne@allegorithmic.com)
	@date 09-14-2015
	@copyright Allegorithmic. All rights reserved.
*/
#include "StdAfx.h"
#include "ProceduralMaterialEditorPlugin.h"
#include "QtViewPane.h"
#include "QProceduralMaterialEditorMainWindow.h"
#include "IResourceSelectorHost.h"

#include "CryExtension/ICryFactoryRegistry.h"

ProceduralMaterialEditorPlugin::ProceduralMaterialEditorPlugin(IEditor* editor)
    : m_registered(false)
{
#if defined(USE_SUBSTANCE)
    m_registered = RegisterMultiInstanceQtViewPane<QProceduralMaterialEditorMainWindow>(editor, "Substance Editor", "Tools");
    if (m_registered)
    {
        RegisterModuleResourceSelectors(GetIEditor()->GetResourceSelectorHost());
    }
#endif
}

void ProceduralMaterialEditorPlugin::Release()
{
#if defined(USE_SUBSTANCE)
    if (m_registered)
    {
        UnregisterQtViewPane<QProceduralMaterialEditorMainWindow>();
    }
    delete this;
#endif
}
