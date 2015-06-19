// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DecalComponent.cpp: Decal component implementation.
=============================================================================*/

#include "EnginePrivate.h"
#include "LevelUtils.h"
#include "Components/DecalComponent.h"

FDeferredDecalProxy::FDeferredDecalProxy(const UDecalComponent* InComponent)
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
	ComponentTrans = InComponent->GetComponentToWorld();
	DrawInGame = InComponent->ShouldRender();
	bOwnerSelected = InComponent->IsOwnerSelected();
	SortOrder = InComponent->SortOrder;
}

void FDeferredDecalProxy::SetTransform(const FTransform& InComponentToWorld)
{
	ComponentTrans = InComponentToWorld;
}

UDecalComponent::UDecalComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
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
	return FBoxSphereBounds();
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

