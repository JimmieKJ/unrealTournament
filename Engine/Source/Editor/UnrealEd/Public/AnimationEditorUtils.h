// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#ifndef __AnimationEditorUtils_h__
#define __AnimationEditorUtils_h__

#include "Editor/ContentBrowser/Public/ContentBrowserModule.h"

/** dialog to prompt users to decide an animation asset name */
class SCreateAnimationAssetDlg : public SWindow
{
public:
	SLATE_BEGIN_ARGS(SCreateAnimationAssetDlg)
	{
	}
		SLATE_ARGUMENT(FText, DefaultAssetPath)
	SLATE_END_ARGS()

		SCreateAnimationAssetDlg()
		: UserResponse(EAppReturnType::Cancel)
	{
	}

	void Construct(const FArguments& InArgs);

public:
	/** Displays the dialog in a blocking fashion */
	EAppReturnType::Type ShowModal();

	/** Gets the resulting asset path */
	FString GetAssetPath();

	/** Gets the resulting asset name */
	FString GetAssetName();

	/** Gets the resulting full asset path (path+'/'+name) */
	FString GetFullAssetPath();

protected:
	void OnPathChange(const FString& NewPath);
	void OnNameChange(const FText& NewName, ETextCommit::Type CommitInfo);
	FReply OnButtonClick(EAppReturnType::Type ButtonID);

	bool ValidatePackage();

	EAppReturnType::Type UserResponse;
	FText AssetPath;
	FText AssetName;

	static FText LastUsedAssetPath;
};

/** Defines FCanExecuteAction delegate interface.  Returns true when an action is able to execute. */
DECLARE_DELEGATE_OneParam(FAnimAssetCreated, TArray<class UObject*>);

//Animation editor utility functions
namespace AnimationEditorUtils
{
	UNREALED_API void CreateAnimationAssets(const TArray<TWeakObjectPtr<USkeleton>>& Skeletons, TSubclassOf<UAnimationAsset> AssetClass, const FString& InPrefix, FAnimAssetCreated AssetCreated );
	UNREALED_API void CreateNewAnimBlueprint(TArray<TWeakObjectPtr<USkeleton>> Skeletons, FAnimAssetCreated AssetCreated);
	UNREALED_API void FillCreateAssetMenu(FMenuBuilder& MenuBuilder, TArray<TWeakObjectPtr<USkeleton>> Skeletons, FAnimAssetCreated AssetCreated, bool bInContentBrowser=true);
	UNREALED_API void CreateUniqueAssetName(const FString& InBasePackageName, const FString& InSuffix, FString& OutPackageName, FString& OutAssetName);
	UNREALED_API bool ApplyCompressionAlgorithm(TArray<UAnimSequence*>& AnimSequencePtrs, class UAnimCompress* Algorithm);
	
	// The following functions are used to fix subgraph arrays for assets
	UNREALED_API void RegenerateSubGraphArrays(UAnimBlueprint* Blueprint);
	void RegenerateGraphSubGraphs(UAnimBlueprint* OwningBlueprint, UEdGraph* GraphToFix);
	void RemoveDuplicateSubGraphs(UEdGraph* GraphToClean);
	void FindChildGraphsFromNodes(UEdGraph* GraphToSearch, TArray<UEdGraph*>& ChildGraphs);
	UNREALED_API void SetPoseWatch(UPoseWatch* PoseWatch, UAnimBlueprint* AnimBlueprintIfKnown = nullptr);
	UNREALED_API UPoseWatch* FindPoseWatchForNode(const UEdGraphNode* Node, UAnimBlueprint* AnimBlueprintIfKnown=nullptr);
	UNREALED_API void MakePoseWatchForNode(UAnimBlueprint* AnimBlueprint, UEdGraphNode* Node, FColor PoseWatchColour);
	UNREALED_API void RemovePoseWatch(UPoseWatch* PoseWatch, UAnimBlueprint* AnimBlueprintIfKnown=nullptr);
	UNREALED_API void UpdatePoseWatchColour(UPoseWatch* PoseWatch, FColor NewPoseWatchColour);

	//////////////////////////////////////////////////////////////////////////////////////////

	template <typename TFactory, typename T>
	void ExecuteNewAnimAsset(TArray<TWeakObjectPtr<USkeleton>> Objects, const FString InSuffix, FAnimAssetCreated AssetCreated, bool bInContentBrowser)
	{
		if(bInContentBrowser && Objects.Num() == 1)
		{
			auto Object = Objects[0].Get();

			if(Object)
			{
				// Determine an appropriate name for inline-rename
				FString Name;
				FString PackageName;
				CreateUniqueAssetName(Object->GetOutermost()->GetName(), InSuffix, PackageName, Name);

				TFactory* Factory = NewObject<TFactory>();
				Factory->TargetSkeleton = Object;

				FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
				ContentBrowserModule.Get().CreateNewAsset(Name, FPackageName::GetLongPackagePath(PackageName), T::StaticClass(), Factory);

				if(AssetCreated.IsBound())
				{
					// @TODO: this doesn't work
					//FString LongPackagePath = FPackageName::GetLongPackagePath(PackageName);
					UObject* 	Parent = FindPackage(NULL, *PackageName);
					UObject* NewAsset = FindObject<UObject>(Parent, *Name, false);
					if(NewAsset)
					{
						TArray<UObject*> NewAssets;
						NewAssets.Add(NewAsset);
						AssetCreated.Execute(NewAssets);
					}
				}
			}
		}
		else
		{
			CreateAnimationAssets(Objects, T::StaticClass(), InSuffix, AssetCreated);
		}
	}
} // namespace AnimationEditorUtils

#endif //__AnimationEditorUtils_h__
