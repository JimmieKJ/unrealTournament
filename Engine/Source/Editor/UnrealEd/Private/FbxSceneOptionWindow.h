// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SlateBasics.h"
#include "AssetRegistryModule.h"
#include "Fbx/SFbxSceneTreeView.h"
#include "Fbx/SFbxSceneListView.h"
#include "Factories/FbxSceneImportFactory.h"

class IDetailsView;

class SFbxSceneOptionWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SFbxSceneOptionWindow)
		: _SceneInfo(NULL)
		, _GlobalImportSettings(NULL)
		, _SceneImportOptionsDisplay(NULL)
		, _SceneImportOptionsStaticMeshDisplay(NULL)
		, _SceneMeshOverrideOptions(NULL)
		, _OwnerWindow()
		, _FullPath()
		{}

		SLATE_ARGUMENT( TSharedPtr<FFbxSceneInfo>, SceneInfo )
		SLATE_ARGUMENT( UnFbx::FBXImportOptions*, GlobalImportSettings)
		SLATE_ARGUMENT( class UFbxSceneImportOptions*, SceneImportOptionsDisplay)
		SLATE_ARGUMENT( class UFbxSceneImportOptionsStaticMesh*, SceneImportOptionsStaticMeshDisplay)
		SLATE_ARGUMENT( MeshInfoOverrideOptions*, SceneMeshOverrideOptions)
		SLATE_ARGUMENT( class UFbxSceneImportOptionsSkeletalMesh*, SceneImportOptionsSkeletalMeshDisplay)
		SLATE_ARGUMENT( class UFbxSceneImportOptionsAnimation*, SceneImportOptionsAnimationDisplay)
		SLATE_ARGUMENT( class UFbxSceneImportOptionsMaterial*, SceneImportOptionsMaterialDisplay)
		SLATE_ARGUMENT( TSharedPtr<SWindow>, OwnerWindow)
		SLATE_ARGUMENT( FText, FullPath )
	SLATE_END_ARGS()

public:

	SFbxSceneOptionWindow();
	~SFbxSceneOptionWindow();

	void Construct(const FArguments& InArgs);
	virtual bool SupportsKeyboardFocus() const override { return true; }

	void CloseFbxSceneOption()
	{
		//Free all resource before closing the window
		
		//Unregister spawn tab
		if (FbxSceneImportTabManager.IsValid())
		{
			FbxSceneImportTabManager->UnregisterAllTabSpawners();
			FbxSceneImportTabManager->CloseAllAreas();
		}
		FbxSceneImportTabManager = NULL;
		Layout = NULL;

		//Clear scene tab resource
		SceneTabTreeview = NULL;
		SceneTabDetailsView = NULL;
		
		SceneInfo = NULL;
		SceneImportOptionsDisplay = NULL;
		if (OwnerWindow.IsValid())
		{
			//Close the window
			OwnerWindow->RequestDestroyWindow();
		}
		OwnerWindow = NULL;
	}

	FReply OnImport()
	{
		bShouldImport = true;
		CloseFbxSceneOption();
		return FReply::Handled();
	}

	FReply OnCancel()
	{
		bShouldImport = false;
		CloseFbxSceneOption();
		return FReply::Handled();
	}

	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override
	{
		if( InKeyEvent.GetKey() == EKeys::Escape )
		{
			return OnCancel();
		}

		return FReply::Unhandled();
	}

	bool ShouldImport() const
	{
		return bShouldImport;
	}

	static void CopyFbxOptionsToFbxOptions(UnFbx::FBXImportOptions *SourceOptions, UnFbx::FBXImportOptions *DestinationOptions);

	static void CopyStaticMeshOptionsToFbxOptions(UnFbx::FBXImportOptions *ImportSettings, class UFbxSceneImportOptionsStaticMesh* StaticMeshOptions);
	static void CopyFbxOptionsToStaticMeshOptions(UnFbx::FBXImportOptions *ImportSettings, class UFbxSceneImportOptionsStaticMesh* StaticMeshOptions);

	static void CopySkeletalMeshOptionsToFbxOptions(UnFbx::FBXImportOptions *ImportSettings, class UFbxSceneImportOptionsSkeletalMesh* SkeletalMeshOptions);
	static void CopyFbxOptionsToSkeletalMeshOptions(UnFbx::FBXImportOptions *ImportSettings, class UFbxSceneImportOptionsSkeletalMesh* SkeletalMeshOptions);

	static void CopyAnimationOptionsToFbxOptions(UnFbx::FBXImportOptions *ImportSettings, class UFbxSceneImportOptionsAnimation* AnimationOptions);
	static void CopyFbxOptionsToAnimationOptions(UnFbx::FBXImportOptions *ImportSettings, class UFbxSceneImportOptionsAnimation* AnimationOptions);

	static void CopyMaterialOptionsToFbxOptions(UnFbx::FBXImportOptions *ImportSettings, class UFbxSceneImportOptionsMaterial* MaterialOptions);
	static void CopyFbxOptionsToMaterialOptions(UnFbx::FBXImportOptions *ImportSettings, class UFbxSceneImportOptionsMaterial* MaterialOptions);

private:

	bool CanImport() const;

	void InitAllTabs();
	TSharedPtr<SWidget> SpawnDockTab();
	TSharedRef<SDockTab> SpawnSceneTab(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnStaticMeshTab(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnSkeletalMeshTab(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnMaterialTab(const FSpawnTabArgs& Args);

private:
	//SFbxSceneOptionWindow Arguments
	
	TSharedPtr< FFbxSceneInfo > SceneInfo;
	UnFbx::FBXImportOptions *GlobalImportSettings;
	class UFbxSceneImportOptions* SceneImportOptionsDisplay;
	class UFbxSceneImportOptionsStaticMesh* SceneImportOptionsStaticMeshDisplay;
	MeshInfoOverrideOptions* SceneMeshOverrideOptions;
	class UFbxSceneImportOptionsSkeletalMesh* SceneImportOptionsSkeletalMeshDisplay;
	class UFbxSceneImportOptionsAnimation* SceneImportOptionsAnimationDisplay;
	class UFbxSceneImportOptionsMaterial* SceneImportOptionsMaterialDisplay;
	TSharedPtr< SWindow > OwnerWindow;

	
	//Variables

	TSharedPtr<FTabManager> FbxSceneImportTabManager;
	TSharedPtr<FTabManager::FLayout> Layout;
	bool			bShouldImport;
	
	
	//Scene tab variables
	
	TSharedPtr<SFbxSceneTreeView> SceneTabTreeview;
	TSharedPtr<IDetailsView> SceneTabDetailsView;

	
	//StaticMesh tab Variables

	TSharedPtr<SFbxSceneStaticMeshListView> StaticMeshTabListView;
	TSharedPtr<IDetailsView> StaticMeshTabDetailsView;
};
