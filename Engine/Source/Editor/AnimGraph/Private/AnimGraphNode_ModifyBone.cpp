// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphPrivatePCH.h"
#include "AnimGraphNode_ModifyBone.h"
#include "CompilerResultsLog.h"

/////////////////////////////////////////////////////
// UAnimGraphNode_ModifyBone

#define LOCTEXT_NAMESPACE "A3Nodes"

UAnimGraphNode_ModifyBone::UAnimGraphNode_ModifyBone(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CurWidgetMode = (int32)FWidget::WM_Rotate;
}

void UAnimGraphNode_ModifyBone::ValidateAnimNodeDuringCompilation(USkeleton* ForSkeleton, FCompilerResultsLog& MessageLog)
{
	if (ForSkeleton->GetReferenceSkeleton().FindBoneIndex(Node.BoneToModify.BoneName) == INDEX_NONE)
	{
		MessageLog.Warning(*LOCTEXT("NoBoneToModify", "@@ - You must pick a bone to modify").ToString(), this);
	}

	if ((Node.TranslationMode == BMM_Ignore) && (Node.RotationMode == BMM_Ignore) && (Node.ScaleMode == BMM_Ignore))
	{
		MessageLog.Warning(*LOCTEXT("NothingToModify", "@@ - No components to modify selected.  Either Rotation, Translation, or Scale should be set to something other than Ignore").ToString(), this);
	}

	Super::ValidateAnimNodeDuringCompilation(ForSkeleton, MessageLog);
}

FText UAnimGraphNode_ModifyBone::GetControllerDescription() const
{
	return LOCTEXT("TransformModifyBone", "Transform (Modify) Bone");
}

FText UAnimGraphNode_ModifyBone::GetTooltipText() const
{
	return LOCTEXT("AnimGraphNode_ModifyBone_Tooltip", "The Transform Bone node alters the transform - i.e. Translation, Rotation, or Scale - of the bone");
}

FText UAnimGraphNode_ModifyBone::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if ((TitleType == ENodeTitleType::ListView || TitleType == ENodeTitleType::MenuTitle) && (Node.BoneToModify.BoneName == NAME_None))
	{
		return GetControllerDescription();
	}
	// @TODO: the bone can be altered in the property editor, so we have to 
	//        choose to mark this dirty when that happens for this to properly work
	else //if (!CachedNodeTitles.IsTitleCached(TitleType, this))
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("ControllerDescription"), GetControllerDescription());
		Args.Add(TEXT("BoneName"), FText::FromName(Node.BoneToModify.BoneName));

		// FText::Format() is slow, so we cache this to save on performance
		if (TitleType == ENodeTitleType::ListView || TitleType == ENodeTitleType::MenuTitle)
		{
			CachedNodeTitles.SetCachedTitle(TitleType, FText::Format(LOCTEXT("AnimGraphNode_ModifyBone_ListTitle", "{ControllerDescription} - Bone: {BoneName}"), Args), this);
		}
		else
		{
			CachedNodeTitles.SetCachedTitle(TitleType, FText::Format(LOCTEXT("AnimGraphNode_ModifyBone_Title", "{ControllerDescription}\nBone: {BoneName}"), Args), this);
		}
	}
	return CachedNodeTitles[TitleType];
}

int32 UAnimGraphNode_ModifyBone::GetWidgetCoordinateSystem(const USkeletalMeshComponent* SkelComp)
{
	EBoneControlSpace Space = BCS_BoneSpace;
	switch (CurWidgetMode)
	{
	case FWidget::WM_Rotate:
		Space = Node.RotationSpace;
		break;
	case FWidget::WM_Translate:
		Space = Node.TranslationSpace;
		break;
	case FWidget::WM_Scale:
		Space = Node.ScaleSpace;
		break;
	}

	switch (Space)
	{
	default:
	case BCS_ParentBoneSpace:
		//@TODO: No good way of handling this one
		return COORD_World;
	case BCS_BoneSpace:
		return COORD_Local;
	case BCS_ComponentSpace:
	case BCS_WorldSpace:
		return COORD_World;
	}
}

FVector UAnimGraphNode_ModifyBone::GetWidgetLocation(const USkeletalMeshComponent* SkelComp, FAnimNode_SkeletalControlBase* AnimNode)
{
	USkeleton* Skeleton = SkelComp->SkeletalMesh->Skeleton;
	FVector WidgetLoc = FVector::ZeroVector;

	// if the current widget mode is translate, then shows the widget according to translation space
	if (CurWidgetMode == FWidget::WM_Translate)
	{
		FCSPose<FCompactPose>& MeshBases = AnimNode->ForwardedPose;
		WidgetLoc = ConvertWidgetLocation(SkelComp, MeshBases, Node.BoneToModify.BoneName, GetNodeValue(FString("Translation"), Node.Translation), Node.TranslationSpace);

		if (MeshBases.GetPose().IsValid() && Node.TranslationMode == BMM_Additive)
		{
			if(Node.TranslationSpace == EBoneControlSpace::BCS_WorldSpace ||
				Node.TranslationSpace == EBoneControlSpace::BCS_ComponentSpace)
			{
				const FMeshPoseBoneIndex MeshBoneIndex(SkelComp->GetBoneIndex(Node.BoneToModify.BoneName));
				const FCompactPoseBoneIndex BoneIndex = MeshBases.GetPose().GetBoneContainer().MakeCompactPoseIndex(MeshBoneIndex);

				if (BoneIndex != INDEX_NONE)
				{
					const FTransform& BoneTM = MeshBases.GetComponentSpaceTransform(BoneIndex);
					WidgetLoc += BoneTM.GetLocation();
				}
			}
		}
	}
	else // if the current widget mode is not translate mode, then show the widget on the bone to modify
	{
		int32 MeshBoneIndex = SkelComp->GetBoneIndex(Node.BoneToModify.BoneName);

		if (MeshBoneIndex != INDEX_NONE)
		{
			const FTransform BoneTM = SkelComp->GetBoneTransform(MeshBoneIndex);
			WidgetLoc = BoneTM.GetLocation();
		}
	}
	
	return WidgetLoc;
}

EBoneModificationMode UAnimGraphNode_ModifyBone::GetBoneModificationMode(int32 InWidgetMode)
{
	FWidget::EWidgetMode InMode = (FWidget::EWidgetMode)InWidgetMode;
	switch (InMode)
	{
	case FWidget::WM_Translate:
		return Node.TranslationMode;
		break;
	case FWidget::WM_Rotate:
		return Node.RotationMode;
		break;
	case FWidget::WM_Scale:
		return Node.ScaleMode;
		break;
	case FWidget::WM_TranslateRotateZ:
	case FWidget::WM_2D:
		break;
	}

	return EBoneModificationMode::BMM_Ignore;
}

int32 UAnimGraphNode_ModifyBone::GetNextWidgetMode(int32 InWidgetMode)
{
	FWidget::EWidgetMode InMode = (FWidget::EWidgetMode)InWidgetMode;
	switch (InMode)
	{
	case FWidget::WM_Translate:
		return FWidget::WM_Rotate;
	case FWidget::WM_Rotate:
		return FWidget::WM_Scale;
	case FWidget::WM_Scale:
		return FWidget::WM_Translate;
	case FWidget::WM_TranslateRotateZ:
	case FWidget::WM_2D:
		break;
	}

	return (int32)FWidget::WM_None;
}

int32 UAnimGraphNode_ModifyBone::FindValidWidgetMode(int32 InWidgetMode)
{	
	FWidget::EWidgetMode InMode = (FWidget::EWidgetMode)InWidgetMode;
	FWidget::EWidgetMode ValidMode = InMode;
	if (InMode == FWidget::WM_None)
	{	// starts from Rotate mode
		ValidMode = FWidget::WM_Rotate;
	}

	// find from current widget mode and loop 1 cycle until finding a valid mode
	for (int32 Index = 0; Index < 3; Index++)
	{
		if (GetBoneModificationMode(ValidMode) != BMM_Ignore)
		{
			return ValidMode;
		}

		ValidMode = (FWidget::EWidgetMode)GetNextWidgetMode(ValidMode);
	}
	
	// if couldn't find a valid mode, returns None
	ValidMode = FWidget::WM_None;

	return (int32)ValidMode;
}

int32 UAnimGraphNode_ModifyBone::GetWidgetMode(const USkeletalMeshComponent* SkelComp)
{
	int32 BoneIndex = SkelComp->GetBoneIndex(Node.BoneToModify.BoneName);
	if (BoneIndex != INDEX_NONE)
	{
		CurWidgetMode = FindValidWidgetMode(CurWidgetMode);
		return CurWidgetMode;
	}

	return (int32)FWidget::WM_None;
}

int32 UAnimGraphNode_ModifyBone::ChangeToNextWidgetMode(const USkeletalMeshComponent* SkelComp, int32 InCurWidgetMode)
{
	int32 NextWidgetMode = GetNextWidgetMode(InCurWidgetMode);
	CurWidgetMode = FindValidWidgetMode(NextWidgetMode);

	return CurWidgetMode;
}

bool UAnimGraphNode_ModifyBone::SetWidgetMode(const USkeletalMeshComponent* SkelComp, int32 InWidgetMode)
{
	// if InWidgetMode is available 
	if (FindValidWidgetMode(InWidgetMode) == InWidgetMode)
	{
		CurWidgetMode = InWidgetMode;
		return true;
	}

	return false;
}

FName UAnimGraphNode_ModifyBone::FindSelectedBone()
{
	return Node.BoneToModify.BoneName;
}

void UAnimGraphNode_ModifyBone::DoTranslation(const USkeletalMeshComponent* SkelComp, FVector& Drag, FAnimNode_Base* InOutAnimNode)
{
	FAnimNode_ModifyBone* ModifyBone = static_cast<FAnimNode_ModifyBone*>(InOutAnimNode);

	if (ModifyBone->TranslationMode != EBoneModificationMode::BMM_Ignore)
	{
		FVector Offset = ConvertCSVectorToBoneSpace(SkelComp, Drag, ModifyBone->ForwardedPose, Node.BoneToModify.BoneName, Node.TranslationSpace);

		ModifyBone->Translation += Offset;

		Node.Translation = ModifyBone->Translation;
	}
}

void UAnimGraphNode_ModifyBone::DoRotation(const USkeletalMeshComponent* SkelComp, FRotator& Rotation, FAnimNode_Base* InOutAnimNode)
{
	FAnimNode_ModifyBone* ModifyBone = static_cast<FAnimNode_ModifyBone*>(InOutAnimNode);

	if (Node.RotationMode != EBoneModificationMode::BMM_Ignore)
	{
		FQuat DeltaQuat = ConvertCSRotationToBoneSpace(SkelComp, Rotation, ModifyBone->ForwardedPose, Node.BoneToModify.BoneName, Node.RotationSpace);

		FQuat PrevQuat(ModifyBone->Rotation);
		FQuat NewQuat = DeltaQuat * PrevQuat;
		ModifyBone->Rotation = NewQuat.Rotator();
		Node.Rotation = ModifyBone->Rotation;
	}
}

void UAnimGraphNode_ModifyBone::DoScale(const USkeletalMeshComponent* SkelComp, FVector& Scale, FAnimNode_Base* InOutAnimNode)
{
	FAnimNode_ModifyBone* ModifyBone = static_cast<FAnimNode_ModifyBone*>(InOutAnimNode);

	if (Node.ScaleMode != EBoneModificationMode::BMM_Ignore)
	{
		FVector Offset = Scale;
		ModifyBone->Scale += Offset;
		Node.Scale = ModifyBone->Scale;
	}
}

void UAnimGraphNode_ModifyBone::CopyNodeDataTo(FAnimNode_Base* OutAnimNode)
{
	FAnimNode_ModifyBone* ModifyBone = static_cast<FAnimNode_ModifyBone*>(OutAnimNode);

	// copies Pin values from the internal node to get data which are not compiled yet
	ModifyBone->Translation = GetNodeValue(FString("Translation"), Node.Translation);
	ModifyBone->Rotation = GetNodeValue(FString("Rotation"), Node.Rotation);
	ModifyBone->Scale = GetNodeValue(FString("Scale"), Node.Scale);

	// copies Modes
	ModifyBone->TranslationMode = Node.TranslationMode;
	ModifyBone->RotationMode = Node.RotationMode;
	ModifyBone->ScaleMode = Node.ScaleMode;

	// copies Spaces
	ModifyBone->TranslationSpace = Node.TranslationSpace;
	ModifyBone->RotationSpace = Node.RotationSpace;
	ModifyBone->ScaleSpace = Node.ScaleSpace;
}

void UAnimGraphNode_ModifyBone::CopyNodeDataFrom(const FAnimNode_Base* InNewAnimNode)
{
	const FAnimNode_ModifyBone* ModifyBone = static_cast<const FAnimNode_ModifyBone*>(InNewAnimNode);

	switch (CurWidgetMode)
	{
	case FWidget::WM_Translate:
		SetNodeValue(FString("Translation"), Node.Translation, ModifyBone->Translation);
		break;
	case FWidget::WM_Rotate:
		SetNodeValue(FString("Rotation"), Node.Rotation, ModifyBone->Rotation);
		break;
	case FWidget::WM_Scale:
		SetNodeValue(FString("Scale"), Node.Scale, ModifyBone->Scale);
		break;
	}
}
#undef LOCTEXT_NAMESPACE
