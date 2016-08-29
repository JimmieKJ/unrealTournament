// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#include "PersonaPrivatePCH.h"
#include "SRetargetManager.h"
#include "ObjectTools.h"
#include "ScopedTransaction.h"
#include "AssetRegistryModule.h"
#include "Editor/PropertyEditor/Public/PropertyEditorModule.h"
#include "Editor/ContentBrowser/Public/ContentBrowserModule.h"
#include "AssetNotifications.h"
#include "Animation/Rig.h"
#include "BoneSelectionWidget.h"
#include "SRetargetSourceWindow.h"
#include "SRigWindow.h"
#include "Editor/AnimGraph/Classes/AnimPreviewInstance.h"

#define LOCTEXT_NAMESPACE "SRetargetManager"


//////////////////////////////////////////////////////////////////////////
// SRetargetManager

void SRetargetManager::Construct(const FArguments& InArgs)
{
	PersonaPtr = InArgs._Persona;
	Skeleton = NULL;

	if ( PersonaPtr.IsValid() )
	{
		Skeleton = PersonaPtr.Pin()->GetSkeleton();
		PersonaPtr.Pin()->RegisterOnPostUndo(FPersona::FOnPostUndo::CreateSP( this, &SRetargetManager::PostUndo ) );
	}

	const FString DocLink = TEXT("Shared/Editors/Persona");
	ChildSlot
	[
		SNew (SVerticalBox)

		+ SVerticalBox::Slot()
		.Padding(5, 5)
		.AutoHeight()
		[
			// explainint this is retarget source window
			// and what it is
			SNew(STextBlock)
			.TextStyle( FEditorStyle::Get(), "Persona.RetargetManager.ImportantText" )
			.Text(LOCTEXT("RetargetSource_Title", "Manager Retarget Source"))
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
			.Text(LOCTEXT("RetargetSource_Description", "You can add/rename/delete Retarget Sources. When you have different proportional meshes per skeleton, you can use this settings to indicate if this animation is from different source. \
														For example, if your default skeleton is from small guy, and if you have animation for big guy, you can create Retarget Source from the big guy and set it for the animation. \
														Retargeting system will use this information when extracting animation. "))
		]

		+ SVerticalBox::Slot()
		.Padding(2, 5)
		.FillHeight(0.5)
		[
			// construct retarget source window
			SNew(SRetargetSourceWindow)
			.Persona(PersonaPtr)
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
			.Text(LOCTEXT("RigTemplate_Description", "You can set up Rig for this skeleton, then when you retarget animation to different skeleton with the same Rig, it will use the information to convert data. "))
		]

		+ SVerticalBox::Slot()
		.FillHeight(1)
		.Padding(2, 5)
		[
			// construct rig manager window
			SNew(SRigWindow)
			.Persona(PersonaPtr)
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
			.Text(LOCTEXT("BasePose_Description", "This information is used when retargeting assets to different skeleton. You need to make sure the ref pose of both mesh is same when retargeting, so you can see the pose and \
											edit using bone transform widget, and click Save button below. "))
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

SRetargetManager::~SRetargetManager()
{
	if (PersonaPtr.IsValid())
	{
		PersonaPtr.Pin()->UnregisterOnPostUndo(this);
	}
}

FReply SRetargetManager::OnViewRetargetBasePose()
{
	UDebugSkelMeshComponent * PreviewMeshComp = PersonaPtr.Pin()->GetPreviewMeshComponent();
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
	UDebugSkelMeshComponent * PreviewMeshComp = PersonaPtr.Pin()->GetPreviewMeshComponent();
	if (PreviewMeshComp && PreviewMeshComp->SkeletalMesh)
	{
		USkeletalMesh * PreviewMesh = PreviewMeshComp->SkeletalMesh;

		check(PreviewMesh && Skeleton == PreviewMesh->Skeleton);

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
		UDebugSkelMeshComponent * PreviewMeshComp = PersonaPtr.Pin()->GetPreviewMeshComponent();
		if(PreviewMeshComp && PreviewMeshComp->SkeletalMesh)
		{
			USkeletalMesh * PreviewMesh = PreviewMeshComp->SkeletalMesh;

			check(PreviewMesh && Skeleton == PreviewMesh->Skeleton);

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
	UDebugSkelMeshComponent * PreviewMeshComp = PersonaPtr.Pin()->GetPreviewMeshComponent();
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

