// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DecalComponent.cpp: Decal component implementation.
=============================================================================*/

#include "EnginePrivate.h"
#include "LevelUtils.h"
#include "Components/DecalComponent.h"

static TAutoConsoleVariable<float> CVarDecalFadeDurationScale(
	TEXT("r.Decal.FadeDurationScale"),
	1.0f,
	TEXT("Scales the per decal fade durations. Lower values shortens lifetime and fade duration. Default is 1.0f.")
	);

FDeferredDecalProxy::FDeferredDecalProxy(const UDecalComponent* InComponent)
	: InvFadeDuration(0.0f)
	, FadeStartDelayNormalized(1.0f)
{
	UMaterialInterface* EffectiveMaterial = UMaterial::GetDefaultMaterial(MD_DeferredDecal);

	if(InComponent->DecalMaterial)
	{
		UMaterial* BaseMaterial = InComponent->DecalMaterial->GetMaterial();

		if(BaseMaterial->MaterialDomain == MD_DeferredDecal)
		{
			EffectiveMaterial = InComponent->DecalMaterial;
		}
	}

	Component = InComponent;
	DecalMaterial = EffectiveMaterial;
	SetTransformIncludingDecalSize(InComponent->GetTransformIncludingDecalSize());
	DrawInGame = InComponent->ShouldRender();
	bOwnerSelected = InComponent->IsOwnerSelected();
	SortOrder = InComponent->SortOrder;
	InitializeFadingParameters(InComponent->GetWorld()->GetTimeSeconds(), InComponent->GetFadeDuration(), InComponent->GetFadeStartDelay());
	
}

void FDeferredDecalProxy::SetTransformIncludingDecalSize(const FTransform& InComponentToWorldIncludingDecalSize)
{
	ComponentTrans = InComponentToWorldIncludingDecalSize;
}

void FDeferredDecalProxy::InitializeFadingParameters(float AbsSpawnTime, float FadeDuration, float FadeStartDelay)
{
	if (FadeDuration > 0.0f)
	{
		InvFadeDuration = 1.0f / FadeDuration;
		FadeStartDelayNormalized = (AbsSpawnTime + FadeStartDelay + FadeDuration) * InvFadeDuration;
	}
}

UDecalComponent::UDecalComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, FadeScreenSize(0.01)
	, FadeStartDelay(0.0f)
	, FadeDuration(0.0f)
	, bDestroyOwnerAfterFade(true)
	, DecalSize(128.0f, 256.0f, 256.0f)
{
}

void UDecalComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (Ar.UE4Ver() < VER_UE4_DECAL_SIZE)
	{
		DecalSize = FVector(1.0f, 1.0f, 1.0f);
	}
}

void UDecalComponent::SetLifeSpan(const float LifeSpan)
{
	if (LifeSpan > 0.f)
	{
		GetWorld()->GetTimerManager().SetTimer(TimerHandle_DestroyDecalComponent, this, &UDecalComponent::LifeSpanCallback, LifeSpan, false);
	}
	else
	{
		GetWorld()->GetTimerManager().ClearTimer(TimerHandle_DestroyDecalComponent);
	}
}

void UDecalComponent::LifeSpanCallback()
{
	DestroyComponent();

	auto* Owner = GetOwner();

	if (bDestroyOwnerAfterFade && Owner && (FadeDuration > 0.0f || FadeStartDelay > 0.0f))
	{
		Owner->Destroy();
	}
}

float UDecalComponent::GetFadeStartDelay() const
{
	return FadeStartDelay;
}

float UDecalComponent::GetFadeDuration() const
{
	return FadeDuration;
}

void UDecalComponent::SetFadeOut(float StartDelay, float Duration, bool DestroyOwnerAfterFade /*= true*/)
{
	float FadeDurationScale = CVarDecalFadeDurationScale.GetValueOnGameThread();
	FadeDurationScale = (FadeDurationScale <= SMALL_NUMBER) ? 0.0f : FadeDurationScale;

	FadeStartDelay = StartDelay * FadeDurationScale;
	FadeDuration = Duration * FadeDurationScale;
	bDestroyOwnerAfterFade = DestroyOwnerAfterFade;
	SetLifeSpan(FadeStartDelay + FadeDuration);

	MarkRenderStateDirty();
}

void UDecalComponent::SetSortOrder(int32 Value)
{
	SortOrder = Value;

	MarkRenderStateDirty();
}

void UDecalComponent::SetDecalMaterial(class UMaterialInterface* NewDecalMaterial)
{
	DecalMaterial = NewDecalMaterial;

	MarkRenderStateDirty();	
}

void UDecalComponent::PushSelectionToProxy()
{
	MarkRenderStateDirty();	
}

class UMaterialInterface* UDecalComponent::GetDecalMaterial() const
{
	return DecalMaterial;
}

class UMaterialInstanceDynamic* UDecalComponent::CreateDynamicMaterialInstance()
{
	// Create the MID
	UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(DecalMaterial, this);

	// Assign it, once parent is set
	SetDecalMaterial(Instance);

	return Instance;
}

void UDecalComponent::GetUsedMaterials( TArray<UMaterialInterface*>& OutMaterials ) const
{
	OutMaterials.Add( GetDecalMaterial() );
}


FDeferredDecalProxy* UDecalComponent::CreateSceneProxy()
{
	return new FDeferredDecalProxy(this);
}

FBoxSphereBounds UDecalComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	return FBoxSphereBounds(FVector(0, 0, 0), DecalSize, DecalSize.Size()).TransformBy(LocalToWorld);
}

void UDecalComponent::BeginPlay()
{
	Super::BeginPlay();

	SetLifeSpan(FadeStartDelay + FadeDuration);
}

void UDecalComponent::CreateRenderState_Concurrent()
{
	Super::CreateRenderState_Concurrent();

	// Mimics UPrimitiveComponent's visibility logic, although without the UPrimitiveCompoent visibility flags
	if ( ShouldComponentAddToScene() && ShouldRender() )
	{
		World->Scene->AddDecal(this);
	}
}

void UDecalComponent::SendRenderTransform_Concurrent()
{	
	//If Decal isn't hidden update its transform.
	if ( ShouldComponentAddToScene() && ShouldRender() )
	{
		World->Scene->UpdateDecalTransform(this);
	}

	Super::SendRenderTransform_Concurrent();
}

const UObject* UDecalComponent::AdditionalStatObject() const
{
	return DecalMaterial;
}

void UDecalComponent::DestroyRenderState_Concurrent()
{
	Super::DestroyRenderState_Concurrent();
	World->Scene->RemoveDecal(this);
}

