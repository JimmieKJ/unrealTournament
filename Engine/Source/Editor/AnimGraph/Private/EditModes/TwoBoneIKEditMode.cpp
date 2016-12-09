// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "EditModes/TwoBoneIKEditMode.h"
#include "SceneManagement.h"
#include "EngineUtils.h"
#include "IPersonaPreviewScene.h"
#include "Animation/DebugSkelMeshComponent.h"

FTwoBoneIKEditMode::FTwoBoneIKEditMode()
	: TwoBoneIKRuntimeNode(nullptr)
	, TwoBoneIKGraphNode(nullptr)
	, BoneSelectMode(BSM_EndEffector)
	, PreviousBoneSpace(BCS_BoneSpace)
{
}

void FTwoBoneIKEditMode::EnterMode(UAnimGraphNode_Base* InEditorNode, FAnimNode_Base* InRuntimeNode)
{
	TwoBoneIKRuntimeNode = static_cast<FAnimNode_TwoBoneIK*>(InRuntimeNode);
	TwoBoneIKGraphNode = CastChecked<UAnimGraphNode_TwoBoneIK>(InEditorNode);

	NodePropertyDelegateHandle = TwoBoneIKGraphNode->OnNodePropertyChanged().AddSP(this, &FTwoBoneIKEditMode::OnExternalNodePropertyChange);
	PreviousBoneSpace = TwoBoneIKGraphNode->Node.EffectorLocationSpace;

	FAnimNodeEditMode::EnterMode(InEditorNode, InRuntimeNode);

	ManageBoneSelectActor();
}

void FTwoBoneIKEditMode::ExitMode()
{
	// destroy bone select actor
	if (BoneSelectActor != nullptr)
	{
		USkeletalMeshComponent* SkelComp = GetAnimPreviewScene().GetPreviewMeshComponent();
		if (SkelComp->GetWorld()->DestroyActor(BoneSelectActor.Get()))
		{
			BoneSelectActor = nullptr;
		}
	}

	TwoBoneIKGraphNode->OnNodePropertyChanged().Remove(NodePropertyDelegateHandle);

	TwoBoneIKGraphNode = nullptr;
	TwoBoneIKRuntimeNode = nullptr;

	FAnimNodeEditMode::ExitMode();
}

void FTwoBoneIKEditMode::Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
{
	USkeletalMeshComponent* SkelMeshComp = GetAnimPreviewScene().GetPreviewMeshComponent();
	if (SkelMeshComp && SkelMeshComp->SkeletalMesh && SkelMeshComp->SkeletalMesh->Skeleton)
	{
		USkeleton* Skeleton = SkelMeshComp->SkeletalMesh->Skeleton;

		DrawTargetLocation(PDI, SkelMeshComp, Skeleton, TwoBoneIKGraphNode->Node.EffectorLocationSpace, TwoBoneIKGraphNode->Node.EffectorSpaceBoneName, TwoBoneIKGraphNode->Node.EffectorLocation, FColor(255, 128, 128), FColor(180, 128, 128));
		DrawTargetLocation(PDI, SkelMeshComp, Skeleton, TwoBoneIKGraphNode->Node.JointTargetLocationSpace, TwoBoneIKGraphNode->Node.JointTargetSpaceBoneName, TwoBoneIKGraphNode->Node.JointTargetLocation, FColor(128, 255, 128), FColor(128, 180, 128));
	}
}

void FTwoBoneIKEditMode::DrawTargetLocation(FPrimitiveDrawInterface* PDI, USkeletalMeshComponent* SkelComp, USkeleton * Skeleton, EBoneControlSpace SpaceBase, FName SpaceBoneName, const FVector& TargetLocation, const FColor& TargetColor, const FColor& BoneColor) const
{
	const bool bInBoneSpace = (SpaceBase == BCS_ParentBoneSpace) || (SpaceBase == BCS_BoneSpace);
	const int32 SpaceBoneIndex = bInBoneSpace ? Skeleton->GetReferenceSkeleton().FindBoneIndex(SpaceBoneName) : INDEX_NONE;
	// Transform EffectorLocation from EffectorLocationSpace to ComponentSpace.
	FTransform TargetTransform = FTransform (TargetLocation);
	FTransform CSTransform;

	ConvertToComponentSpaceTransform(SkelComp, TargetTransform, CSTransform, SpaceBoneIndex, SpaceBase);

	FTransform WorldTransform = CSTransform * SkelComp->ComponentToWorld;

	if (bInBoneSpace)
	{
		ConvertToComponentSpaceTransform(SkelComp, FTransform::Identity, CSTransform, SpaceBoneIndex, SpaceBase);
		WorldTransform = CSTransform * SkelComp->ComponentToWorld;
		DrawCoordinateSystem( PDI, WorldTransform.GetLocation(), WorldTransform.GetRotation().Rotator(), 20.f, SDPG_Foreground );
		DrawWireDiamond( PDI, WorldTransform.ToMatrixWithScale(), 2.f, BoneColor, SDPG_Foreground );
	}
	else
	{
		DrawCoordinateSystem(PDI, WorldTransform.GetLocation(), WorldTransform.GetRotation().Rotator(), 20.f, SDPG_Foreground);
		DrawWireDiamond(PDI, WorldTransform.ToMatrixWithScale(), 2.f, TargetColor, SDPG_Foreground);
	}
}

FVector FTwoBoneIKEditMode::GetWidgetLocation() const
{
	EBoneControlSpace Space;
	FName BoneName;
	FVector Location;
	 
	if (BoneSelectMode == BSM_EndEffector)
	{
		Space = TwoBoneIKRuntimeNode->EffectorLocationSpace;
		Location = TwoBoneIKRuntimeNode->EffectorLocation;
		BoneName = TwoBoneIKRuntimeNode->EffectorSpaceBoneName;

	}
	else // BSM_JointTarget
	{
		Space = TwoBoneIKGraphNode->Node.JointTargetLocationSpace;

		if (Space == BCS_WorldSpace || Space == BCS_ComponentSpace)
		{
			return FVector::ZeroVector;
		}
		Location = TwoBoneIKRuntimeNode->JointTargetLocation;
		BoneName = TwoBoneIKRuntimeNode->JointTargetSpaceBoneName;
	}

	USkeletalMeshComponent* SkelComp = GetAnimPreviewScene().GetPreviewMeshComponent();
	return ConvertWidgetLocation(SkelComp, TwoBoneIKRuntimeNode->ForwardedPose, BoneName, Location, Space);
}

FWidget::EWidgetMode FTwoBoneIKEditMode::GetWidgetMode() const
{
	int32 BoneIndex = GetAnimPreviewScene().GetPreviewMeshComponent()->GetBoneIndex(TwoBoneIKGraphNode->Node.IKBone.BoneName);
	// Two bone IK node uses only Translate
	if (BoneIndex != INDEX_NONE)
	{
		return FWidget::WM_Translate;
	}

	return FWidget::WM_None;
}

FName FTwoBoneIKEditMode::GetSelectedBone() const
{
	FName SelectedBone;

	// should return mesh bone index
	if (BoneSelectMode == BSM_EndEffector)
	{
		if (TwoBoneIKGraphNode->Node.EffectorLocationSpace == EBoneControlSpace::BCS_BoneSpace ||
			TwoBoneIKGraphNode->Node.EffectorLocationSpace == EBoneControlSpace::BCS_ParentBoneSpace)
		{
			SelectedBone = TwoBoneIKGraphNode->Node.EffectorSpaceBoneName;
		}
		else
		{
			SelectedBone = TwoBoneIKGraphNode->Node.IKBone.BoneName;
		}

	}
	else if (BoneSelectMode == BSM_JointTarget)
	{
		SelectedBone = TwoBoneIKGraphNode->Node.JointTargetSpaceBoneName;
	}

	return SelectedBone;
}

bool FTwoBoneIKEditMode::HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click)
{
	bool bResult = FAnimNodeEditMode::HandleClick(InViewportClient, HitProxy, Click);

	if (HitProxy != nullptr && HitProxy->IsA(HActor::StaticGetType()))
	{
		HActor* ActorHitProxy = static_cast<HActor*>(HitProxy);
		ABoneSelectActor* BoneActor = Cast<ABoneSelectActor>(ActorHitProxy->Actor);

		if (BoneActor)
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

			bResult = true;
		}
	}

	return bResult;
}

void FTwoBoneIKEditMode::Tick(FEditorViewportClient* ViewportClient, float DeltaTime)
{
	FAnimNodeEditMode::Tick(ViewportClient, DeltaTime);

	ManageBoneSelectActor();

	USkeletalMeshComponent* SkelComp = GetAnimPreviewScene().GetPreviewMeshComponent();

	if (BoneSelectActor.IsValid())
	{
		// move the actor's position
		if (BoneSelectMode == BSM_JointTarget)
		{
			FName BoneName;
			int32 EffectorBoneIndex = SkelComp->GetBoneIndex(TwoBoneIKGraphNode->Node.EffectorSpaceBoneName);
			if (EffectorBoneIndex != INDEX_NONE)
			{
				BoneName = TwoBoneIKGraphNode->Node.EffectorSpaceBoneName;
			}
			else
			{
				BoneName = TwoBoneIKGraphNode->Node.IKBone.BoneName;
			}

			FVector ActorLocation = ConvertWidgetLocation(SkelComp, TwoBoneIKRuntimeNode->ForwardedPose, BoneName, TwoBoneIKGraphNode->Node.EffectorLocation, TwoBoneIKGraphNode->Node.EffectorLocationSpace);
			BoneSelectActor->SetActorLocation(ActorLocation + FVector(0, 10, 0));

		}
		else if (BoneSelectMode == BSM_EndEffector)
		{
			int32 JointTargetIndex = SkelComp->GetBoneIndex(TwoBoneIKGraphNode->Node.JointTargetSpaceBoneName);

			if (JointTargetIndex != INDEX_NONE)
			{
				FVector ActorLocation = ConvertWidgetLocation(SkelComp, TwoBoneIKRuntimeNode->ForwardedPose, TwoBoneIKGraphNode->Node.JointTargetSpaceBoneName, TwoBoneIKGraphNode->Node.JointTargetLocation, TwoBoneIKGraphNode->Node.JointTargetLocationSpace);
				BoneSelectActor->SetActorLocation(ActorLocation + FVector(0, 10, 0));
			}
		}
	}

	// Cache the current bone space
	PreviousBoneSpace = TwoBoneIKGraphNode->Node.EffectorLocationSpace;
}

void FTwoBoneIKEditMode::OnExternalNodePropertyChange(FPropertyChangedEvent& InPropertyEvent)
{
	USkeletalMeshComponent* SkelComponent = GetAnimPreviewScene().GetPreviewMeshComponent();

	if(!SkelComponent)
	{
		// Can't do anything below without the component
		return;
	}

	UProperty* Property = InPropertyEvent.Property;
	UProperty* InnerProperty = InPropertyEvent.MemberProperty;
	
	if(InnerProperty && InnerProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UAnimGraphNode_TwoBoneIK, Node))
	{
		if(Property && Property->GetFName() == GET_MEMBER_NAME_CHECKED(FAnimNode_TwoBoneIK, EffectorLocationSpace))
		{
			if(TwoBoneIKGraphNode->Node.EffectorLocationSpace == BCS_BoneSpace || TwoBoneIKGraphNode->Node.EffectorLocationSpace == BCS_ParentBoneSpace)
			{
				// Moving to bone space, to actually do this we would need the transform at the node before the
				// one we're editing... We can't do that so just reset the transform.
				TwoBoneIKRuntimeNode->EffectorLocationSpace = TwoBoneIKGraphNode->Node.EffectorLocationSpace;
				TwoBoneIKRuntimeNode->EffectorLocation = FVector::ZeroVector;
				TwoBoneIKGraphNode->SetDefaultValue(GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_TwoBoneIK, EffectorLocation), TwoBoneIKRuntimeNode->EffectorLocation);
			}
			else
			{
				// Need to convert our transform into world/component space
				FTransform CurrentTransform = FTransform(GetWidgetLocation());
				FTransform NewBSTransform;

				int32 SelectedBoneIndex = SkelComponent->SkeletalMesh->Skeleton->GetReferenceSkeleton().FindBoneIndex(TwoBoneIKGraphNode->Node.EffectorSpaceBoneName);

				ConvertToBoneSpaceTransform(SkelComponent, CurrentTransform, NewBSTransform, SelectedBoneIndex, TwoBoneIKGraphNode->Node.EffectorLocationSpace);

				TwoBoneIKRuntimeNode->EffectorLocationSpace = TwoBoneIKGraphNode->Node.EffectorLocationSpace;
				TwoBoneIKRuntimeNode->EffectorLocation = NewBSTransform.GetLocation();

				TwoBoneIKGraphNode->SetDefaultValue(GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_TwoBoneIK, EffectorLocation), TwoBoneIKRuntimeNode->EffectorLocation);
			}
		}
	}
}

void FTwoBoneIKEditMode::DoTranslation(FVector& InTranslation)
{
	USkeletalMeshComponent* SkelComp = GetAnimPreviewScene().GetPreviewMeshComponent();

	if (BoneSelectMode == BSM_EndEffector)
	{
		FVector Offset = ConvertCSVectorToBoneSpace(SkelComp, InTranslation, TwoBoneIKRuntimeNode->ForwardedPose, GetSelectedBone(), TwoBoneIKGraphNode->Node.EffectorLocationSpace);

		TwoBoneIKRuntimeNode->EffectorLocation += Offset;
		TwoBoneIKGraphNode->Node.EffectorLocation = TwoBoneIKRuntimeNode->EffectorLocation;
		TwoBoneIKGraphNode->SetDefaultValue(GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_TwoBoneIK, EffectorLocation), TwoBoneIKRuntimeNode->EffectorLocation);
	}
	else if (BoneSelectMode == BSM_JointTarget)
	{
		FVector Offset = ConvertCSVectorToBoneSpace(SkelComp, InTranslation, TwoBoneIKRuntimeNode->ForwardedPose, GetSelectedBone(), TwoBoneIKGraphNode->Node.JointTargetLocationSpace);

		TwoBoneIKRuntimeNode->JointTargetLocation += Offset;
		TwoBoneIKGraphNode->Node.JointTargetLocation = TwoBoneIKRuntimeNode->JointTargetLocation;
		TwoBoneIKGraphNode->SetDefaultValue(GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_TwoBoneIK, JointTargetLocation), TwoBoneIKRuntimeNode->JointTargetLocation);
	}
}

void FTwoBoneIKEditMode::ManageBoneSelectActor()
{
	USkeletalMeshComponent* SkelComp = GetAnimPreviewScene().GetPreviewMeshComponent();
	USkeleton* Skeleton = SkelComp->SkeletalMesh->Skeleton;

	// create a bone select actor if required
	int32 JointTargetIndex = Skeleton->GetReferenceSkeleton().FindBoneIndex(TwoBoneIKGraphNode->Node.JointTargetSpaceBoneName);

	const bool bNeedsActor = JointTargetIndex != INDEX_NONE &&
							(TwoBoneIKGraphNode->Node.JointTargetLocationSpace == EBoneControlSpace::BCS_BoneSpace ||
							 TwoBoneIKGraphNode->Node.JointTargetLocationSpace == EBoneControlSpace::BCS_ParentBoneSpace);

	if (bNeedsActor)
	{
		if (!BoneSelectActor.IsValid())
		{
			UWorld* World = SkelComp->GetWorld();
			check(World);

			BoneSelectActor = World->SpawnActor<ABoneSelectActor>(FVector(0), FRotator(0, 0, 0));
			check(BoneSelectActor.IsValid());
		}
	}
	else if(BoneSelectActor.IsValid())
	{
		if (SkelComp->GetWorld()->DestroyActor(BoneSelectActor.Get()))
		{
			BoneSelectActor = nullptr;
		}
	}
}
