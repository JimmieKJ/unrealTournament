// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "GameplayDebuggerPrivatePCH.h"
#include "GameplayDebuggerRenderingComponent.h"
#include "DebugRenderSceneProxy.h"

//////////////////////////////////////////////////////////////////////////
// FGameplayDebuggerCompositeSceneProxy

class FGameplayDebuggerCompositeSceneProxy : public FDebugRenderSceneProxy
{
public:
	FGameplayDebuggerCompositeSceneProxy(const UPrimitiveComponent* InComponent) : FDebugRenderSceneProxy(InComponent) { }

	virtual ~FGameplayDebuggerCompositeSceneProxy()
	{
		for (int32 Idx = 0; Idx < ChildProxies.Num(); Idx++)
		{
			delete ChildProxies[Idx];
		}
	}

	virtual void DrawStaticElements(FStaticPrimitiveDrawInterface* PDI) override
	{
		for (int32 Idx = 0; Idx < ChildProxies.Num(); Idx++)
		{
			ChildProxies[Idx]->DrawStaticElements(PDI);
		}
	}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
	{
		for (int32 Idx = 0; Idx < ChildProxies.Num(); Idx++)
		{
			ChildProxies[Idx]->GetDynamicMeshElements(Views, ViewFamily, VisibilityMap, Collector);
		}
	}

	virtual void RegisterDebugDrawDelgate() override
	{
		for (int32 Idx = 0; Idx < ChildProxies.Num(); Idx++)
		{
			ChildProxies[Idx]->RegisterDebugDrawDelgate();
		}
	}

	virtual void UnregisterDebugDrawDelgate() override
	{
		for (int32 Idx = 0; Idx < ChildProxies.Num(); Idx++)
		{
			ChildProxies[Idx]->UnregisterDebugDrawDelgate();
		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		FPrimitiveViewRelevance Result;
		for (int32 Idx = 0; Idx < ChildProxies.Num(); Idx++)
		{
			Result |= ChildProxies[Idx]->GetViewRelevance(View);
		}
		return Result;
	}

	virtual uint32 GetMemoryFootprint(void) const override
	{
		return sizeof(*this) + GetAllocatedSize();
	}

	uint32 GetAllocatedSize(void) const
	{
		uint32 Size = ChildProxies.GetAllocatedSize();
		for (int32 Idx = 0; Idx < ChildProxies.Num(); Idx++)
		{
			Size += ChildProxies[Idx]->GetMemoryFootprint();
		}

		return Size;
	}

	void AddChild(FDebugRenderSceneProxy* NewChild)
	{
		ChildProxies.AddUnique(NewChild);
	}

	void AddRange(TArray<FDebugRenderSceneProxy*> Children)
	{
		ChildProxies.Append(Children);
	}

protected:
	TArray<FDebugRenderSceneProxy*> ChildProxies;
};

//////////////////////////////////////////////////////////////////////////
// UGameplayDebuggerRenderingComponent

UGameplayDebuggerRenderingComponent::UGameplayDebuggerRenderingComponent(const FObjectInitializer& ObjInitializer) : Super(ObjInitializer)
{
}

FPrimitiveSceneProxy* UGameplayDebuggerRenderingComponent::CreateSceneProxy()
{
	FGameplayDebuggerCompositeSceneProxy* CompositeProxy = nullptr;

	AGameplayDebuggerCategoryReplicator* OwnerReplicator = Cast<AGameplayDebuggerCategoryReplicator>(GetOwner());
	if (OwnerReplicator && OwnerReplicator->IsEnabled())
	{
		TArray<FDebugRenderSceneProxy*> SceneProxies;
		for (int32 Idx = 0; Idx < OwnerReplicator->GetNumCategories(); Idx++)
		{
			TSharedRef<FGameplayDebuggerCategory> Category = OwnerReplicator->GetCategory(Idx);
			if (Category->IsCategoryEnabled())
			{
				FDebugRenderSceneProxy* CategorySceneProxy = Category->CreateSceneProxy(this);
				if (CategorySceneProxy)
				{
					SceneProxies.Add(CategorySceneProxy);
				}
			}
		}

		if (SceneProxies.Num())
		{
			CompositeProxy = new FGameplayDebuggerCompositeSceneProxy(this);
			CompositeProxy->AddRange(SceneProxies);
		}
	}

	return CompositeProxy;
}

FBoxSphereBounds UGameplayDebuggerRenderingComponent::CalcBounds(const FTransform &LocalToWorld) const
{
	return FBoxSphereBounds(FBox::BuildAABB(FVector::ZeroVector, FVector(1000000.0f, 1000000.0f, 1000000.0f)));
}

void UGameplayDebuggerRenderingComponent::CreateRenderState_Concurrent()
{
	Super::CreateRenderState_Concurrent();

	if (SceneProxy)
	{
		static_cast<FGameplayDebuggerCompositeSceneProxy*>(SceneProxy)->RegisterDebugDrawDelgate();
	}
}

void UGameplayDebuggerRenderingComponent::DestroyRenderState_Concurrent()
{
	if (SceneProxy)
	{
		static_cast<FGameplayDebuggerCompositeSceneProxy*>(SceneProxy)->UnregisterDebugDrawDelgate();
	}

	Super::DestroyRenderState_Concurrent();
}
