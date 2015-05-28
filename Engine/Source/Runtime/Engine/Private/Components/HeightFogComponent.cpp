// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	HeightFogComponent.cpp: Height fog implementation.
=============================================================================*/

#include "EnginePrivate.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Net/UnrealNetwork.h"

UExponentialHeightFogComponent::UExponentialHeightFogComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	FogInscatteringColor = FLinearColor(0.447f, 0.638f, 1.0f);

	DirectionalInscatteringExponent = 4.0f;
	DirectionalInscatteringStartDistance = 10000.0f;
	DirectionalInscatteringColor = FLinearColor(0.25f, 0.25f, 0.125f);

	FogDensity = 0.02f;
	FogHeightFalloff = 0.2f;
	FogMaxOpacity = 1.0f;
	StartDistance = 0.0f;
}

void UExponentialHeightFogComponent::AddFogIfNeeded()
{
	if (ShouldComponentAddToScene() && ShouldRender() && IsRegistered() && FogDensity * 1000 > DELTA && FogMaxOpacity > DELTA
		&& (GetOuter() == NULL || !GetOuter()->HasAnyFlags(RF_ClassDefaultObject)))
	{
		World->Scene->AddExponentialHeightFog(this);
	}
}

void UExponentialHeightFogComponent::CreateRenderState_Concurrent()
{
	Super::CreateRenderState_Concurrent();
	AddFogIfNeeded();
}

void UExponentialHeightFogComponent::SendRenderTransform_Concurrent()
{
	World->Scene->RemoveExponentialHeightFog(this);
	AddFogIfNeeded();
	Super::SendRenderTransform_Concurrent();
}

void UExponentialHeightFogComponent::DestroyRenderState_Concurrent()
{
	Super::DestroyRenderState_Concurrent();
	World->Scene->RemoveExponentialHeightFog(this);
}

#if WITH_EDITOR
void UExponentialHeightFogComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	FogDensity = FMath::Clamp(FogDensity, 0.0f, 10.0f);
	FogHeightFalloff = FMath::Clamp(FogHeightFalloff, 0.0f, 2.0f);
	FogMaxOpacity = FMath::Clamp(FogMaxOpacity, 0.0f, 1.0f);
	StartDistance = FMath::Clamp(StartDistance, 0.0f, (float)WORLD_MAX);

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif // WITH_EDITOR

void UExponentialHeightFogComponent::PostInterpChange(UProperty* PropertyThatChanged)
{
	Super::PostInterpChange(PropertyThatChanged);

	MarkRenderStateDirty();
}

void UExponentialHeightFogComponent::SetFogDensity(float Value)
{
	if(FogDensity != Value)
	{
		FogDensity = Value;
		MarkRenderStateDirty();
	}
}

void UExponentialHeightFogComponent::SetFogInscatteringColor(FLinearColor Value)
{
	if(FogInscatteringColor != Value)
	{
		FogInscatteringColor = Value;
		MarkRenderStateDirty();
	}
}

void UExponentialHeightFogComponent::SetDirectionalInscatteringExponent(float Value)
{
	if(DirectionalInscatteringExponent != Value)
	{
		DirectionalInscatteringExponent = Value;
		MarkRenderStateDirty();
	}
}

void UExponentialHeightFogComponent::SetDirectionalInscatteringStartDistance(float Value)
{
	if(DirectionalInscatteringStartDistance != Value)
	{
		DirectionalInscatteringStartDistance = Value;
		MarkRenderStateDirty();
	}
}

void UExponentialHeightFogComponent::SetDirectionalInscatteringColor(FLinearColor Value)
{
	if(DirectionalInscatteringColor != Value)
	{
		DirectionalInscatteringColor = Value;
		MarkRenderStateDirty();
	}
}

void UExponentialHeightFogComponent::SetFogHeightFalloff(float Value)
{
	if(FogHeightFalloff != Value)
	{
		FogHeightFalloff = Value;
		MarkRenderStateDirty();
	}
}

void UExponentialHeightFogComponent::SetFogMaxOpacity(float Value)
{
	if(FogMaxOpacity != Value)
	{
		FogMaxOpacity = Value;
		MarkRenderStateDirty();
	}
}

void UExponentialHeightFogComponent::SetStartDistance(float Value)
{
	if(StartDistance != Value)
	{
		StartDistance = Value;
		MarkRenderStateDirty();
	}
}

//////////////////////////////////////////////////////////////////////////
// AExponentialHeightFog

AExponentialHeightFog::AExponentialHeightFog(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Component = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("HeightFogComponent0"));
	RootComponent = Component;

	bHidden = false;

#if WITH_EDITORONLY_DATA
	if (!IsRunningCommandlet() && (GetSpriteComponent() != NULL))
	{
		// Structure to hold one-time initialization
		struct FConstructorStatics
		{
			ConstructorHelpers::FObjectFinderOptional<UTexture2D> FogTextureObject;
			FName ID_Fog;
			FText NAME_Fog;
			FConstructorStatics()
				: FogTextureObject(TEXT("/Engine/EditorResources/S_ExpoHeightFog"))
				, ID_Fog(TEXT("Fog"))
				, NAME_Fog(NSLOCTEXT("SpriteCategory", "Fog", "Fog"))
			{
			}
		};
		static FConstructorStatics ConstructorStatics;

		GetSpriteComponent()->Sprite = ConstructorStatics.FogTextureObject.Get();
		GetSpriteComponent()->RelativeScale3D = FVector(0.5f, 0.5f, 0.5f);
		GetSpriteComponent()->SpriteInfo.Category = ConstructorStatics.ID_Fog;
		GetSpriteComponent()->SpriteInfo.DisplayName = ConstructorStatics.NAME_Fog;
		GetSpriteComponent()->AttachParent = Component;
	}
#endif // WITH_EDITORONLY_DATA
}

void AExponentialHeightFog::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	bEnabled = Component->bVisible;
}

void AExponentialHeightFog::GetLifetimeReplicatedProps( TArray< FLifetimeProperty > & OutLifetimeProps ) const
{
	Super::GetLifetimeReplicatedProps( OutLifetimeProps );
	
	DOREPLIFETIME( AExponentialHeightFog, bEnabled );
}

void AExponentialHeightFog::OnRep_bEnabled()
{
	Component->SetVisibility(bEnabled);
}

/** Returns Component subobject **/
UExponentialHeightFogComponent* AExponentialHeightFog::GetComponent() const { return Component; }
