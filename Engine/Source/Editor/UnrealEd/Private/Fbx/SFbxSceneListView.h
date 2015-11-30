// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Classes/Factories/FbxSceneImportFactory.h"

class STextComboBox;

typedef TSharedPtr<FFbxMeshInfo> FbxMeshInfoPtr;

class SFbxSceneStaticMeshListView : public SListView<FbxMeshInfoPtr>
{
public:
	
	~SFbxSceneStaticMeshListView();

	SLATE_BEGIN_ARGS(SFbxSceneStaticMeshListView)
	: _SceneInfo(nullptr)
	, _GlobalImportSettings(nullptr)
	, _SceneMeshOverrideOptions(nullptr)
	, _SceneImportOptionsStaticMeshDisplay(nullptr)
	{}
		SLATE_ARGUMENT(TSharedPtr<FFbxSceneInfo>, SceneInfo)
		SLATE_ARGUMENT(UnFbx::FBXImportOptions*, GlobalImportSettings)
		SLATE_ARGUMENT(MeshInfoOverrideOptions*, SceneMeshOverrideOptions)
		SLATE_ARGUMENT(class UFbxSceneImportOptionsStaticMesh*, SceneImportOptionsStaticMeshDisplay)
	SLATE_END_ARGS()
	
	/** Construct this widget */
	void Construct(const FArguments& InArgs);
	TSharedRef< ITableRow > OnGenerateRowFbxSceneListView(FbxMeshInfoPtr Item, const TSharedRef< STableViewBase >& OwnerTable);
	bool CanDeleteOverride()  const;
	FReply OnDeleteOverride();
	FReply OnSelectAssetUsing();
	FReply OnCreateOverrideOptions();
	void OnToggleSelectAll(ECheckBoxState CheckType);
	
	void OnFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent);
	TSharedPtr<STextComboBox> CreateOverrideOptionComboBox();

protected:
	TSharedPtr<FFbxSceneInfo> SceneInfo;
	UnFbx::FBXImportOptions* GlobalImportSettings;
	MeshInfoOverrideOptions *SceneMeshOverrideOptions;

	UnFbx::FBXImportOptions *CurrentStaticMeshImportOptions;


	/** the elements we show in the tree view */
	TArray<FbxMeshInfoPtr> FbxMeshesArray;
	class UFbxSceneImportOptionsStaticMesh *SceneImportOptionsStaticMeshDisplay;

	/** Open a context menu for the current selection */
	TSharedPtr<SWidget> OnOpenContextMenu();
	void AddSelectionToImport();
	void RemoveSelectionFromImport();
	void SetSelectionImportState(bool MarkForImport);
	void OnSelectionChanged(FbxMeshInfoPtr Item, ESelectInfo::Type SelectionType);
	void RefreshOptions();
	void CreateOverrideOptions();
	void ResetToDefaultOptions();

	void OnChangedOverrideOptions(TSharedPtr<FString> ItemSelected, ESelectInfo::Type SelectInfo);
	void AssignToOptions(TSharedPtr<FString> OptionName);
	

	TArray<TSharedPtr<FString>> OverrideNameOptions;
	TMap<TSharedPtr<FString>, UnFbx::FBXImportOptions*> OverrideNameOptionsMap;
	TSharedPtr<STextComboBox> OptionComboBox;
	TSharedPtr<FString> DefaultOptionNamePtr;
};
