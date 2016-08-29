// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "IMergeActorsTool.h"
#include "MeshUtilities.h"

#include "MeshMergingTool.generated.h"

class SMeshMergingDialog;

/** Singleton wrapper to allow for using the setting structure in SSettingsView */
UCLASS(config = Engine)
class UMeshMergingSettingsObject : public UObject
{
	GENERATED_BODY()
public:
	UMeshMergingSettingsObject()		
	{
		Settings.bMergePhysicsData = true;
	}

	static UMeshMergingSettingsObject* Get()
	{
		static bool bInitialized = false;
		// This is a singleton, use default object
		UMeshMergingSettingsObject* DefaultSettings = GetMutableDefault<UMeshMergingSettingsObject>();

		if (!bInitialized)
		{
			bInitialized = true;
		}

		return DefaultSettings;
	}

public:
	UPROPERTY(editanywhere, meta = (ShowOnlyInnerProperties), Category = MergeSettings)
	FMeshMergingSettings Settings;
};

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
	virtual bool CanMerge() const override;
	virtual bool RunMerge(const FString& PackageName) override;	
private:
	/** Whether to replace source actors with a merged actor in the world */
	bool bReplaceSourceActors;

	/** Pointer to the mesh merging dialog containing settings for the merge */
	TSharedPtr<SMeshMergingDialog> MergingDialog;

	/** Pointer to singleton settings object */
	UMeshMergingSettingsObject* SettingsObject;
};
