// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Classes/Factories/FbxSceneImportFactory.h"
#include "SSceneBaseMeshListView.h"

class SFbxSceneSkeletalMeshListView : public SFbxSSceneBaseMeshListView
{
public:
	
	~SFbxSceneSkeletalMeshListView();

	SLATE_BEGIN_ARGS(SFbxSceneSkeletalMeshListView)
	: _SceneInfo(nullptr)
	, _GlobalImportSettings(nullptr)
	, _OverrideNameOptionsMap(nullptr)
	, _SceneImportOptionsSkeletalMeshDisplay(nullptr)
	{}
		SLATE_ARGUMENT(TSharedPtr<FFbxSceneInfo>, SceneInfo)
		SLATE_ARGUMENT(UnFbx::FBXImportOptions*, GlobalImportSettings)
		SLATE_ARGUMENT(FbxOverrideNameOptionsArrayPtr, OverrideNameOptions)
		SLATE_ARGUMENT(ImportOptionsNameMapPtr, OverrideNameOptionsMap)
		SLATE_ARGUMENT(class UFbxSceneImportOptionsSkeletalMesh*, SceneImportOptionsSkeletalMeshDisplay)
	SLATE_END_ARGS()
	
		/** Construct this widget */
	void Construct(const FArguments& InArgs);
	TSharedRef< ITableRow > OnGenerateRowFbxSceneListView(FbxMeshInfoPtr Item, const TSharedRef< STableViewBase >& OwnerTable);
	
	void OnFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent);
protected:
	
	class UFbxSceneImportOptionsSkeletalMesh *SceneImportOptionsSkeletalMeshDisplay;

	virtual void OnChangedOverrideOptions(TSharedPtr<FString> ItemSelected, ESelectInfo::Type SelectInfo);
	/** Open a context menu for the current selection */
	TSharedPtr<SWidget> OnOpenContextMenu();
};
