// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#include "UnrealEd.h"
#include "SkeletalRenderPublic.h"
#include "AnimationRuntime.h"
#include "AnimPreviewInstance.h"
#include "Animation/AnimComposite.h"
#include "Animation/AnimInstance.h"
#include "Animation/BlendSpace.h"

//////////////////////////////////////////////////////////////////////////
// FDebugSkelMeshSceneProxy

/**
* A skeletal mesh component scene proxy with additional debugging options.
*/
class FDebugSkelMeshSceneProxy : public FSkeletalMeshSceneProxy
{
private:
	/** Holds onto the skeletal mesh component that created it so it can handle the rendering of bones properly. */
	const UDebugSkelMeshComponent* SkeletalMeshComponent;
public:
	/** 
	* Constructor. 
	* @param	Component - skeletal mesh primitive being added
	*/
	FDebugSkelMeshSceneProxy(const UDebugSkelMeshComponent* InComponent, FSkeletalMeshResource* InSkelMeshResource, const FColor& InWireframeOverlayColor = FColor::White) :
		FSkeletalMeshSceneProxy( InComponent, InSkelMeshResource )
	{
		SkeletalMeshComponent = InComponent;
		WireframeColor = FLinearColor(InWireframeOverlayColor);
	}

	virtual ~FDebugSkelMeshSceneProxy()
	{
	}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
	{
		if (SkeletalMeshComponent->bDrawMesh)
		{
			// We don't want to draw the mesh geometry to the hit testing render target
			// so that we can get to triangle strips that are partially obscured by other
			// triangle strips easier.
			GetMeshElementsConditionallySelectable(Views, ViewFamily, /*bSelectable=*/false, VisibilityMap, Collector);
		}

		//@todo - the rendering thread should never read from UObjects directly!  These are race conditions, the properties should be mirrored on the proxy
		if( SkeletalMeshComponent->MeshObject && (SkeletalMeshComponent->bDrawNormals || SkeletalMeshComponent->bDrawTangents || SkeletalMeshComponent->bDrawBinormals) )
		{
			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
			{
				if (VisibilityMap & (1 << ViewIndex))
				{
					SkeletalMeshComponent->MeshObject->DrawVertexElements(Collector.GetPDI(ViewIndex), SkeletalMeshComponent->ComponentToWorld, SkeletalMeshComponent->bDrawNormals, SkeletalMeshComponent->bDrawTangents, SkeletalMeshComponent->bDrawBinormals);
				}
			}
		}
	}

	uint32 GetAllocatedSize() const
	{
		return FSkeletalMeshSceneProxy::GetAllocatedSize();
	}

	virtual uint32 GetMemoryFootprint() const override
	{
		return sizeof(*this) + GetAllocatedSize();
	}
};

//////////////////////////////////////////////////////////////////////////
// UDebugSkelMeshComponent

UDebugSkelMeshComponent::UDebugSkelMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bDrawMesh = true;
	PreviewInstance = NULL;
	bDisplayRawAnimation = false;
	bDisplayNonRetargetedPose = false;

	// wind is turned off in the editor by default
	bEnableWind = false;

	bMeshSocketsVisible = true;
	bSkeletonSocketsVisible = true;

	TurnTableSpeedScaling = 1.f;
	TurnTableMode = EPersonaTurnTableMode::Stopped;

#if WITH_APEX_CLOTHING
	SectionsDisplayMode = ESectionDisplayMode::None;
	// always shows cloth morph target when previewing in editor
	bClothMorphTarget = false;
#endif //#if WITH_APEX_CLOTHING
}

FBoxSphereBounds UDebugSkelMeshComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	FBoxSphereBounds Result = Super::CalcBounds(LocalToWorld);

	if (! IsUsingInGameBounds())
	{
		// extend bounds by bones but without root bone
		FBox BoundingBox(0);
		const int32 NumBones = GetNumComponentSpaceTransforms();
		for (int32 BoneIndex = 1; BoneIndex < NumBones; ++BoneIndex)
		{
			BoundingBox += GetBoneMatrix(BoneIndex).GetOrigin();
		}
		Result = Result + FBoxSphereBounds(BoundingBox);
	}

	return Result;
}

bool UDebugSkelMeshComponent::IsUsingInGameBounds() const
{
	return bIsUsingInGameBounds;
}

void UDebugSkelMeshComponent::UseInGameBounds(bool bUseInGameBounds)
{
	bIsUsingInGameBounds = bUseInGameBounds;
}

bool UDebugSkelMeshComponent::CheckIfBoundsAreCorrrect()
{
	if (GetPhysicsAsset())
	{
		bool bWasUsingInGameBounds = IsUsingInGameBounds();
		FTransform TempTransform = FTransform::Identity;
		UseInGameBounds(true);
		FBoxSphereBounds InGameBounds = CalcBounds(TempTransform);
		UseInGameBounds(false);
		FBoxSphereBounds PreviewBounds = CalcBounds(TempTransform);
		UseInGameBounds(bWasUsingInGameBounds);
		// calculate again to have bounds as requested
		CalcBounds(TempTransform);
		// if in-game bounds are of almost same size as preview bounds or bigger, it seems to be fine
		if (! InGameBounds.GetSphere().IsInside(PreviewBounds.GetSphere(), PreviewBounds.GetSphere().W * 0.1f) && // for spheres: A.IsInside(B) checks if A is inside of B
			! PreviewBounds.GetBox().IsInside(InGameBounds.GetBox().ExpandBy(PreviewBounds.GetSphere().W * 0.1f))) // for boxes: A.IsInside(B) checks if B is inside of A
		{
			return true;
		}
	}
	return false;
}

float WrapInRange(float StartVal, float MinVal, float MaxVal)
{
	float Size = MaxVal - MinVal;
	float EndVal = StartVal;
	while (EndVal < MinVal)
	{
		EndVal += Size;
	}

	while (EndVal > MaxVal)
	{
		EndVal -= Size;
	}
	return EndVal;
}

void UDebugSkelMeshComponent::ConsumeRootMotion(const FVector& FloorMin, const FVector& FloorMax)
{
	//Extract root motion regardless of where we use it so that we don't hit
	//problems with it building up in the instance

	FRootMotionMovementParams ExtractedRootMotion;

	if (UAnimInstance* AnimInst = GetAnimInstance())
	{
		ExtractedRootMotion = AnimInst->ConsumeExtractedRootMotion(1.f);
	}

	if (bPreviewRootMotion)
	{
		if (ExtractedRootMotion.bHasRootMotion)
		{
			AddLocalTransform(ExtractedRootMotion.GetRootMotionTransform());

			//Handle moving component so that it stays within the editor floor
			FTransform CurrentTransform = GetRelativeTransform();
			FVector Trans = CurrentTransform.GetTranslation();
			Trans.X = WrapInRange(Trans.X, FloorMin.X, FloorMax.X);
			Trans.Y = WrapInRange(Trans.Y, FloorMin.Y, FloorMax.Y);
			CurrentTransform.SetTranslation(Trans);
			SetRelativeTransform(CurrentTransform);
		}
	}
	else
	{
		if (TurnTableMode == EPersonaTurnTableMode::Stopped)
		{
			SetWorldTransform(FTransform());
		}
		else
		{
			SetRelativeLocation(FVector::ZeroVector);
		}
	}
}

FPrimitiveSceneProxy* UDebugSkelMeshComponent::CreateSceneProxy()
{
	FDebugSkelMeshSceneProxy* Result = NULL;
	ERHIFeatureLevel::Type SceneFeatureLevel = GetWorld()->FeatureLevel;
	FSkeletalMeshResource* SkelMeshResource = SkeletalMesh ? SkeletalMesh->GetResourceForRendering() : NULL;

	// only create a scene proxy for rendering if
	// properly initialized
	if( SkelMeshResource && 
		SkelMeshResource->LODModels.IsValidIndex(PredictedLODLevel) &&
		!bHideSkin &&
		MeshObject)
	{
		const FColor WireframeMeshOverlayColor(102,205,170,255);
		Result = ::new FDebugSkelMeshSceneProxy(this, SkelMeshResource, WireframeMeshOverlayColor);
	}

#if WITH_APEX_CLOTHING
	if (SectionsDisplayMode == ESectionDisplayMode::None)
	{
		SectionsDisplayMode = FindCurrentSectionDisplayMode();
	}

#endif //#if WITH_APEX_CLOTHING

	return Result;
}


bool UDebugSkelMeshComponent::IsPreviewOn() const
{
	return (PreviewInstance != NULL) && (PreviewInstance == AnimScriptInstance);
}

FString UDebugSkelMeshComponent::GetPreviewText() const
{
#define LOCTEXT_NAMESPACE "SkelMeshComponent"

	if (IsPreviewOn())
	{
		UAnimationAsset* CurrentAsset = PreviewInstance->GetCurrentAsset();
		if (UBlendSpaceBase* BlendSpace = Cast<UBlendSpaceBase>(CurrentAsset))
		{
			return FText::Format( LOCTEXT("BlendSpace", "Blend Space {0}"), FText::FromString(BlendSpace->GetName()) ).ToString();
		}
		else if (UAnimMontage* Montage = Cast<UAnimMontage>(CurrentAsset))
		{
			return FText::Format( LOCTEXT("Montage", "Montage {0}"), FText::FromString(Montage->GetName()) ).ToString();
		}
		else if(UAnimComposite* Composite = Cast<UAnimComposite>(CurrentAsset))
		{
			return FText::Format(LOCTEXT("Composite", "Composite {0}"), FText::FromString(Composite->GetName())).ToString();
		}
		else if (UAnimSequence* Sequence = Cast<UAnimSequence>(CurrentAsset))
		{
			return FText::Format( LOCTEXT("Animation", "Animation {0}"), FText::FromString(Sequence->GetName()) ).ToString();
		}
	}

	return LOCTEXT("ReferencePose", "Reference Pose").ToString();

#undef LOCTEXT_NAMESPACE
}

void UDebugSkelMeshComponent::InitAnim(bool bForceReinit)
{
	// If we already have PreviewInstnace and its asset's Skeleton does not match with mesh's Skeleton
	// then we need to clear it up to avoid an issue
	if ( PreviewInstance && PreviewInstance->GetCurrentAsset() && SkeletalMesh )
	{
		if ( PreviewInstance->GetCurrentAsset()->GetSkeleton() != SkeletalMesh->Skeleton )
		{
			// if it doesn't match, just clear it
			PreviewInstance->SetAnimationAsset(NULL);
		}
	}

	Super::InitAnim(bForceReinit);

	// if PreviewInstance is NULL, create here once
	if (PreviewInstance == NULL)
	{
		PreviewInstance = NewObject<UAnimPreviewInstance>(this);
		check(PreviewInstance);

		//Set transactional flag in order to restore slider position when undo operation is performed
		PreviewInstance->SetFlags(RF_Transactional);
	}

	// if anim script instance is null because it's not playing a blueprint, set to PreviewInstnace by default
	// that way if user would like to modify bones or do extra stuff, it will work
	if (AnimScriptInstance == NULL)
	{
		AnimScriptInstance = PreviewInstance;
		AnimScriptInstance->InitializeAnimation();
	}
}

bool UDebugSkelMeshComponent::IsWindEnabled() const
{
	return bEnableWind;
}

void UDebugSkelMeshComponent::EnablePreview(bool bEnable, UAnimationAsset* PreviewAsset)
{
	if (PreviewInstance)
	{
		if (bEnable)
		{
			// back up current AnimInstance if not currently previewing anything
			if (!IsPreviewOn())
			{
				SavedAnimScriptInstance = AnimScriptInstance;
			}

			AnimScriptInstance = PreviewInstance;

#if WITH_APEX_CLOTHING
		    // turn off these options when playing animations because max distances / back stops don't have meaning while moving
		    bDisplayClothMaxDistances = false;
			bDisplayClothBackstops = false;
		    // restore previous state
		    bDisableClothSimulation = bPrevDisableClothSimulation;
#endif // #if WITH_APEX_CLOTHING
    
			PreviewInstance->SetAnimationAsset(PreviewAsset);
		}
		else if (IsPreviewOn())
		{
			if (PreviewInstance->GetCurrentAsset() == PreviewAsset || PreviewAsset == NULL)
			{
				// now recover to saved AnimScriptInstance;
				AnimScriptInstance = SavedAnimScriptInstance;
				PreviewInstance->SetAnimationAsset(nullptr);
			}
		}
	}
}

bool UDebugSkelMeshComponent::ShouldCPUSkin()
{
	return 	bCPUSkinning || bDrawBoneInfluences || bDrawNormals || bDrawTangents || bDrawBinormals;
}


void UDebugSkelMeshComponent::PostInitMeshObject(FSkeletalMeshObject* InMeshObject)
{
	Super::PostInitMeshObject( InMeshObject );

	if (InMeshObject)
	{
		if(bDrawBoneInfluences)
		{
			InMeshObject->EnableBlendWeightRendering(true, BonesOfInterest);
		}
	}
}

void UDebugSkelMeshComponent::SetShowBoneWeight(bool bNewShowBoneWeight)
{
	// Check we are actually changing it!
	if(bNewShowBoneWeight == bDrawBoneInfluences)
	{
		return;
	}

	// if turning on this mode
	if(bNewShowBoneWeight)
	{
		SkelMaterials.Empty();
		int32 NumMaterials = GetNumMaterials();
		for (int32 i=0; i<NumMaterials; i++)
		{
			// Back up old material
			SkelMaterials.Add(GetMaterial(i));
			// Set special bone weight material
			SetMaterial(i, GEngine->BoneWeightMaterial);
		}
	}
	// if turning it off
	else
	{
		int32 NumMaterials = GetNumMaterials();
		check(NumMaterials == SkelMaterials.Num());
		for (int32 i=0; i<NumMaterials; i++)
		{
			// restore original material
			SetMaterial(i, SkelMaterials[i]);
		}
	}

	bDrawBoneInfluences = bNewShowBoneWeight;
}

void UDebugSkelMeshComponent::GenSpaceBases(TArray<FTransform>& OutSpaceBases)
{
	TArray<FTransform> TempBoneSpaceTransforms;
	TempBoneSpaceTransforms.AddUninitialized(OutSpaceBases.Num());
	FVector TempRootBoneTranslation;
	FBlendedHeapCurve TempCurve;
	PreviewInstance->PrePerformAnimationEvaluation();
	PerformAnimationEvaluation(SkeletalMesh, AnimScriptInstance, OutSpaceBases, TempBoneSpaceTransforms, TempRootBoneTranslation, TempCurve);
	PreviewInstance->PostPerformAnimationEvaluation();
}

void UDebugSkelMeshComponent::RefreshBoneTransforms(FActorComponentTickFunction* TickFunction)
{
	// Run regular update first so we get RequiredBones up to date.
	Super::RefreshBoneTransforms(NULL); // Pass NULL so we force non threaded work

	const bool bIsPreviewInstance = (PreviewInstance && PreviewInstance == AnimScriptInstance);

	BakedAnimationPoses.Reset();
	if(bDisplayBakedAnimation && bIsPreviewInstance && PreviewInstance->GetRequiredBones().IsValid())
	{
		if(UAnimSequence* Sequence = Cast<UAnimSequence>(PreviewInstance->GetCurrentAsset()))
		{
			BakedAnimationPoses.AddUninitialized(PreviewInstance->GetRequiredBones().GetNumBones());
			bool bSavedUseSourceData = PreviewInstance->GetRequiredBones().ShouldUseSourceData();
			PreviewInstance->GetRequiredBones().SetUseRAWData(true);
			PreviewInstance->GetRequiredBones().SetUseSourceData(false);
			PreviewInstance->EnableControllers(false);
			GenSpaceBases(BakedAnimationPoses);
			PreviewInstance->GetRequiredBones().SetUseRAWData(false);
			PreviewInstance->GetRequiredBones().SetUseSourceData(bSavedUseSourceData);
			PreviewInstance->EnableControllers(true);
		}
	}

	SourceAnimationPoses.Reset();
	if(bDisplaySourceAnimation && bIsPreviewInstance && PreviewInstance->GetRequiredBones().IsValid())
	{
		if(UAnimSequence* Sequence = Cast<UAnimSequence>(PreviewInstance->GetCurrentAsset()))
		{
			SourceAnimationPoses.AddUninitialized(PreviewInstance->GetRequiredBones().GetNumBones());
			bool bSavedUseSourceData = PreviewInstance->GetRequiredBones().ShouldUseSourceData();
			PreviewInstance->GetRequiredBones().SetUseSourceData(true);
			PreviewInstance->EnableControllers(false);
			GenSpaceBases(SourceAnimationPoses);
			PreviewInstance->GetRequiredBones().SetUseSourceData(bSavedUseSourceData);
			PreviewInstance->EnableControllers(true);
		}
	}

	UncompressedSpaceBases.Reset();
	if (bDisplayRawAnimation && AnimScriptInstance && AnimScriptInstance->GetRequiredBones().IsValid())
	{
		UncompressedSpaceBases.AddUninitialized(AnimScriptInstance->GetRequiredBones().GetNumBones());

		AnimScriptInstance->GetRequiredBones().SetUseRAWData(true);
		GenSpaceBases(UncompressedSpaceBases);
		AnimScriptInstance->GetRequiredBones().SetUseRAWData(false);
	}

	// Non retargeted pose.
	NonRetargetedSpaceBases.Reset();
	if( bDisplayNonRetargetedPose && AnimScriptInstance && AnimScriptInstance->GetRequiredBones().IsValid() )
	{
		NonRetargetedSpaceBases.AddUninitialized(AnimScriptInstance->GetRequiredBones().GetNumBones());
		AnimScriptInstance->GetRequiredBones().SetDisableRetargeting(true);
		GenSpaceBases(NonRetargetedSpaceBases);
		AnimScriptInstance->GetRequiredBones().SetDisableRetargeting(false);
	}

	// Only works in PreviewInstance, and not for anim blueprint. This is intended.
	AdditiveBasePoses.Reset();
	if( bDisplayAdditiveBasePose && bIsPreviewInstance && PreviewInstance->GetRequiredBones().IsValid() )
	{
		if (UAnimSequence* Sequence = Cast<UAnimSequence>(PreviewInstance->GetCurrentAsset())) 
		{ 
			if (Sequence->IsValidAdditive()) 
			{ 
				FCSPose<FCompactPose> CSAdditiveBasePose;
				{
					FCompactPose AdditiveBasePose;
					FBlendedCurve AdditiveCurve;
					AdditiveCurve.InitFrom(AnimScriptInstance->GetSkelMeshComponent()->GetCachedAnimCurveMappingNameUids());
					AdditiveBasePose.SetBoneContainer(&PreviewInstance->GetRequiredBones());
					Sequence->GetAdditiveBasePose(AdditiveBasePose, AdditiveCurve, FAnimExtractContext(PreviewInstance->GetCurrentTime()));
					CSAdditiveBasePose.InitPose(AdditiveBasePose);
				}

				FBoneContainer& BoneContainer = PreviewInstance->GetRequiredBones();
				const int32 NumSkeletonBones = BoneContainer.GetNumBones();

				AdditiveBasePoses.AddUninitialized(NumSkeletonBones);

				for (int32 i = 0; i < AdditiveBasePoses.Num(); ++i)
				{
					FCompactPoseBoneIndex CompactIndex = BoneContainer.MakeCompactPoseIndex(FMeshPoseBoneIndex(i));

					// AdditiveBasePoses has one entry for every bone in the asset ref skeleton - if we're on a LOD
					// we need to check this is actually valid for the current pose.
					if(CSAdditiveBasePose.GetPose().IsValidIndex(CompactIndex))
					{
						AdditiveBasePoses[i] = CSAdditiveBasePose.GetComponentSpaceTransform(CompactIndex);
					}
					else
					{
						AdditiveBasePoses[i] = FTransform::Identity;
					}
				}
			}
		}
	}
}

#if WITH_EDITOR
void UDebugSkelMeshComponent::ReportAnimNotifyError(const FText& Error, UObject* InSourceNotify)
{
	for (FAnimNotifyErrors& Errors : AnimNotifyErrors)
	{
		if (Errors.SourceNotify == InSourceNotify)
		{
			Errors.Errors.Add(Error.ToString());
			return;
		}
	}

	int32 i = AnimNotifyErrors.Num();
	AnimNotifyErrors.Add(FAnimNotifyErrors(InSourceNotify));
	AnimNotifyErrors[i].Errors.Add(Error.ToString());
}

void UDebugSkelMeshComponent::ClearAnimNotifyErrors(UObject* InSourceNotify)
{
	for (FAnimNotifyErrors& Errors : AnimNotifyErrors)
	{
		if (Errors.SourceNotify == InSourceNotify)
		{
			Errors.Errors.Empty();
		}
	}
}
#endif

#if WITH_APEX_CLOTHING

void UDebugSkelMeshComponent::ToggleClothSectionsVisibility(bool bShowOnlyClothSections)
{
	FSkeletalMeshResource* SkelMeshResource = GetSkeletalMeshResource();
	if (SkelMeshResource)
	{
		PreEditChange(NULL);

		for (int32 LODIndex = 0; LODIndex < SkelMeshResource->LODModels.Num(); LODIndex++)
		{
			FStaticLODModel& LODModel = SkelMeshResource->LODModels[LODIndex];

			for (int32 SecIdx = 0; SecIdx < LODModel.Sections.Num(); SecIdx++)
			{
				FSkelMeshSection& Section = LODModel.Sections[SecIdx];

				// toggle visibility between cloth sections and non-cloth sections
				if (bShowOnlyClothSections)
				{
					// enables only cloth sections
					if (Section.HasApexClothData())
					{
						Section.bDisabled = false;
					}
					else
					{
						Section.bDisabled = true;
					}
				}
				else
				{   // disables cloth sections and also corresponding original sections
					if (Section.HasApexClothData())
					{
						Section.bDisabled = true;
						LODModel.Sections[Section.CorrespondClothSectionIndex].bDisabled = true;
					}
					else
					{
						Section.bDisabled = false;
					}
				}
			}
		}
		PostEditChange();
	}
}

void UDebugSkelMeshComponent::RestoreClothSectionsVisibility()
{
	// if this skeletal mesh doesn't have any clothing assets, just return
	if (!SkeletalMesh || SkeletalMesh->ClothingAssets.Num() == 0)
	{
		return;
	}

	FSkeletalMeshResource* SkelMeshResource = GetSkeletalMeshResource();
	if (SkelMeshResource)
	{
		PreEditChange(NULL);

		for(int32 LODIndex = 0; LODIndex < SkelMeshResource->LODModels.Num(); LODIndex++)
		{
			FStaticLODModel& LODModel = SkelMeshResource->LODModels[LODIndex];

			// enables all sections first
			for(int32 SecIdx = 0; SecIdx < LODModel.Sections.Num(); SecIdx++)
			{
				LODModel.Sections[SecIdx].bDisabled = false;
			}

			// disables corresponding original section to enable the cloth section instead
			for(int32 SecIdx = 0; SecIdx < LODModel.Sections.Num(); SecIdx++)
			{
				FSkelMeshSection& Section = LODModel.Sections[SecIdx];

				if(Section.HasApexClothData())
				{
					LODModel.Sections[Section.CorrespondClothSectionIndex].bDisabled = true;
				}
			}
		}

		PostEditChange();
	}
}

int32 UDebugSkelMeshComponent::FindCurrentSectionDisplayMode()
{
	ESectionDisplayMode DisplayMode = ESectionDisplayMode::None;

	FSkeletalMeshResource* SkelMeshResource = GetSkeletalMeshResource();
	// if this skeletal mesh doesn't have any clothing asset, returns "None"
	if (!SkelMeshResource || !SkeletalMesh || SkeletalMesh->ClothingAssets.Num() == 0)
	{
		return ESectionDisplayMode::None;
	}
	else
	{
		int32 LODIndex;
		int32 NumLODs = SkelMeshResource->LODModels.Num();
		for (LODIndex = 0; LODIndex < NumLODs; LODIndex++)
		{
			// if find any LOD model which has cloth data, then break
			if (SkelMeshResource->LODModels[LODIndex].HasApexClothData())
			{
				break;
			}
		}

		// couldn't find 
		if (LODIndex == NumLODs)
		{
			return ESectionDisplayMode::None;
		}

		FStaticLODModel& LODModel = SkelMeshResource->LODModels[LODIndex];

		// firstly, find cloth sections
		for (int32 SecIdx = 0; SecIdx < LODModel.Sections.Num(); SecIdx++)
		{
			FSkelMeshSection& Section = LODModel.Sections[SecIdx];

			if (Section.HasApexClothData())
			{
				// Normal state if the cloth section is visible and the corresponding section is disabled
				if (Section.bDisabled == false &&
					LODModel.Sections[Section.CorrespondClothSectionIndex].bDisabled == true)
				{
					DisplayMode = ESectionDisplayMode::ShowOnlyClothSections;
					break;
				}
			}
		}

		// secondly, find non-cloth sections except cloth-corresponding sections
		bool bFoundNonClothSection = false;

		for (int32 SecIdx = 0; SecIdx < LODModel.Sections.Num(); SecIdx++)
		{
			FSkelMeshSection& Section = LODModel.Sections[SecIdx];

			// not related to cloth sections
			if (!Section.HasApexClothData() &&
				Section.CorrespondClothSectionIndex < 0)
			{
				bFoundNonClothSection = true;
				if (!Section.bDisabled)
				{
					if (DisplayMode == ESectionDisplayMode::ShowOnlyClothSections)
					{
						DisplayMode = ESectionDisplayMode::ShowAll;
					}
					else
					{
						DisplayMode = ESectionDisplayMode::HideOnlyClothSections;
					}
				}
				break;
			}
		}
	}

	return DisplayMode;
}

void UDebugSkelMeshComponent::CheckClothTeleport()
{
	// do nothing to avoid clothing reset while modifying properties
	// modifying values can cause frame delay and clothes will be reset by a large delta time (low fps)
	// doesn't need cloth teleport while previewing
}

#endif // #if WITH_APEX_CLOTHING

void UDebugSkelMeshComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	if (TurnTableMode == EPersonaTurnTableMode::Playing)
	{
		FRotator Rotation = GetRelativeTransform().Rotator();
		// Take into account time dilation, so it doesn't affect turn table turn rate.
		float CurrentTimeDilation = 1.0f;
		if (UWorld* MyWorld = GetWorld())
		{
			CurrentTimeDilation = MyWorld->GetWorldSettings()->GetEffectiveTimeDilation();
		}
		Rotation.Yaw += 36.f * TurnTableSpeedScaling * DeltaTime / FMath::Max(CurrentTimeDilation, KINDA_SMALL_NUMBER);
		SetRelativeRotation(Rotation);
	}

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}
