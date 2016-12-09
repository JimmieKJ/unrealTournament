// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#include "SRetargetManager.h"
#include "Misc/MessageDialog.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "EditorStyleSet.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Input/SButton.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "Widgets/SToolTip.h"
#include "IDocumentation.h"
#include "ScopedTransaction.h"
#include "SRetargetSourceWindow.h"
#include "SRigWindow.h"
#include "AnimPreviewInstance.h"
#include "IEditableSkeleton.h"

#define LOCTEXT_NAMESPACE "SRetargetManager"


//////////////////////////////////////////////////////////////////////////
// SRetargetManager

void SRetargetManager::Construct(const FArguments& InArgs, const TSharedRef<IEditableSkeleton>& InEditableSkeleton, const TSharedRef<IPersonaPreviewScene>& InPreviewScene, FSimpleMulticastDelegate& InOnPostUndo)
{
	EditableSkeletonPtr = InEditableSkeleton;
	PreviewScenePtr = InPreviewScene;
	InOnPostUndo.Add(FSimpleDelegate::CreateSP(this, &SRetargetManager::PostUndo));

	const FString DocLink = TEXT("Shared/Editors/Persona");
	ChildSlot
	[
		SNew (SVerticalBox)

		+ SVerticalBox::Slot()
		.Padding(5, 5)
		.AutoHeight()
		[
			// explain this is retarget source window
			// and what it is
			SNew(STextBlock)
			.TextStyle( FEditorStyle::Get(), "Persona.RetargetManager.ImportantText" )
			.Text(LOCTEXT("RetargetSource_Title", "Manage Retarget Source"))
		]

		+ SVerticalBox::Slot()
		.Padding(5, 5)
		.AutoHeight()
		[
			// explainint this is retarget source window
			// and what it is
			SNew(STextBlock)
			.AutoWrapText(true)
			.ToolTip(IDocumentation::Get()->CreateToolTip(LOCTEXT("RetargetSource_Tooltip", "Add/Delete/Rename Retarget Sources."),
																			NULL,
																			DocLink,
																			TEXT("RetargetSource")))
			.Font(FEditorStyle::GetFontStyle(TEXT("Persona.RetargetManager.FilterFont")))
			.Text(LOCTEXT("RetargetSource_Description", "You can add/rename/delete Retarget Sources. When you have different proportional meshes per skeleton, you can use this setting to indicate if this animation is from a different source." \
														"For example, if your default skeleton is from a small guy, and if you have an animation for a big guy, you can create a Retarget Source from the big guy and set it for the animation." \
														"The Retargeting system will use this information when extracting animation. "))
		]

		+ SVerticalBox::Slot()
		.Padding(2, 5)
		.FillHeight(0.5)
		[
			// construct retarget source window
			SNew(SRetargetSourceWindow, InEditableSkeleton, InOnPostUndo)
		]

		+SVerticalBox::Slot()
		.Padding(5, 5)
		.AutoHeight()
		[
			SNew(SSeparator)
			.Orientation(Orient_Horizontal)
		]

		+ SVerticalBox::Slot()
		.Padding(5, 5)
		.AutoHeight()
		[
			// explainint this is retarget source window
			// and what it is
			SNew(STextBlock)
			.TextStyle(FEditorStyle::Get(), "Persona.RetargetManager.ImportantText")
			.Text(LOCTEXT("RigTemplate_Title", "Set up Rig"))
		]

		+ SVerticalBox::Slot()
		.Padding(5, 5)
		.AutoHeight()
		[
			// explainint this is retarget source window
			// and what it is
			SNew(STextBlock)
			.AutoWrapText(true)
			.ToolTip(IDocumentation::Get()->CreateToolTip(LOCTEXT("RigSetup_Tooltip", "Set up Rig for retargeting between skeletons."),
																			NULL,
																			DocLink,
																			TEXT("RigSetup")))
			.Font(FEditorStyle::GetFontStyle(TEXT("Persona.RetargetManager.FilterFont")))
			.Text(LOCTEXT("RigTemplate_Description", "You can set up a Rig for this skeleton, then when you retarget the animation to a different skeleton with the same Rig, it will use the information to convert data. "))
		]

		+ SVerticalBox::Slot()
		.FillHeight(1)
		.Padding(2, 5)
		[
			// construct rig manager window
			SNew(SRigWindow, InEditableSkeleton, InOnPostUndo)
		]

		+SVerticalBox::Slot()
		.Padding(2, 5)
		.AutoHeight()
		[
			SNew(SSeparator)
			.Orientation(Orient_Horizontal)
		]

		+ SVerticalBox::Slot()
		.Padding(5, 5)
		.AutoHeight()
		[
			// explainint this is retarget source window
			// and what it is
			SNew(STextBlock)
			.TextStyle(FEditorStyle::Get(), "Persona.RetargetManager.ImportantText")
			.Text(LOCTEXT("BasePose_Title", "Manage Retarget Base Pose"))
		]
		// construct base pose options
		+SVerticalBox::Slot()
		.Padding(2, 5)
		.AutoHeight()
		[
			// explainint this is retarget source window
			// and what it is
			SNew(STextBlock)
			.AutoWrapText(true)
			.ToolTip(IDocumentation::Get()->CreateToolTip(LOCTEXT("RetargetBasePose_Tooltip", "Set up base pose for retargeting."),
																			NULL,
																			DocLink,
																			TEXT("SetupBasePose")))
			.Font(FEditorStyle::GetFontStyle(TEXT("Persona.RetargetManager.FilterFont")))
			.Text(LOCTEXT("BasePose_Description", "This information is used when retargeting assets to a different skeleton. You need to make sure the ref pose of both meshes is the same when retargeting, so you can see the pose and" \
											" edit using the bone transform widget, and click the Save button below. "))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()		// This is required to make the scrollbar work, as content overflows Slate containers by default
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Right)
		.Padding(2, 5)
		[
			// two button 1. view 2. save to base pose
			SNew(SHorizontalBox)

			+SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Center)
			[
				SNew(SButton)
				.OnClicked(FOnClicked::CreateSP(this, &SRetargetManager::OnResetRetargetBasePose))
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Text(LOCTEXT("ResetRetargetBasePose_Label", "Reset Pose"))
				.ToolTipText(LOCTEXT("ResetRetargetBasePose_Tooltip", "Restore Retarget Base Pose to Mesh Refeference Pose"))
			]

			+SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Center)
			[
				SNew(SButton)
				.OnClicked(FOnClicked::CreateSP(this, &SRetargetManager::OnViewRetargetBasePose))
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Text(this, &SRetargetManager::GetToggleRetargetBasePose)
				.ToolTipText(LOCTEXT("ViewRetargetBasePose_Tooltip", "Toggle to View/Edit Retarget Base Pose"))
			]

			+SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Center)
			[
				SNew(SButton)
				.OnClicked(FOnClicked::CreateSP(this, &SRetargetManager::OnSaveRetargetBasePose))
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Text(LOCTEXT("SaveRetargetBasePose_Label", "Save Pose"))
				.ToolTipText(LOCTEXT("SaveRetargetBasePose_Tooltip", "Save Current Pose to Retarget Base Pose"))
			]
		]
	];
}

FReply SRetargetManager::OnViewRetargetBasePose()
{
	UDebugSkelMeshComponent * PreviewMeshComp = PreviewScenePtr.Pin()->GetPreviewMeshComponent();
	if (PreviewMeshComp && PreviewMeshComp->PreviewInstance)
	{
		const FScopedTransaction Transaction(LOCTEXT("ViewRetargetBasePose_Action", "Edit Retarget Base Pose"));
		PreviewMeshComp->PreviewInstance->SetForceRetargetBasePose(!PreviewMeshComp->PreviewInstance->GetForceRetargetBasePose());
		PreviewMeshComp->Modify();
		// reset all bone transform since you don't want to keep any bone transform change
		PreviewMeshComp->PreviewInstance->ResetModifiedBone();
		// add root 
		if (PreviewMeshComp->PreviewInstance->GetForceRetargetBasePose())
		{
			PreviewMeshComp->BonesOfInterest.Add(0);
		}
	}

	return FReply::Handled();
}

FReply SRetargetManager::OnSaveRetargetBasePose()
{
	UDebugSkelMeshComponent * PreviewMeshComp = PreviewScenePtr.Pin()->GetPreviewMeshComponent();
	if (PreviewMeshComp && PreviewMeshComp->SkeletalMesh)
	{
		USkeletalMesh * PreviewMesh = PreviewMeshComp->SkeletalMesh;

		check(PreviewMesh && &EditableSkeletonPtr.Pin()->GetSkeleton() == PreviewMesh->Skeleton);

		if (PreviewMesh)
		{
			const FScopedTransaction Transaction( LOCTEXT("SaveRetargetBasePose_Action", "Save Retarget Base Pose") );
			PreviewMesh->Modify();
			// get space bases and calculate local
			const TArray<FTransform> & SpaceBases = PreviewMeshComp->GetComponentSpaceTransforms();
			// @todo check to see if skeleton vs preview mesh makes it different for missing bones
			const FReferenceSkeleton& RefSkeleton = PreviewMesh->RefSkeleton;
			TArray<FTransform> & NewRetargetBasePose = PreviewMesh->RetargetBasePose;
			// if you're using master pose component in preview, this won't work
			check (PreviewMesh->RefSkeleton.GetNum() == SpaceBases.Num());
			int32 TotalNumBones = PreviewMesh->RefSkeleton.GetNum();
			NewRetargetBasePose.Empty(TotalNumBones);
			NewRetargetBasePose.AddUninitialized(TotalNumBones);

			for (int32 BoneIndex = 0; BoneIndex<TotalNumBones; ++BoneIndex)
			{
				// this is slower, but skeleton can have more bones, so I can't just access
				// Parent index from Skeleton. Going safer route
				FName BoneName = PreviewMeshComp->GetBoneName(BoneIndex);
				FName ParentBoneName = PreviewMeshComp->GetParentBone(BoneName);
				int32 ParentIndex = RefSkeleton.FindBoneIndex(ParentBoneName);

				if (ParentIndex != INDEX_NONE)
				{
					NewRetargetBasePose[BoneIndex] = SpaceBases[BoneIndex].GetRelativeTransform(SpaceBases[ParentIndex]);
				}
				else
				{
					NewRetargetBasePose[BoneIndex] = SpaceBases[BoneIndex];
				}
			}

			// Clear PreviewMeshComp bone modified, they're baked now
			PreviewMeshComp->PreviewInstance->ResetModifiedBone();
			// turn off the retarget base pose if you're looking at it
			PreviewMeshComp->PreviewInstance->SetForceRetargetBasePose(false);
		}
	}
	return FReply::Handled();
}

FReply SRetargetManager::OnResetRetargetBasePose()
{
	const FText Message = LOCTEXT("ResetRetargetBasePose_Confirm", "This will reset current Retarget Base Pose to the Reference Pose of current preview mesh. Would you like to continue?");
	EAppReturnType::Type Response = FMessageDialog::Open(EAppMsgType::OkCancel, Message);
	if(Response == EAppReturnType::Ok)
	{
		UDebugSkelMeshComponent * PreviewMeshComp = PreviewScenePtr.Pin()->GetPreviewMeshComponent();
		if(PreviewMeshComp && PreviewMeshComp->SkeletalMesh)
		{
			USkeletalMesh * PreviewMesh = PreviewMeshComp->SkeletalMesh;

			check(PreviewMesh && &EditableSkeletonPtr.Pin()->GetSkeleton() == PreviewMesh->Skeleton);

			if(PreviewMesh)
			{
				const FScopedTransaction Transaction(LOCTEXT("ResetRetargetBasePose_Action", "Reset Retarget Base Pose"));
				PreviewMesh->Modify();
				// reset to original ref pose
				PreviewMesh->RetargetBasePose = PreviewMesh->RefSkeleton.GetRefBonePose();
				// turn off the retarget base pose if you're looking at it
				PreviewMeshComp->PreviewInstance->SetForceRetargetBasePose(true);
			}
		}
	}

	return FReply::Handled();
}

void SRetargetManager::PostUndo()
{
}

FText SRetargetManager::GetToggleRetargetBasePose() const
{
	UDebugSkelMeshComponent * PreviewMeshComp = PreviewScenePtr.Pin()->GetPreviewMeshComponent();
	if(PreviewMeshComp && PreviewMeshComp->PreviewInstance)
	{
		if (PreviewMeshComp->PreviewInstance->GetForceRetargetBasePose())
		{
			return LOCTEXT("HideRetargetBasePose_Label", "Hide Pose");
		}
		else
		{
			return LOCTEXT("ViewRetargetBasePose_Label", "View Pose");
		}
	}

	return LOCTEXT("InvalidRetargetBasePose_Label", "No Mesh for Base Pose");
}
#undef LOCTEXT_NAMESPACE

