// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ICurveTableEditor.h"
#include "Toolkits/AssetEditorToolkit.h"

class UCurveTable;

/** Viewer/editor for a CurveTable */
class FCurveTableEditor :
	public ICurveTableEditor
{

public:
	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;

	/**
	 * Edits the specified table
	 *
	 * @param	Mode					Asset editing mode for this editor (standalone or world-centric)
	 * @param	InitToolkitHost			When Mode is WorldCentric, this is the level editor instance to spawn this editor within
	 * @param	Table					The table to edit
	 */
	void InitCurveTableEditor( const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UCurveTable* Table );

	/** Destructor */
	virtual ~FCurveTableEditor();

	/** IToolkit interface */
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;

	/** Creates the grid widget used to display the data */
	TSharedRef<SUniformGridPanel> CreateGridPanel() const;

	/**	Spawns the tab with the curve table inside */
	TSharedRef<SDockTab> SpawnTab_CurveTable( const FSpawnTabArgs& Args );

private:

	/**	The tab id for the curve table tab */
	static const FName CurveTableTabId;
};