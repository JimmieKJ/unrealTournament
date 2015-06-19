// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "IMergeActorsTool.h"
#include "MeshUtilities.h"

/**
 * Mesh Merging Tool
 */
class FMeshMergingTool : public IMergeActorsTool
{
	friend class SMeshMergingDialog;

public:

	FMeshMergingTool();

	// IMergeActorsTool interface
	virtual TSharedRef<SWidget> GetWidget() override;
	virtual FName GetIconName() const override { return "MergeActors.MeshMergingTool"; }
	virtual FText GetTooltipText() const override;
	virtual FString GetDefaultPackageName() const override;
	virtual bool RunMerge(const FString& PackageName) override;

private:

	/** Current mesh merging settings */
	FMeshMergingSettings MergingSettings;

	/** Whether to spawn merged actor in the world */
	bool bPlaceInWorld;

	bool bExportSpecificLOD;
	int32 ExportLODIndex;
};
