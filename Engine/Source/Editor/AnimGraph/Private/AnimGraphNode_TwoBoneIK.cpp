// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphPrivatePCH.h"
#include "AnimGraphNode_TwoBoneIK.h"
#include "DebugRenderSceneProxy.h"

// for customization details
#include "../../PropertyEditor/Public/PropertyHandle.h"
#include "../../PropertyEditor/Public/DetailLayoutBuilder.h"
#include "../../PropertyEditor/Public/DetailCategoryBuilder.h"
#include "../../PropertyEditor/Public/DetailWidgetRow.h"
#include "../../PropertyEditor/Public/IDetailPropertyRow.h"

#define LOCTEXT_NAMESPACE "A3Nodes"

/////////////////////////////////////////////////////
// FTwoBoneIKDelegate

class FTwoBoneIKDelegate : public TSharedFromThis<FTwoBoneIKDelegate>
{
public:
	void UpdateLocationSpace(class IDetailLayoutBuilder* DetailBuilder)
	{
		if (DetailBuilder)
		{
			DetailBuilder->ForceRefreshDetails();
		}
	}
};

TSharedPtr<FTwoBoneIKDelegate> UAnimGraphNode_TwoBoneIK::TwoBoneIKDelegate = NULL;

/////////////////////////////////////////////////////
// UAnimGraphNode_TwoBoneIK


UAnimGraphNode_TwoBoneIK::UAnimGraphNode_TwoBoneIK(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	BoneSelectMode = BSM_EndEffector;
}

FText UAnimGraphNode_TwoBoneIK::GetControllerDescription() const
{
	return LOCTEXT("TwoBoneIK", "Two Bone IK");
}

FText UAnimGraphNode_TwoBoneIK::GetTooltipText() const
{
	return LOCTEXT("AnimGraphNode_TwoBoneIK_Tooltip", "The Two Bone IK control applies an inverse kinematic (IK) solver to a 3-joint chain, such as the limbs of a character.");
}

FText UAnimGraphNode_TwoBoneIK::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if ((TitleType == ENodeTitleType::ListView || TitleType == ENodeTitleType::MenuTitle) && (Node.IKBone.BoneName == NAME_None))
	{
		return GetControllerDescription();
	}
	// @TODO: the bone can be altered in the property editor, so we have to 
	//        choose to mark this dirty when that happens for this to properly work
	else //if (!CachedNodeTitles.IsTitleCached(TitleType, this))
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("ControllerDescription"), GetControllerDescription());
		Args.Add(TEXT("BoneName"), FText::FromName(Node.IKBone.BoneName));

		// FText::Format() is slow, so we cache this to save on performance
		if (TitleType == ENodeTitleType::ListView || TitleType == ENodeTitleType::MenuTitle)
		{
			CachedNodeTitles.SetCachedTitle(TitleType, FText::Format(LOCTEXT("AnimGraphNode_IKBone_ListTitle", "{ControllerDescription} - Bone: {BoneName}"), Args), this);
		}
		else
		{
			CachedNodeTitles.SetCachedTitle(TitleType, FText::Format(LOCTEXT("AnimGraphNode_IKBone_Title", "{ControllerDescription}\nBone: {BoneName}"), Args), this);
		}
	}
	return CachedNodeTitles[TitleType];
}

void UAnimGraphNode_TwoBoneIK::Draw( FPrimitiveDrawInterface* PDI, USkeletalMeshComponent* SkelMeshComp ) const
{
	if (SkelMeshComp && SkelMeshComp->SkeletalMesh && SkelMeshComp->SkeletalMesh->Skeleton)
	{
		USkeleton * Skeleton = SkelMeshComp->SkeletalMesh->Skeleton;

		DrawTargetLocation(PDI, SkelMeshComp, Skeleton, Node.EffectorLocationSpace, Node.EffectorSpaceBoneName, Node.EffectorLocation, FColor(255, 128, 128), FColor(180, 128, 128));
		DrawTargetLocation(PDI, SkelMeshComp, Skeleton, Node.JointTargetLocationSpace, Node.JointTargetSpaceBoneName, Node.JointTargetLocation, FColor(128, 255, 128), FColor(128, 180, 128));
	}
}

void UAnimGraphNode_TwoBoneIK::DrawTargetLocation(FPrimitiveDrawInterface* PDI, USkeletalMeshComponent* SkelComp, USkeleton * Skeleton, EBoneControlSpace SpaceBase, FName SpaceBoneName, const FVector & TargetLocation, const FColor & TargetColor, const FColor & BoneColor) const
{
	const bool bInBoneSpace = (SpaceBase == BCS_ParentBoneSpace) || (SpaceBase == BCS_BoneSpace);
	const int32 SpaceBoneIndex = bInBoneSpace ? Skeleton->GetReferenceSkeleton().FindBoneIndex(SpaceBoneName) : INDEX_NONE;
	// Transform EffectorLocation from EffectorLocationSpace to ComponentSpace.
	FTransform TargetTransform = FTransform (TargetLocation);
	FTransform CSTransform;

	ConvertToComponentSpaceTransform(SkelComp, TargetTransform, CSTransform, SpaceBoneIndex, SpaceBase);

	FTransform WorldTransform = CSTransform * SkelComp->ComponentToWorld;

#if 0 // @TODO : remove this code because this doesn't show correct target location
	DrawCoordinateSystem( PDI, WorldTransform.GetLocation(), WorldTransform.GetRotation().Rotator(), 20.f, SDPG_Foreground );
	DrawWireDiamond( PDI, WorldTransform.ToMatrixWithScale(), 2.f, TargetColor, SDPG_Foreground );
#endif

	if (bInBoneSpace)
	{
		ConvertToComponentSpaceTransform(SkelComp, FTransform::Identity, CSTransform, SpaceBoneIndex, SpaceBase);
		WorldTransform = CSTransform * SkelComp->ComponentToWorld;
		DrawCoordinateSystem( PDI, WorldTransform.GetLocation(), WorldTransform.GetRotation().Rotator(), 20.f, SDPG_Foreground );
		DrawWireDiamond( PDI, WorldTransform.ToMatrixWithScale(), 2.f, BoneColor, SDPG_Foreground );
	}
}

void UAnimGraphNode_TwoBoneIK::CopyNodeDataTo(FAnimNode_Base* AnimNode)
{
	FAnimNode_TwoBoneIK* TwoBoneIK = static_cast<FAnimNode_TwoBoneIK*>(AnimNode);

	// copies Pin values from the internal node to get data which are not compiled yet
	TwoBoneIK->EffectorLocation = GetNodeValue(FString("EffectorLocation"), Node.EffectorLocation);
	TwoBoneIK->JointTargetLocation = GetNodeValue(FString("JointTargetLocation"), Node.JointTargetLocation);
}

void UAnimGraphNode_TwoBoneIK::CopyNodeDataFrom(const FAnimNode_Base* InNewAnimNode)
{
	const FAnimNode_TwoBoneIK* TwoBoneIK = static_cast<const FAnimNode_TwoBoneIK*>(InNewAnimNode);
	
	if (BoneSelectMode == BSM_EndEffector)
	{
		SetNodeValue(FString("EffectorLocation"), Node.EffectorLocation, TwoBoneIK->EffectorLocation);
	}
	else
	{
		SetNodeValue(FString("JointTargetLocation"), Node.JointTargetLocation, TwoBoneIK->JointTargetLocation);
	}
}


FVector UAnimGraphNode_TwoBoneIK::GetWidgetLocation(const USkeletalMeshComponent* SkelComp, FAnimNode_SkeletalControlBase* AnimNode)
{
	EBoneControlSpace Space;
	FName BoneName;
	FVector Location;
	 
	if (BoneSelectMode == BSM_EndEffector)
	{
		Space = Node.EffectorLocationSpace;
		Location = GetNodeValue(FString("EffectorLocation"), Node.EffectorLocation);
		BoneName = Node.EffectorSpaceBoneName;

	}
	else // BSM_JointTarget
	{
		Space = Node.JointTargetLocationSpace;

		if (Space == BCS_WorldSpace || Space == BCS_ComponentSpace)
		{
			return FVector::ZeroVector;
		}
		Location = GetNodeValue(FString("JointTargetLocation"), Node.JointTargetLocation);
		BoneName = Node.JointTargetSpaceBoneName;
	}

	return ConvertWidgetLocation(SkelComp, AnimNode->ForwardedPose, BoneName, Location, Space);
}

int32 UAnimGraphNode_TwoBoneIK::GetWidgetMode(const USkeletalMeshComponent* SkelComp)
{
	int32 BoneIndex = SkelComp->GetBoneIndex(Node.IKBone.BoneName);
	// Two bone IK node uses only Translate
	if (BoneIndex != INDEX_NONE)
	{
		return FWidget::WM_Translate;
	}

	return FWidget::WM_None;
}

void UAnimGraphNode_TwoBoneIK::MoveSelectActorLocation(const USkeletalMeshComponent* SkelComp, FAnimNode_SkeletalControlBase* AnimNode)
{
	USkeleton * Skeleton = SkelComp->SkeletalMesh->Skeleton;

	// create a bone select actor

	if (!BoneSelectActor.IsValid())
	{
		if (Node.JointTargetLocationSpace == EBoneControlSpace::BCS_BoneSpace ||
			Node.JointTargetLocationSpace == EBoneControlSpace::BCS_ParentBoneSpace)
		{
			int32 JointTargetIndex = Skeleton->GetReferenceSkeleton().FindBoneIndex(Node.JointTargetSpaceBoneName);

			if (JointTargetIndex != INDEX_NONE)
			{
				UWorld* World = SkelComp->GetWorld();
				check(World);

				BoneSelectActor = World->SpawnActor<ABoneSelectActor>(FVector(0), FRotator(0, 0, 0));
				check(BoneSelectActor.IsValid());
			}
		}
	}

	if (!BoneSelectActor.IsValid())
	{
		return;
	}

	// move the actor's position
	if (BoneSelectMode == BSM_JointTarget)
	{
		FName BoneName;
		int32 EffectorBoneIndex = SkelComp->GetBoneIndex(Node.EffectorSpaceBoneName);
		if (EffectorBoneIndex != INDEX_NONE)
		{
			BoneName = Node.EffectorSpaceBoneName;
		}
		else
		{
			BoneName = Node.IKBone.BoneName;
		}

		FVector ActorLocation = ConvertWidgetLocation(SkelComp, AnimNode->ForwardedPose, BoneName, Node.EffectorLocation, Node.EffectorLocationSpace);
		BoneSelectActor->SetActorLocation(ActorLocation + FVector(0, 10, 0));

	}
	else if (BoneSelectMode == BSM_EndEffector)
	{
		int32 JointTargetIndex = SkelComp->GetBoneIndex(Node.JointTargetSpaceBoneName);

		if (JointTargetIndex != INDEX_NONE)
		{
			FVector ActorLocation = ConvertWidgetLocation(SkelComp, AnimNode->ForwardedPose, Node.JointTargetSpaceBoneName, Node.JointTargetLocation, Node.JointTargetLocationSpace);
			BoneSelectActor->SetActorLocation(ActorLocation + FVector(0, 10, 0));
		}
	}
}

FName UAnimGraphNode_TwoBoneIK::FindSelectedBone()
{
	FName SelectedBone;

	// should return mesh bone index
	if (BoneSelectMode == BSM_EndEffector)
	{
		if (Node.EffectorLocationSpace == EBoneControlSpace::BCS_BoneSpace ||
			Node.EffectorLocationSpace == EBoneControlSpace::BCS_ParentBoneSpace)
		{
			SelectedBone = Node.EffectorSpaceBoneName;
		}
		else
		{
			SelectedBone = Node.IKBone.BoneName;
		}

	}
	else if (BoneSelectMode == BSM_JointTarget)
	{
		SelectedBone = Node.JointTargetSpaceBoneName;
	}

	return SelectedBone;
}

bool UAnimGraphNode_TwoBoneIK::IsActorClicked(HActor* ActorHitProxy)
{
	ABoneSelectActor* BoneActor = Cast<ABoneSelectActor>(ActorHitProxy->Actor);

	if (BoneActor)
	{
		return true;
	}

	return false;
}

void UAnimGraphNode_TwoBoneIK::ProcessActorClick(HActor* ActorHitProxy)
{
	// toggle bone selection mode
	if (BoneSelectMode == BSM_EndEffector)
	{
		BoneSelectMode = BSM_JointTarget;
	}
	else
	{
		BoneSelectMode = BSM_EndEffector;
	}
}

void UAnimGraphNode_TwoBoneIK::DoTranslation(const USkeletalMeshComponent* SkelComp, FVector& Drag, FAnimNode_Base* InOutAnimNode)
{
	FAnimNode_TwoBoneIK* TwoBoneIK = static_cast<FAnimNode_TwoBoneIK*>(InOutAnimNode);

	if (BoneSelectMode == BSM_EndEffector)
	{
		FVector Offset = ConvertCSVectorToBoneSpace(SkelComp, Drag, TwoBoneIK->ForwardedPose, FindSelectedBone(), Node.EffectorLocationSpace);

		TwoBoneIK->EffectorLocation += Offset;
		Node.EffectorLocation = TwoBoneIK->EffectorLocation;
	}
	else if (BoneSelectMode == BSM_JointTarget)
	{
		FVector Offset = ConvertCSVectorToBoneSpace(SkelComp, Drag, TwoBoneIK->ForwardedPose, FindSelectedBone(), Node.JointTargetLocationSpace);

		TwoBoneIK->JointTargetLocation += Offset;
		Node.JointTargetLocation = TwoBoneIK->JointTargetLocation;
	}
}

void UAnimGraphNode_TwoBoneIK::DeselectActor(USkeletalMeshComponent* SkelComp)
{
	if(BoneSelectActor!=NULL)
	{
		if (SkelComp->GetWorld()->DestroyActor(BoneSelectActor.Get()))
		{
			BoneSelectActor = NULL;
		}
	}
}

void UAnimGraphNode_TwoBoneIK::CustomizeDetails(class IDetailLayoutBuilder& DetailBuilder)
{
	// initialize just once
	if (!TwoBoneIKDelegate.IsValid())
	{
		TwoBoneIKDelegate = MakeShareable(new FTwoBoneIKDelegate());
	}

	EBoneControlSpace Space = Node.EffectorLocationSpace;
	const FString TakeRotationPropName = FString::Printf(TEXT("Node.%s"), GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_TwoBoneIK, bTakeRotationFromEffectorSpace));
	const FString MaintainEffectorPropName = FString::Printf(TEXT("Node.%s"), GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_TwoBoneIK, bMaintainEffectorRelRot));
	const FString EffectorBoneName = FString::Printf(TEXT("Node.%s"), GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_TwoBoneIK, EffectorSpaceBoneName));
	const FString EffectorLocationPropName = FString::Printf(TEXT("Node.%s"), GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_TwoBoneIK, EffectorLocation));

	if (Space == BCS_BoneSpace || Space == BCS_ParentBoneSpace)
	{
		IDetailCategoryBuilder& IKCategory = DetailBuilder.EditCategory("IK");
		IDetailCategoryBuilder& EffectorCategory = DetailBuilder.EditCategory("EndEffector");
		TSharedPtr<IPropertyHandle> PropertyHandle;
		PropertyHandle = DetailBuilder.GetProperty(*TakeRotationPropName, GetClass());
		EffectorCategory.AddProperty(PropertyHandle);
		PropertyHandle = DetailBuilder.GetProperty(*MaintainEffectorPropName, GetClass());
		EffectorCategory.AddProperty(PropertyHandle);
		PropertyHandle = DetailBuilder.GetProperty(*EffectorBoneName, GetClass());
		EffectorCategory.AddProperty(PropertyHandle);
	}
	else // hide all properties in EndEffector category
	{
		TSharedPtr<IPropertyHandle> PropertyHandle = DetailBuilder.GetProperty(*EffectorLocationPropName, GetClass());
		DetailBuilder.HideProperty(PropertyHandle);
		PropertyHandle = DetailBuilder.GetProperty(*TakeRotationPropName, GetClass());
		DetailBuilder.HideProperty(PropertyHandle);
		PropertyHandle = DetailBuilder.GetProperty(*MaintainEffectorPropName, GetClass());
		DetailBuilder.HideProperty(PropertyHandle);
		PropertyHandle = DetailBuilder.GetProperty(*EffectorBoneName, GetClass());
		DetailBuilder.HideProperty(PropertyHandle);
	}

	Space = Node.JointTargetLocationSpace;
	bool bPinVisibilityChanged = false;
	const FString JointTargetSpaceBoneName = FString::Printf(TEXT("Node.%s"), GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_TwoBoneIK, JointTargetSpaceBoneName));
	const FString JointTargetLocation = FString::Printf(TEXT("Node.%s"), GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_TwoBoneIK, JointTargetLocation));

	if (Space == BCS_BoneSpace || Space == BCS_ParentBoneSpace)
	{
		IDetailCategoryBuilder& IKCategory = DetailBuilder.EditCategory("IK");
		IDetailCategoryBuilder& EffectorCategory = DetailBuilder.EditCategory("JointTarget");
		TSharedPtr<IPropertyHandle> PropertyHandle;
		PropertyHandle = DetailBuilder.GetProperty(*JointTargetSpaceBoneName, GetClass());
		EffectorCategory.AddProperty(PropertyHandle);
	}
	else // hide all properties in JointTarget category except for JointTargetLocationSpace
	{
		TSharedPtr<IPropertyHandle> PropertyHandle = DetailBuilder.GetProperty(*JointTargetLocation, GetClass());
		DetailBuilder.HideProperty(PropertyHandle);
		PropertyHandle = DetailBuilder.GetProperty(*JointTargetSpaceBoneName, GetClass());
		DetailBuilder.HideProperty(PropertyHandle);
	}

	const FString EffectorLocationSpace = FString::Printf(TEXT("Node.%s"), GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_TwoBoneIK, EffectorLocationSpace));
	const FString JointTargetLocationSpace = FString::Printf(TEXT("Node.%s"), GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_TwoBoneIK, JointTargetLocationSpace));

	// refresh UIs when bone space is changed
	TSharedRef<IPropertyHandle> EffectorLocHandle = DetailBuilder.GetProperty(*EffectorLocationSpace, GetClass());
	if (EffectorLocHandle->IsValidHandle())
	{
		FSimpleDelegate UpdateEffectorSpaceDelegate = FSimpleDelegate::CreateSP(TwoBoneIKDelegate.Get(), &FTwoBoneIKDelegate::UpdateLocationSpace, &DetailBuilder);
		EffectorLocHandle->SetOnPropertyValueChanged(UpdateEffectorSpaceDelegate);
	}

	TSharedRef<IPropertyHandle> JointTragetLocHandle = DetailBuilder.GetProperty(*JointTargetLocationSpace, GetClass());
	if (JointTragetLocHandle->IsValidHandle())
	{
		FSimpleDelegate UpdateJointSpaceDelegate = FSimpleDelegate::CreateSP(TwoBoneIKDelegate.Get(), &FTwoBoneIKDelegate::UpdateLocationSpace, &DetailBuilder);
		JointTragetLocHandle->SetOnPropertyValueChanged(UpdateJointSpaceDelegate);
	}
}

#undef LOCTEXT_NAMESPACE
