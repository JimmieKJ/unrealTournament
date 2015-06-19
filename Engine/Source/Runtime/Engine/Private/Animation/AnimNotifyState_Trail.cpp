// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Animation/AnimNotifies/AnimNotifyState_Trail.h"
#include "ParticleDefinitions.h"
#include "Particles/TypeData/ParticleModuleTypeDataAnimTrail.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "MessageLog.h"
#include "AssertionMacros.h"
#include "Animation/AnimInstance.h"

#define LOCTEXT_NAMESPACE "AnimNotifyState_Trail"

DEFINE_LOG_CATEGORY(LogAnimTrails);

/////////////////////////////////////////////////////
// UAnimNotifyState_Trail

UAnimNotifyState_Trail::UAnimNotifyState_Trail(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PSTemplate = NULL;
	FirstSocketName = NAME_None;
	SecondSocketName = NAME_None;
	WidthScaleMode = ETrailWidthMode_FromCentre;
	WidthScaleCurve = NAME_None;

#if WITH_EDITORONLY_DATA
	bRenderGeometry = true;
	bRenderSpawnPoints = false;
	bRenderTangents = false;
	bRenderTessellation = false;
#endif // WITH_EDITORONLY_DATA
}

void UAnimNotifyState_Trail::NotifyBegin(class USkeletalMeshComponent * MeshComp, class UAnimSequenceBase * Animation, float TotalDuration)
{
	bool bError = ValidateInput(MeshComp);

	TArray<USceneComponent*> Children;
	MeshComp->GetChildrenComponents(false, Children);
	
	UAnimInstance* AnimInst = MeshComp->GetAnimInstance();
	float Width = 1.0f;
	if (WidthScaleCurve != NAME_None && AnimInst)
	{
		Width = AnimInst->GetCurveValue(WidthScaleCurve);
	}

	UParticleSystem* ParticleSystemTemplate = OverridePSTemplate(MeshComp, Animation);
	if (ParticleSystemTemplate == nullptr)
	{
		ParticleSystemTemplate = PSTemplate;
	}

	bool bFoundExistingTrail = false;
	for (USceneComponent* Comp : Children)
	{
		UParticleSystemComponent* ParticleComp = Cast<UParticleSystemComponent>(Comp);
		if (ParticleComp)
		{
			TArray< FParticleAnimTrailEmitterInstance* > TrailEmitters;
			ParticleComp->GetOwnedTrailEmitters(TrailEmitters, this, false);

			bFoundExistingTrail = TrailEmitters.Num() > 0;

			//If there are any trails, ensure the template hasn't been changed. Also destroy the component if there are errors.
			if (TrailEmitters.Num() > 0 && (bError || PSTemplate != ParticleComp->Template))
			{
				//The PSTemplate was changed so we need to destroy this system and create it again with the new template. May be able to just change the template?
				ParticleComp->DestroyComponent();
			}
			else
			{
				for (FParticleAnimTrailEmitterInstance* Trail : TrailEmitters)
				{
					Trail->BeginTrail();
					Trail->SetTrailSourceData(FirstSocketName, SecondSocketName, WidthScaleMode, Width);

#if WITH_EDITORONLY_DATA
					Trail->SetTrailDebugData( bRenderGeometry, bRenderSpawnPoints, bRenderTessellation, bRenderTangents);
#endif
				}
			}

			if (bFoundExistingTrail)
			{
				break;//Found and re-used the existing trail so can exit now.
			}
		}
	}

	if (!bFoundExistingTrail && !bError)
	{
		//Spawn a new component from PSTemplate. This notify is made the outer so that the component can be identified later.
		UParticleSystemComponent* NewParticleComp = NewObject<UParticleSystemComponent>(MeshComp);
		NewParticleComp->bAutoDestroy = true;
		NewParticleComp->SecondsBeforeInactive = 0.0f;
		NewParticleComp->bAutoActivate = false;
		NewParticleComp->SetTemplate(PSTemplate);
		NewParticleComp->bOverrideLODMethod = false;

		NewParticleComp->RegisterComponentWithWorld(MeshComp->GetWorld());
		NewParticleComp->AttachTo(MeshComp, NAME_None);
		//NewParticleComp->SetWorldLocationAndRotation(Location, Rotation);

		NewParticleComp->SetRelativeScale3D(FVector(1.f));
		NewParticleComp->ActivateSystem(true);

		TArray< FParticleAnimTrailEmitterInstance* > TrailEmitters;
		NewParticleComp->GetOwnedTrailEmitters(TrailEmitters, this, true);

		for (FParticleAnimTrailEmitterInstance* Trail : TrailEmitters)
		{
			Trail->BeginTrail();
			Trail->SetTrailSourceData(FirstSocketName, SecondSocketName, WidthScaleMode, Width);

#if WITH_EDITORONLY_DATA
			Trail->SetTrailDebugData(bRenderGeometry, bRenderSpawnPoints, bRenderTessellation, bRenderTangents);
#endif
		}
	}

	Received_NotifyBegin(MeshComp, Animation, TotalDuration);
}

void UAnimNotifyState_Trail::NotifyTick(class USkeletalMeshComponent * MeshComp, class UAnimSequenceBase * Animation, float FrameDeltaTime)
{
	bool bError = ValidateInput(MeshComp, true);

	TArray<USceneComponent*> Children;
	MeshComp->GetChildrenComponents(false, Children);

	UAnimInstance* AnimInst = MeshComp->GetAnimInstance();
	float Width = 1.0f;
	if (WidthScaleCurve != NAME_None && AnimInst)
	{
		Width = AnimInst->GetCurveValue(WidthScaleCurve);
	}

	for (USceneComponent* Comp : Children)
	{
		UParticleSystemComponent* ParticleComp = Cast<UParticleSystemComponent>(Comp);
		if ( ParticleComp )
		{
			TArray< FParticleAnimTrailEmitterInstance* > TrailEmitters;
			ParticleComp->GetOwnedTrailEmitters(TrailEmitters, this, false);
			if (bError && TrailEmitters.Num() > 0)
			{
				Comp->DestroyComponent();
			}
			else
			{
				for (FParticleAnimTrailEmitterInstance* Trail : TrailEmitters)
				{
					Trail->SetTrailSourceData(FirstSocketName, SecondSocketName, WidthScaleMode, Width);

#if WITH_EDITORONLY_DATA
					Trail->SetTrailDebugData(bRenderGeometry, bRenderSpawnPoints, bRenderTessellation, bRenderTangents);
#endif
				}
			}
		}
	}

	Received_NotifyTick(MeshComp, Animation, FrameDeltaTime);
}

void UAnimNotifyState_Trail::NotifyEnd(class USkeletalMeshComponent * MeshComp, class UAnimSequenceBase * Animation)
{
	TArray<USceneComponent*> Children;
	MeshComp->GetChildrenComponents(false, Children);

	for (USceneComponent* Comp : Children)
	{
		UParticleSystemComponent* ParticleComp = Cast<UParticleSystemComponent>(Comp);
		if (ParticleComp)
		{
			TArray< FParticleAnimTrailEmitterInstance* > TrailEmitters;
			ParticleComp->GetOwnedTrailEmitters(TrailEmitters, this, false);
			for (FParticleAnimTrailEmitterInstance* Trail : TrailEmitters)
			{
				Trail->EndTrail();
			}
		}
	}

	Received_NotifyEnd(MeshComp, Animation);
}

bool UAnimNotifyState_Trail::ValidateInput(class USkeletalMeshComponent * MeshComp, bool bReportErrors/* =false */)
{
#if WITH_EDITOR
	bool bError = false;
	FText FirstSocketEqualsNoneErrorText = FText::Format( LOCTEXT("NoneFirstSocket", "{0}: Must set First Socket Name."), FText::FromString(GetName()));
	FText SecondSocketEqualsNoneErrorText = FText::Format( LOCTEXT("NoneSecondSocket", "{0}: Must set Second Socket Name."), FText::FromString(GetName()));
	FText PSTemplateEqualsNoneErrorText = FText::Format( LOCTEXT("NonePSTemplate", "{0}: Trail must have a PSTemplate."), FText::FromString(GetName()));
	FString PSTemplateName = PSTemplate ? PSTemplate->GetName() : "";
	FText PSTemplateInvalidErrorText = FText::Format(LOCTEXT("InvalidPSTemplateFmt", "{0}: {1} does not contain any trail emittter."), FText::FromString(GetName()), FText::FromString(PSTemplateName));

	MeshComp->ClearAnimNotifyErrors(this);

	//Validate the user input and report any errors.
	if (FirstSocketName == NAME_None)
	{
		if (bReportErrors)
		{
			MeshComp->ReportAnimNotifyError(FirstSocketEqualsNoneErrorText, this);
		}
		bError = true;
	}

	if (SecondSocketName == NAME_None)
	{
		if (bReportErrors)
		{
			MeshComp->ReportAnimNotifyError(SecondSocketEqualsNoneErrorText, this);
		}
		bError = true;
	}

	if (!PSTemplate)
	{
		if (bReportErrors)
		{
			MeshComp->ReportAnimNotifyError(PSTemplateEqualsNoneErrorText, this);
		}
		bError = true;
	}
	else
	{
		if (!PSTemplate->ContainsEmitterType(UParticleModuleTypeDataAnimTrail::StaticClass()))
		{
			if (bReportErrors)
			{
				MeshComp->ReportAnimNotifyError(PSTemplateInvalidErrorText, this);
			}

			bError = true;
		}
	}
	return bError;
#else
	return false;
#endif // WITH_EDITOR
}

#undef  LOCTEXT_NAMESPACE