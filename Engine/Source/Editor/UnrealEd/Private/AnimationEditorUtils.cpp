// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "AnimationEditorUtils.h"
#include "AssetToolsModule.h"
#include "Animation/AnimComposite.h"
#include "Animation/AnimCompress.h"
#include "Animation/BlendSpace.h"
#include "Animation/BlendSpace1D.h"
#include "Animation/AimOffsetBlendSpace.h"
#include "Animation/AimOffsetBlendSpace1D.h"
#include "Animation/AnimCompress.h"

#include "AnimationGraph.h"
#include "AnimStateNodeBase.h"
#include "AnimStateTransitionNode.h"
#include "AnimGraphNode_StateMachineBase.h"
#include "AnimationStateMachineGraph.h"
#include "K2Node_Composite.h"
#include "AssertionMacros.h"
#include "Engine/PoseWatch.h"
#include "BlueprintEditorUtils.h"

#define LOCTEXT_NAMESPACE "AnimationEditorUtils"

/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Create Animation dialog to determine a newly created asset's name
///////////////////////////////////////////////////////////////////////////////

FText SCreateAnimationAssetDlg::LastUsedAssetPath;

void SCreateAnimationAssetDlg::Construct(const FArguments& InArgs)
{
	AssetPath = FText::FromString(FPackageName::GetLongPackagePath(InArgs._DefaultAssetPath.ToString()));
	AssetName = FText::FromString(FPackageName::GetLongPackageAssetName(InArgs._DefaultAssetPath.ToString()));

	if (AssetPath.IsEmpty())
	{
		AssetPath = LastUsedAssetPath;
	}
	else
	{
		LastUsedAssetPath = AssetPath;
	}

	FPathPickerConfig PathPickerConfig;
	PathPickerConfig.DefaultPath = AssetPath.ToString();
	PathPickerConfig.OnPathSelected = FOnPathSelected::CreateSP(this, &SCreateAnimationAssetDlg::OnPathChange);
	PathPickerConfig.bAddDefaultPath = true;

	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

	SWindow::Construct(SWindow::FArguments()
		.Title(LOCTEXT("SCreateAnimationAssetDlg_Title", "Create a New Animation Asset"))
		.SupportsMinimize(false)
		.SupportsMaximize(false)
		//.SizingRule( ESizingRule::Autosized )
		.ClientSize(FVector2D(450, 450))
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot() // Add user input block
			.Padding(2)
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("SelectPath", "Select Path to create animation"))
						.Font(FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 14))
					]

					+ SVerticalBox::Slot()
						.FillHeight(1)
						.Padding(3)
						[
							ContentBrowserModule.Get().CreatePathPicker(PathPickerConfig)
						]

					+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(SSeparator)
						]

					+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(3)
						[
							SNew(SHorizontalBox)

							+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(0, 0, 10, 0)
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("AnimationName", "Animation Name"))
							]

							+ SHorizontalBox::Slot()
								[
									SNew(SEditableTextBox)
									.Text(AssetName)
									.OnTextCommitted(this, &SCreateAnimationAssetDlg::OnNameChange)
									.MinDesiredWidth(250)
								]
						]

				]
			]

			+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Right)
				.Padding(5)
				[
					SNew(SUniformGridPanel)
					.SlotPadding(FEditorStyle::GetMargin("StandardDialog.SlotPadding"))
					.MinDesiredSlotWidth(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
					.MinDesiredSlotHeight(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
					+ SUniformGridPanel::Slot(0, 0)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
						.Text(LOCTEXT("OK", "OK"))
						.OnClicked(this, &SCreateAnimationAssetDlg::OnButtonClick, EAppReturnType::Ok)
					]
					+ SUniformGridPanel::Slot(1, 0)
						[
							SNew(SButton)
							.HAlign(HAlign_Center)
							.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
							.Text(LOCTEXT("Cancel", "Cancel"))
							.OnClicked(this, &SCreateAnimationAssetDlg::OnButtonClick, EAppReturnType::Cancel)
						]
				]
		]);
}

void SCreateAnimationAssetDlg::OnNameChange(const FText& NewName, ETextCommit::Type CommitInfo)
{
	AssetName = NewName;
}

void SCreateAnimationAssetDlg::OnPathChange(const FString& NewPath)
{
	AssetPath = FText::FromString(NewPath);
	LastUsedAssetPath = AssetPath;
}

FReply SCreateAnimationAssetDlg::OnButtonClick(EAppReturnType::Type ButtonID)
{
	UserResponse = ButtonID;

	if (ButtonID != EAppReturnType::Cancel)
	{
		if (!ValidatePackage())
		{
			// reject the request
			return FReply::Handled();
		}
	}

	RequestDestroyWindow();

	return FReply::Handled();
}

/** Ensures supplied package name information is valid */
bool SCreateAnimationAssetDlg::ValidatePackage()
{
	FText Reason;
	FString FullPath = GetFullAssetPath();

	if (!FPackageName::IsValidLongPackageName(FullPath, false, &Reason)
		|| !FName(*AssetName.ToString()).IsValidObjectName(Reason))
	{
		FMessageDialog::Open(EAppMsgType::Ok, Reason);
		return false;
	}

	return true;
}

EAppReturnType::Type SCreateAnimationAssetDlg::ShowModal()
{
	GEditor->EditorAddModalWindow(SharedThis(this));
	return UserResponse;
}

FString SCreateAnimationAssetDlg::GetAssetPath()
{
	return AssetPath.ToString();
}

FString SCreateAnimationAssetDlg::GetAssetName()
{
	return AssetName.ToString();
}

FString SCreateAnimationAssetDlg::GetFullAssetPath()
{
	return AssetPath.ToString() + "/" + AssetName.ToString();
}

/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////

/////////////////////////////////////////////////////
// Animation editor utility functions
/////////////////////////////////////////////////////

namespace AnimationEditorUtils
{
	/** Creates a unique package and asset name taking the form InBasePackageName+InSuffix */
	void CreateUniqueAssetName(const FString& InBasePackageName, const FString& InSuffix, FString& OutPackageName, FString& OutAssetName) 
	{
		FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");
		AssetToolsModule.Get().CreateUniqueAssetName(InBasePackageName, InSuffix, OutPackageName, OutAssetName);
	}

	void CreateAnimationAssets(const TArray<TWeakObjectPtr<USkeleton>>& Skeletons, TSubclassOf<UAnimationAsset> AssetClass, const FString& InPrefix, FAnimAssetCreated AssetCreated )
	{
		TArray<UObject*> ObjectsToSync;
		for(auto SkelIt = Skeletons.CreateConstIterator(); SkelIt; ++SkelIt)
		{
			USkeleton* Skeleton = (*SkelIt).Get();
			if(Skeleton)
			{
				FString Name;
				FString PackageName;
				FString AssetPath = Skeleton->GetOutermost()->GetName();
				// Determine an appropriate name
				CreateUniqueAssetName(AssetPath, InPrefix, PackageName, Name);

				// set the unique asset as a default name
				TSharedRef<SCreateAnimationAssetDlg> NewAnimDlg =
					SNew(SCreateAnimationAssetDlg)
					.DefaultAssetPath(FText::FromString(PackageName));

				// show a dialog to determine a new asset name
				if (NewAnimDlg->ShowModal() == EAppReturnType::Cancel)
				{
					return;
				}

				PackageName = NewAnimDlg->GetFullAssetPath();
				Name = NewAnimDlg->GetAssetName();

				// Create the asset, and assign its skeleton
				FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
				UAnimationAsset* NewAsset = Cast<UAnimationAsset>(AssetToolsModule.Get().CreateAsset(Name, FPackageName::GetLongPackagePath(PackageName), AssetClass, NULL));

				if(NewAsset)
				{
					NewAsset->SetSkeleton(Skeleton);
					NewAsset->MarkPackageDirty();

					ObjectsToSync.Add(NewAsset);
				}
			}
		}

		if (AssetCreated.IsBound())
		{
			AssetCreated.Execute(ObjectsToSync);
		}
	}

	void CreateNewAnimBlueprint(TArray<TWeakObjectPtr<USkeleton>> Skeletons, FAnimAssetCreated AssetCreated)
	{
		const FString DefaultSuffix = TEXT("_AnimBlueprint");

		if (Skeletons.Num() == 1)
		{
			auto Object = Skeletons[0].Get();

			if (Object)
			{
				// Determine an appropriate name for inline-rename
				FString Name;
				FString PackageName;
				CreateUniqueAssetName(Object->GetOutermost()->GetName(), DefaultSuffix, PackageName, Name);

				UAnimBlueprintFactory* Factory = NewObject<UAnimBlueprintFactory>();
				Factory->TargetSkeleton = Object;

				FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
				ContentBrowserModule.Get().CreateNewAsset(Name, FPackageName::GetLongPackagePath(PackageName), UAnimBlueprint::StaticClass(), Factory);
			}
		}
		else
		{
			TArray<UObject*> SkeletonsToSync;
			for (auto ObjIt = Skeletons.CreateConstIterator(); ObjIt; ++ObjIt)
			{
				auto Object = (*ObjIt).Get();
				if (Object)
				{
					// Determine an appropriate name
					FString Name;
					FString PackageName;
					CreateUniqueAssetName(Object->GetOutermost()->GetName(), DefaultSuffix, PackageName, Name);

					// Create the anim blueprint factory used to generate the asset
					UAnimBlueprintFactory* Factory = NewObject<UAnimBlueprintFactory>();
					Factory->TargetSkeleton = Object;

					FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
					UObject* NewAsset = AssetToolsModule.Get().CreateAsset(Name, FPackageName::GetLongPackagePath(PackageName), UAnimBlueprint::StaticClass(), Factory);

					if (NewAsset)
					{
						SkeletonsToSync.Add(NewAsset);
					}
				}
			}

			if (AssetCreated.IsBound())
			{
				AssetCreated.Execute(SkeletonsToSync);
			}
		}
	}

	void FillCreateAssetMenu(FMenuBuilder& MenuBuilder, TArray<TWeakObjectPtr<USkeleton>> Skeletons, FAnimAssetCreated AssetCreated, bool bInContentBrowser) 
	{
		MenuBuilder.BeginSection("CreateAnimAssets", LOCTEXT("CreateAnimAssetsMenuHeading", "Anim Assets"));
		{
			// only allow for content browser until we support multi assets so we can open new persona with this BP
			if (bInContentBrowser)
			{
				MenuBuilder.AddMenuEntry(
					LOCTEXT("Skeleton_NewAnimBlueprint", "Anim Blueprint"),
					LOCTEXT("Skeleton_NewAnimBlueprintTooltip", "Creates an Anim Blueprint using the selected skeleton."),
					FSlateIcon(FEditorStyle::GetStyleSetName(), "ClassIcon.AnimBlueprint"),
					FUIAction(
						FExecuteAction::CreateStatic(&CreateNewAnimBlueprint, Skeletons, AssetCreated),
						FCanExecuteAction()
						)
					);
			}

			MenuBuilder.AddMenuEntry(
				LOCTEXT("Skeleton_NewAnimComposite", "Anim Composite"),
				LOCTEXT("Skeleton_NewAnimCompositeTooltip", "Creates an AnimComposite using the selected skeleton."),
				FSlateIcon(FEditorStyle::GetStyleSetName(), "ClassIcon.AnimComposite"),
				FUIAction(
					FExecuteAction::CreateStatic(&ExecuteNewAnimAsset<UAnimCompositeFactory, UAnimComposite>, Skeletons, FString("_Composite"), AssetCreated, bInContentBrowser),
					FCanExecuteAction()
					)
				);

			MenuBuilder.AddMenuEntry(
				LOCTEXT("Skeleton_NewAnimMontage", "Anim Montage"),
				LOCTEXT("Skeleton_NewAnimMontageTooltip", "Creates an AnimMontage using the selected skeleton."),
				FSlateIcon(FEditorStyle::GetStyleSetName(), "ClassIcon.AnimMontage"),
				FUIAction(
					FExecuteAction::CreateStatic(&ExecuteNewAnimAsset<UAnimMontageFactory, UAnimMontage>, Skeletons, FString("_Montage"), AssetCreated, bInContentBrowser),
					FCanExecuteAction()
					)
				);
		}
		MenuBuilder.EndSection();

		MenuBuilder.BeginSection("CreateBlendSpace", LOCTEXT("CreateBlendSpaceMenuHeading", "Blend Spaces"));
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("SkeletalMesh_New2DBlendspace", "Blend Space"),
				LOCTEXT("SkeletalMesh_New2DBlendspaceTooltip", "Creates a Blend Space using the selected skeleton."),
				FSlateIcon(FEditorStyle::GetStyleSetName(), "ClassIcon.BlendSpace"),
				FUIAction(
					FExecuteAction::CreateStatic(&ExecuteNewAnimAsset<UBlendSpaceFactoryNew, UBlendSpace>, Skeletons, FString("_BlendSpace"), AssetCreated, bInContentBrowser),
					FCanExecuteAction()
					)
				);

			MenuBuilder.AddMenuEntry(
				LOCTEXT("SkeletalMesh_New1DBlendspace", "Blend Space 1D"),
				LOCTEXT("SkeletalMesh_New1DBlendspaceTooltip", "Creates a 1D Blend Space using the selected skeleton."),
				FSlateIcon(FEditorStyle::GetStyleSetName(), "ClassIcon.BlendSpace1D"),
				FUIAction(
					FExecuteAction::CreateStatic(&ExecuteNewAnimAsset<UBlendSpaceFactory1D, UBlendSpace1D>, Skeletons, FString("_BlendSpace1D"), AssetCreated, bInContentBrowser),
					FCanExecuteAction()
					)
				);
		}
		MenuBuilder.EndSection();

		MenuBuilder.BeginSection("CreateAimOffset", LOCTEXT("CreateAimOffsetMenuHeading", "Aim Offsets"));
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("SkeletalMesh_New2DAimOffset", "Aim Offset"),
				LOCTEXT("SkeletalMesh_New2DAimOffsetTooltip", "Creates a Aim Offset blendspace using the selected skeleton."),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateStatic(&ExecuteNewAnimAsset<UAimOffsetBlendSpaceFactoryNew, UAimOffsetBlendSpace>, Skeletons, FString("_AimOffset2D"), AssetCreated, bInContentBrowser),
					FCanExecuteAction()
					)
				);

			MenuBuilder.AddMenuEntry(
				LOCTEXT("SkeletalMesh_New1DAimOffset", "Aim Offset 1D"),
				LOCTEXT("SkeletalMesh_New1DAimOffsetTooltip", "Creates a 1D Aim Offset blendspace using the selected skeleton."),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateStatic(&ExecuteNewAnimAsset<UAimOffsetBlendSpaceFactory1D, UAimOffsetBlendSpace1D>, Skeletons, FString("_AimOffset1D"), AssetCreated, bInContentBrowser),
					FCanExecuteAction()
					)
				);
		}
		MenuBuilder.EndSection();
	}

	bool ApplyCompressionAlgorithm(TArray<UAnimSequence*>& AnimSequencePtrs, class UAnimCompress* Algorithm)
	{
		if(Algorithm)
		{
			const bool bProceed = (AnimSequencePtrs.Num() > 1)? EAppReturnType::Yes == FMessageDialog::Open(EAppMsgType::YesNo,
				FText::Format(NSLOCTEXT("UnrealEd", "AboutToCompressAnimations_F", "About to compress {0} animations.  Proceed?"), FText::AsNumber(AnimSequencePtrs.Num()))) : true;
			if(bProceed)
			{
				GWarn->BeginSlowTask(LOCTEXT("AnimCompressing", "Compressing"), true);

				{
					TSharedPtr<FAnimCompressContext> CompressContext = MakeShareable(new FAnimCompressContext(false, true, AnimSequencePtrs.Num()));

					for (UAnimSequence* AnimSeq : AnimSequencePtrs)
					{
						AnimSeq->CompressionScheme = static_cast<UAnimCompress*>(StaticDuplicateObject(Algorithm, AnimSeq));
						AnimSeq->RequestAnimCompression(false, CompressContext);
						++CompressContext->AnimIndex;
					}
				}
				
				GWarn->EndSlowTask();

				return true;
			}
		}

		return false;
	}

	void RegenerateSubGraphArrays(UAnimBlueprint* Blueprint)
	{
		// The anim graph should be the first function graph on the blueprint
		if(Blueprint->FunctionGraphs.Num() > 0)
		{
			if(UAnimationGraph* AnimGraph = Cast<UAnimationGraph>(Blueprint->FunctionGraphs[0]))
			{
				RegenerateGraphSubGraphs(Blueprint, AnimGraph);
			}
		}
	}

	void RegenerateGraphSubGraphs(UAnimBlueprint* OwningBlueprint, UEdGraph* GraphToFix)
	{
		TArray<UEdGraph*> ChildGraphs;
		FindChildGraphsFromNodes(GraphToFix, ChildGraphs);

		for(UEdGraph* Child : ChildGraphs)
		{
			RegenerateGraphSubGraphs(OwningBlueprint, Child);
		}

		if(ChildGraphs != GraphToFix->SubGraphs)
		{
			UE_LOG(LogAnimation, Log, TEXT("Fixed missing or duplicated graph entries in SubGraph array for graph %s in AnimBP %s"), *GraphToFix->GetName(), *OwningBlueprint->GetName());
			GraphToFix->SubGraphs = ChildGraphs;
		}
	}

	void RemoveDuplicateSubGraphs(UEdGraph* GraphToClean)
	{
		TArray<UEdGraph*> NewSubGraphArray;

		for(UEdGraph* SubGraph : GraphToClean->SubGraphs)
		{
			NewSubGraphArray.AddUnique(SubGraph);
		}

		if(NewSubGraphArray.Num() != GraphToClean->SubGraphs.Num())
		{
			GraphToClean->SubGraphs = NewSubGraphArray;
		}
	}

	void FindChildGraphsFromNodes(UEdGraph* GraphToSearch, TArray<UEdGraph*>& ChildGraphs)
	{
		for(UEdGraphNode* CurrentNode : GraphToSearch->Nodes)
		{
			if(UAnimGraphNode_StateMachineBase* StateMachine = Cast<UAnimGraphNode_StateMachineBase>(CurrentNode))
			{
				ChildGraphs.AddUnique(StateMachine->EditorStateMachineGraph);
			}
			else if(UAnimStateNodeBase* StateNode = Cast<UAnimStateNodeBase>(CurrentNode))
			{
				ChildGraphs.AddUnique(StateNode->GetBoundGraph());

				if(UAnimStateTransitionNode* TransitionNode = Cast<UAnimStateTransitionNode>(StateNode))
				{
					if(TransitionNode->CustomTransitionGraph)
					{
						ChildGraphs.AddUnique(TransitionNode->CustomTransitionGraph);
					}
				}
			}
			else if(UK2Node_Composite* CompositeNode = Cast<UK2Node_Composite>(CurrentNode))
			{
				ChildGraphs.AddUnique(CompositeNode->BoundGraph);
			}
		}
	}

	void SetPoseWatch(UPoseWatch* PoseWatch, UAnimBlueprint* AnimBlueprintIfKnown)
	{
#if WITH_EDITORONLY_DATA
		if (UAnimGraphNode_Base* TargetNode = Cast<UAnimGraphNode_Base>(PoseWatch->Node))
		{
			UAnimBlueprint* AnimBlueprint = AnimBlueprintIfKnown ? AnimBlueprintIfKnown : Cast<UAnimBlueprint>(FBlueprintEditorUtils::FindBlueprintForNode(TargetNode));

			if ((AnimBlueprint != NULL) && (AnimBlueprint->GeneratedClass != NULL))
			{
				if (UAnimBlueprintGeneratedClass* AnimBPGenClass = Cast<UAnimBlueprintGeneratedClass>(*AnimBlueprint->GeneratedClass))
				{
					// Find the insertion point from the debugging data
					int32 LinkID = AnimBPGenClass->GetLinkIDForNode<FAnimNode_Base>(TargetNode);
					AnimBPGenClass->GetAnimBlueprintDebugData().AddPoseWatch(LinkID, PoseWatch->PoseWatchColour);
				}
			}
		}
#endif	//#if WITH_EDITORONLY_DATA
	}

	UPoseWatch* FindPoseWatchForNode(const UEdGraphNode* Node, UAnimBlueprint* AnimBlueprintIfKnown)
	{
#if WITH_EDITORONLY_DATA
		UAnimBlueprint* AnimBlueprint = AnimBlueprintIfKnown ? AnimBlueprintIfKnown : Cast<UAnimBlueprint>(FBlueprintEditorUtils::FindBlueprintForNode(Node));

		if(AnimBlueprint)
		{
			// iterate backwards so we can remove invalid pose watches as we go
			for (int32 Index = AnimBlueprint->PoseWatches.Num() - 1; Index >= 0; --Index)
			{
				UPoseWatch* PoseWatch = AnimBlueprint->PoseWatches[Index];
				if (PoseWatch == nullptr || PoseWatch->Node == nullptr)
				{
					AnimBlueprint->PoseWatches.RemoveAtSwap(Index);
					continue;
				}

				// Return this pose watch if the node location matches the given node
				if (PoseWatch->Node == Node)
				{
					return PoseWatch;
				}
			}
		}

		return nullptr;
#endif
	}

	void MakePoseWatchForNode(UAnimBlueprint* AnimBlueprint, UEdGraphNode* Node, FColor PoseWatchColour)
	{
#if WITH_EDITORONLY_DATA
		UPoseWatch* NewPoseWatch = NewObject<UPoseWatch>(AnimBlueprint);
		NewPoseWatch->Node = Node;
		NewPoseWatch->PoseWatchColour = PoseWatchColour;
		AnimBlueprint->PoseWatches.Add(NewPoseWatch);

		SetPoseWatch(NewPoseWatch, AnimBlueprint);
#endif
	}

	void RemovePoseWatch(UPoseWatch* PoseWatch, UAnimBlueprint* AnimBlueprintIfKnown)
	{
#if WITH_EDITORONLY_DATA
		if (UAnimGraphNode_Base* TargetNode = Cast<UAnimGraphNode_Base>(PoseWatch->Node))
		{
			UAnimBlueprint* AnimBlueprint = AnimBlueprintIfKnown ? AnimBlueprintIfKnown : Cast<UAnimBlueprint>(FBlueprintEditorUtils::FindBlueprintForNode(TargetNode));

			if (AnimBlueprint)
			{
				AnimBlueprint->PoseWatches.Remove(PoseWatch);

				if (UAnimBlueprintGeneratedClass* AnimBPGenClass = AnimBlueprint->GetAnimBlueprintGeneratedClass())
				{
					int32 LinkID = AnimBPGenClass->GetLinkIDForNode<FAnimNode_Base>(Cast<UAnimGraphNode_Base>(PoseWatch->Node));
					AnimBPGenClass->GetAnimBlueprintDebugData().RemovePoseWatch(LinkID);
				}
			}
		}
#endif
	}

	void UpdatePoseWatchColour(UPoseWatch* PoseWatch, FColor NewPoseWatchColour)
	{
#if WITH_EDITORONLY_DATA
		PoseWatch->PoseWatchColour = NewPoseWatchColour;

		if (UAnimGraphNode_Base* TargetNode = Cast<UAnimGraphNode_Base>(PoseWatch->Node))
		{
			UAnimBlueprint* AnimBlueprint = Cast<UAnimBlueprint>(FBlueprintEditorUtils::FindBlueprintForNode(TargetNode));

			if ((AnimBlueprint != NULL) && (AnimBlueprint->GeneratedClass != NULL))
			{
				if (UAnimBlueprintGeneratedClass* AnimBPGenClass = Cast<UAnimBlueprintGeneratedClass>(*AnimBlueprint->GeneratedClass))
				{
					// Find the insertion point from the debugging data
					int32 LinkID = AnimBPGenClass->GetLinkIDForNode<FAnimNode_Base>(TargetNode);

					AnimBPGenClass->GetAnimBlueprintDebugData().UpdatePoseWatchColour(LinkID, NewPoseWatchColour);
				}
			}
		}
#endif
	}
}

#undef LOCTEXT_NAMESPACE