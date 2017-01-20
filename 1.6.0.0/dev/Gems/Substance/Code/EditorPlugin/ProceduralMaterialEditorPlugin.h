/** @file ProceduralMaterialEditorPlugin.h
	@brief Library hook to register plugin features
	@author Josh Coyne - Allegorithmic (josh.coyne@allegorithmic.com)
	@date 09-14-2015
	@copyright Allegorithmic. All rights reserved.
*/
#include <IEditor.h>
#include <Include/IPlugin.h>

//------------------------------------------------------------------
class ProceduralMaterialEditorPlugin : public IPlugin
{
public:
    ProceduralMaterialEditorPlugin(IEditor* editor);

    void Release() override;
    void ShowAbout() override {}
    const char* GetPluginGUID() override { return "{F255F1DA-64EC-4B72-8A04-236F99A6FF20}"; }
    DWORD GetPluginVersion() override { return 1; }
    const char* GetPluginName() override { return "ProceduralMaterialEditor"; }
    bool CanExitNow() override { return true; }
    void OnEditorNotify(EEditorNotifyEvent aEventId) override {}

private:
    bool m_registered;
};
