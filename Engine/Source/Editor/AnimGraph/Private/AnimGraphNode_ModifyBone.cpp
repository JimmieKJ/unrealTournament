// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphPrivatePCH.h"
#include "AnimGraphNode_ModifyBone.h"

/////////////////////////////////////////////////////
// UAnimGraphNode_ModifyBone

#define LOCTEXT_NAMESPACE "A3Nodes"

UAnimGraphNode_ModifyBone::UAnimGraphNode_ModifyBone(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CurWidgetMode = (int32)FWidget::WM_Rotate;
}

FText UAnimGraphNode_ModifyBone::GetControllerDescription() const
{
	return LOCTEXT("TransformModifyBone", "Transform (Modify) Bone");
}

FString UAnimGraphNode_ModifyBone::GetKeywords() const
{
	return TEXT("Modify, Transform");
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
	else //if (!CachedNodeTitles.IsTitleCached(TitleType))
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("ControllerDescription"), GetControllerDescription());
		Args.Add(TEXT("BoneName"), FText::FromName(Node.BoneToModify.BoneName));

		// FText::Format() is slow, so we cache this to save on performance
		if (TitleType == ENodeTitleType::ListView || TitleType == ENodeTitleType::MenuTitle)
		{
			CachedNodeTitles.SetCachedTitle(TitleType, FText::Format(LOCTEXT("AnimGraphNode_ModifyBone_ListTitle", "{ControllerDescription} - Bone: {BoneName}"), Args));
		}
		else
		{
			CachedNodeTitles.SetCachedTitle(TitleType, FText::Format(LOCTEXT("AnimGraphNode_ModifyBone_Title", "{ControllerDescription}\nBone: {BoneName}"), Args));
		}
	}
	return CachedNodeTitles[TitleType];
}

FVector UAnimGraphNode_ModifyBone::GetWidgetLocation(const USkeletalMeshComponent* SkelComp, FAnimNode_SkeletalControlBase* AnimNode)
{
	USkeleton * Skeleton = SkelComp->SkeletalMesh->Skeleton;
	FVector WidgetLoc = FVector::ZeroVector;

	// if the current widget mode is translate, then shows the widget according to translation space
	if (CurWidgetMode == FWidget::WM_Translate)
	{
		FA2CSPose& MeshBases = AnimNode->ForwardedPose;
		WidgetLoc = ConvertWidgetLocation(SkelComp, MeshBases, Node.BoneToModify.BoneName, Node.Translation, Node.TranslationSpace);

		if(Node.TranslationMode == BMM_Additive)
		{
			if(Node.TranslationSpace == EBoneControlSpace::BCS_WorldSpace ||
				Node.TranslationSpace == EBoneControlSpace::BCS_ComponentSpace)
			{
				int32 MeshBoneIndex = SkelComp->GetBoneIndex(Node.BoneToModify.BoneName);
				if(MeshBoneIndex != INDEX_NONE)
				{
					FTransform BoneTM = MeshBases.GetComponentSpaceTransform(MeshBoneIndex);
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

void UAnimGraphNode_ModifyBone::UpdateDefaultValues(const FAnimNode_Base* AnimNode)
{
	FString UpdateDefaultValueName;
	FVector UpdateValue;

	const FAnimNode_ModifyBone* ModifyBone = static_cast<const FAnimNode_ModifyBone*>(AnimNode);

	if (!ModifyBone)
	{
		return;
	}

	switch (CurWidgetMode)
	{
	case FWidget::WM_Translate:

		UpdateDefaultValueName = FString("Translation");
		UpdateValue = ModifyBone->Translation;
		break;
	case FWidget::WM_Rotate:
		UpdateDefaultValueName = FString("Rotation");
		UpdateValue.X = ModifyBone->Rotation.Pitch;
		UpdateValue.Y = ModifyBone->Rotation.Yaw;
		UpdateValue.Z = ModifyBone->Rotation.Roll;
		break;
	case FWidget::WM_Scale:
		UpdateDefaultValueName = FString("Scale");
		UpdateValue = ModifyBone->Scale;
		break;
	}

	SetDefaultValue(UpdateDefaultValueName, UpdateValue);
}

void UAnimGraphNode_ModifyBone::UpdateAllDefaultValues(const FAnimNode_Base* AnimNode)
{
	FString UpdateDefaultValueName;
	FVector UpdateValue;

	const FAnimNode_ModifyBone* ModifyBone = static_cast<const FAnimNode_ModifyBone*>(AnimNode);

	if (!ModifyBone)
	{
		return;
	}

	UpdateDefaultValueName = FString("Translation");
	UpdateValue = ModifyBone->Translation;
	SetDefaultValue(UpdateDefaultValueName, UpdateValue);

	UpdateDefaultValueName = FString("Rotation");
	UpdateValue.X = ModifyBone->Rotation.Pitch;
	UpdateValue.Y = ModifyBone->Rotation.Yaw;
	UpdateValue.Z = ModifyBone->Rotation.Roll;
	SetDefaultValue(UpdateDefaultValueName, UpdateValue);

	UpdateDefaultValueName = FString("Scale");
	UpdateValue = ModifyBone->Scale;
	SetDefaultValue(UpdateDefaultValueName, UpdateValue);
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
	ModifyBone->Translation = Node.Translation;
	ModifyBone->Rotation = Node.Rotation;
	ModifyBone->Scale = Node.Scale;

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

	// copies Pin data from updated values
	Node.Translation = ModifyBone->Translation;
	Node.Rotation = ModifyBone->Rotation;
	Node.Scale = ModifyBone->Scale;
}
#undef LOCTEXT_NAMESPACE
