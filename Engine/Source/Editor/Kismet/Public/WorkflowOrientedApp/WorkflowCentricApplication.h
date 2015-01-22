// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Toolkits/AssetEditorToolkit.h"
#include "Toolkits/AssetEditorManager.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "WorkflowOrientedApp/WorkflowTabFactory.h"
#include "WorkflowOrientedApp/WorkflowTabManager.h"
#include "WorkflowOrientedApp/WorkflowUObjectDocuments.h"
#include "WorkflowOrientedApp/ApplicationMode.h"

// Delegate for mutating a mode
DECLARE_DELEGATE_RetVal_TwoParams(TSharedRef<FApplicationMode>, FWorkflowApplicationModeExtender, const FName /*ModeName*/, TSharedRef<FApplicationMode> /*InMode*/);

/////////////////////////////////////////////////////
// FWorkflowCentricApplication

class KISMET_API FWorkflowCentricApplication : public FAssetEditorToolkit
{
public:
	// IToolkit interface
	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;
	// End of IToolkit interface
	
	// FAssetEditorToolkit interface
	virtual bool OnRequestClose() override;
	// End of FAssetEditorToolkit interface

	// Returns the current mode of this application
	FName GetCurrentMode() const;
	TSharedPtr<FApplicationMode> GetCurrentModePtr() const { return CurrentAppModePtr; }
	bool IsModeCurrent(FName ModeToCheck) const {return GetCurrentMode() == ModeToCheck;}

	// Attempt to set the current mode.  If this mode is illegal or unknown, the mode will remain unchanged.
	virtual void SetCurrentMode(FName NewMode);

	void PushTabFactories(FWorkflowAllowedTabSet& FactorySetToPush);

	// Gets the mode extender list for all workflow applications (append to customize a specific mode)
	static TArray<FWorkflowApplicationModeExtender>& GetModeExtenderList() { return ModeExtenderList; }
protected:
	TSharedRef<SDockTab> CreatePanelTab(const FSpawnTabArgs& Args, TSharedPtr<FWorkflowTabFactory> TabFactory);

	virtual void AddApplicationMode(FName ModeName, TSharedRef<FApplicationMode> Mode);
protected:
	TSharedPtr<FApplicationMode> CurrentAppModePtr;

private:
	// List of modes; do not access directly, use AddApplicationMode and SetCurrentMode
	TMap< FName, TSharedPtr<FApplicationMode> > ApplicationModeList;

	static TArray<FWorkflowApplicationModeExtender> ModeExtenderList;
};


