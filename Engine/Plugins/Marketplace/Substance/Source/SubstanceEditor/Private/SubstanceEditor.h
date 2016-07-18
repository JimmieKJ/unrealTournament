//! @copyright Allegorithmic. All rights reserved.

#pragma once

#include "SubstanceEditorPrivatePCH.h"
#include "Toolkits/AssetEditorToolkit.h"

/*-----------------------------------------------------------------------------
   FSubstanceEditor
-----------------------------------------------------------------------------*/

class FSubstanceEditor : public ISubstanceEditor, public FGCObject, public FNotifyHook, public FEditorUndoClient
{
public:
	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;

	/** Destructor */
	virtual ~FSubstanceEditor();

	/** Edits the specified Font object */
	void InitSubstanceEditor(const TSharedPtr< class IToolkitHost >& InitToolkitHost, UObject* ObjectToEdit);

	/** ISubstanceEditor interface */
	virtual USubstanceGraphInstance* GetGraph() const override;
	
	/** IToolkit interface */
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;
	//virtual TSharedPtr<SDockableTab> CreateToolkitTab(FName TabIdentifier, const FString& InitializationPayload) override;

	/** FSerializableObject interface */
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

protected:
	// Begin FEditorUndoClient Interface
	/** Handles any post undo cleanup of the GUI so that we don't have stale data being displayed. */
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;
	// End of FEditorUndoClient

private:
	/** FNotifyHook interface */
	virtual void NotifyPostChange( const FPropertyChangedEvent& PropertyChangedEvent, class FEditPropertyChain* PropertyThatChanged) override;

	/** Creates all internal widgets for the tabs to point at */
	void CreateInternalWidgets();

	/** Builds the toolbar widget for the font editor */
	void ExtendToolbar();
	
	/**	Binds our UI commands to delegates */
	void BindCommands();

	/** Toolbar command methods */
	void OnExportPreset() const;
	void OnImportPreset();
	void OnResetDefaultValues();

	/** Common method for replacing a font page with a new texture */
	bool ImportPage(int32 PageNum, const TCHAR* FileName);

	/**	Spawns the viewport tab */
	TSharedRef<SDockTab> SpawnTab_SubstanceEditor( const FSpawnTabArgs& Args );

	/**	Caches the specified tab for later retrieval */
	void AddToSpawnedToolPanels( const FName& TabIdentifier, const TSharedRef<SDockTab>& SpawnedTab );

	/** Callback when an object is reimported, handles steps needed to keep the editor up-to-date. */
	void OnObjectReimported(UObject* InObject);

private:
	/** The font asset being inspected */
	USubstanceGraphInstance* EditedGraph;

	/** List of open tool panels; used to ensure only one exists at any one time */
	TMap< FName, TWeakPtr<SDockTab> > SpawnedToolPanels;

	/** Viewport */
	TSharedPtr<SSubstanceEditorPanel> SubstanceEditorPanel;

	/**	The tab ids for the substance editor */
	static const FName SubstanceTabId;
};
