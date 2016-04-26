// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	TextureInstanceManager.cpp: Implementation of content streaming classes.
=============================================================================*/

#include "EnginePrivate.h"
#include "TextureInstanceManager.h"

FTextureInstanceManager::FBounds4::FBounds4()
:	OriginX( 0, 0, 0, 0 )
,	OriginY( 0, 0, 0, 0 )
,	OriginZ( 0, 0, 0, 0 )
,	ExtentX( 0, 0, 0, 0 )
,	ExtentY( 0, 0, 0, 0 )
,	ExtentZ( 0, 0, 0, 0 )
,	Radius( 0, 0, 0, 0 )
,	MinDistanceSq( 0, 0, 0, 0 )
,	MaxDistanceSq( MAX_FLT, MAX_FLT, MAX_FLT, MAX_FLT )
,	LastRenderTime(0, 0, 0, 0)
{
}


void FTextureInstanceManager::FBounds4::Clear(int32 Index)
{
	check(Index >= 0 && Index < 4);

	OriginX.Component(Index) = 0;
	OriginY.Component(Index) = 0;
	OriginZ.Component(Index) = 0;
	ExtentX.Component(Index) = 0;
	ExtentY.Component(Index) = 0;
	ExtentZ.Component(Index) = 0;
	Radius.Component(Index) = 0;
	MinDistanceSq.Component(Index) = 0;
	MaxDistanceSq.Component(Index) = MAX_FLT;
	LastRenderTime.Component(Index) = 0;
}

void FTextureInstanceManager::FBounds4::Set(int32 Index, const FBoxSphereBounds& Bounds, float InLastRenderTime, float InMinDistance, float InMaxDistance)
{
	check(Index >= 0 && Index < 4);

	OriginX.Component(Index) = Bounds.Origin.X;
	OriginY.Component(Index) = Bounds.Origin.Y;
	OriginZ.Component(Index) = Bounds.Origin.Z;
	ExtentX.Component(Index) = Bounds.BoxExtent.X;
	ExtentY.Component(Index) = Bounds.BoxExtent.Y;
	ExtentZ.Component(Index) = Bounds.BoxExtent.Z;
	Radius.Component(Index) = Bounds.SphereRadius;
	MinDistanceSq.Component(Index) = InMinDistance * InMinDistance;
	MaxDistanceSq.Component(Index) = InMaxDistance != MAX_FLT ? (InMaxDistance * InMaxDistance) : MAX_FLT;
	LastRenderTime.Component(Index) = InLastRenderTime;
}

FTextureInstanceManager::FElement::FElement()
:	Component(nullptr)
,	Texture(nullptr)
,	BoundsIndex(INDEX_NONE)
,	TexelFactor(0)
,	PrevTextureLink(INDEX_NONE)
,	NextTextureLink(INDEX_NONE)
,	NextComponentLink(INDEX_NONE)
{
}

int32 FTextureInstanceManager::AddBounds(const FBoxSphereBounds& Bounds, const UPrimitiveComponent* InComponent, float LastRenderTime, float MinDistance, float MaxDistance)
{
	check(bAllowNullComponents || InComponent);

	int BoundsIndex = INDEX_NONE;

	if (FreeBoundIndices.Num())
	{
		BoundsIndex = FreeBoundIndices.Pop();
	}
	else
	{
		BoundsIndex = Bounds4.Num() * 4;
		Bounds4.Push(FBounds4());

		Bounds4Components.Push(nullptr);
		Bounds4Components.Push(nullptr);
		Bounds4Components.Push(nullptr);
		Bounds4Components.Push(nullptr);

		// Since each element contains 4 entries, add the 3 unused ones
		FreeBoundIndices.Push(BoundsIndex + 1);
		FreeBoundIndices.Push(BoundsIndex + 2);
		FreeBoundIndices.Push(BoundsIndex + 3);

		// This is just to be compatible with : check(Instances.FirstDirtyComponentIndex == Instances.Bounds4Components.Num());
		// Whenever new bounds are allocated, they are automatically up to date.
		if (FirstDirtyComponentIndex == BoundsIndex)
		{
			FirstDirtyComponentIndex += 4;
		}
	}

	Bounds4[BoundsIndex / 4].Set(BoundsIndex % 4, Bounds, LastRenderTime, MinDistance, MaxDistance);
	Bounds4Components[BoundsIndex] = InComponent;

	return BoundsIndex;
}

void FTextureInstanceManager::RemoveBounds(int32 BoundsIndex)
{
	check(BoundsIndex != INDEX_NONE);
	checkSlow(!FreeBoundIndices.Contains(BoundsIndex));

	// If note all indices were freed
	if (1 + FreeBoundIndices.Num() != Bounds4.Num() * 4)
	{
		FreeBoundIndices.Push(BoundsIndex);
		Bounds4[BoundsIndex / 4].Clear(BoundsIndex % 4);
		Bounds4Components[BoundsIndex] = nullptr;
	}
	else
	{
		Bounds4.Empty();
		Bounds4Components.Empty();
		FreeBoundIndices.Empty();
	}
}

void FTextureInstanceManager::AddElement(const UPrimitiveComponent* InComponent, const UTexture2D* InTexture, int InBoundsIndex, float InTexelFactor, int32*& ComponentLink)
{
	check(bAllowNullComponents || InComponent);

	int32 ElementIndex = INDEX_NONE;
	if (FreeElementIndices.Num())
	{
		ElementIndex = FreeElementIndices.Pop();
	}
	else
	{
		ElementIndex = Elements.Num();
		Elements.Push(FElement());
	}

	FElement& Element = Elements[ElementIndex];

	Element.Component = InComponent;
	Element.Texture = InTexture;
	Element.BoundsIndex = InBoundsIndex;
	Element.TexelFactor = InTexelFactor;

	if (InTexture)
	{
		int32* TextureLink = TextureMap.Find(InTexture);
		if (TextureLink)
		{
			FElement& TextureLinkElement = Elements[*TextureLink];
			if (TextureLinkElement.Component == InComponent && 
				TextureLinkElement.BoundsIndex == InBoundsIndex)
			{
				// Abort inserting a new element, and merge the 2 entries together.
				TextureLinkElement.TexelFactor = FMath::Max(TextureLinkElement.TexelFactor, Element.TexelFactor);

				// Clear the element and insert it in the free list.
				Element = FElement();
				FreeElementIndices.Push(ElementIndex);
				return;
			}

			// The new inserted element as the head element.
			Element.NextTextureLink = *TextureLink;
			TextureLinkElement.PrevTextureLink = ElementIndex;
			*TextureLink = ElementIndex;
		}
		else
		{
			TextureMap.Add(InTexture, ElementIndex);
		}
	}

	if (InComponent)
	{
		checkSlow(ComponentLink == ComponentMap.Find(InComponent));
		
		if (ComponentLink)
		{
			// The new inserted element as the head element.
			Element.NextComponentLink = *ComponentLink;
			*ComponentLink = ElementIndex;
		}
		else
		{
			ComponentLink = &ComponentMap.Add(InComponent, ElementIndex);
		}
	}
}

void FTextureInstanceManager::RemoveElement(int32 ElementIndex, int32& NextComponentLink, int32& BoundsIndex, const UTexture2D*& Texture)
{
	FElement& Element = Elements[ElementIndex];
	NextComponentLink = Element.NextComponentLink; 
	BoundsIndex = Element.BoundsIndex; 

	// Unlink textures
	if (Element.Texture)
	{
		if (Element.PrevTextureLink == INDEX_NONE) // If NONE, that means this is the head of the texture list.
		{
			if (Element.NextTextureLink != INDEX_NONE) // Check if there are other entries for this texture.
			{
				 // Replace the head
				*TextureMap.Find(Element.Texture) = Element.NextTextureLink;
				Elements[Element.NextTextureLink].PrevTextureLink = INDEX_NONE;
			}
			else // Otherwise, remove the texture entry
			{
				TextureMap.Remove(Element.Texture);
				Texture = Element.Texture;
			}
		}
		else // Otherwise, just relink entries.
		{
			Elements[Element.PrevTextureLink].NextTextureLink = Element.NextTextureLink;

			if (Element.NextTextureLink != INDEX_NONE)
			{
				Elements[Element.NextTextureLink].PrevTextureLink = Element.PrevTextureLink;
			}
		}
	}


	// Clear the element and insert in free list.
	if (1 + FreeElementIndices.Num() != Elements.Num())
	{
		FreeElementIndices.Push(ElementIndex);
		Element = FElement();
	}
	else
	{
		Elements.Empty();
		FreeElementIndices.Empty();
	}
}

void FTextureInstanceManager::AddComponent(const UPrimitiveComponent* Component)
{
	TArray<FStreamingTexturePrimitiveInfo> TextureInstanceInfos;
	Component->GetStreamingTextureInfoWithNULLRemoval(TextureInstanceInfos);

	bool bHasEntries = false;

	int32 BoundsIndex = AddBounds(Component->Bounds, Component, Component->LastRenderTime);
	int32* ComponentLink = ComponentMap.Find(Component);
	for (int32 TextureIndex = 0; TextureIndex  < TextureInstanceInfos.Num(); ++TextureIndex)
	{
		const FStreamingTexturePrimitiveInfo& Info = TextureInstanceInfos[TextureIndex];
		if (Info.TexelFactor > 0.0f && Info.Bounds.SphereRadius > 0.0f && ensure(FMath::IsFinite(Info.TexelFactor)))
		{
			const UTexture2D* Texture2D = Cast<UTexture2D>(Info.Texture);
			if (Texture2D && IsStreamingTexture(Texture2D))
			{
				AddElement(Component, Texture2D, BoundsIndex, Info.TexelFactor, ComponentLink);
				bHasEntries = true;
			}
		}
	}

	if (!bHasEntries)
	{
		RemoveBounds(BoundsIndex);
	}
}

void FTextureInstanceManager::RemoveComponent(const UPrimitiveComponent* Component, FRemovedTextureArray& RemovedTextures)
{
	TArray<int32, TInlineAllocator<12> > RemovedBoundsIndices;
	int32 ElementIndex = INDEX_NONE;

	ComponentMap.RemoveAndCopyValue(Component, ElementIndex);
	while (ElementIndex != INDEX_NONE)
	{
		int32 BoundsIndex = INDEX_NONE;
		const UTexture2D* Texture = nullptr;

		RemoveElement(ElementIndex, ElementIndex, BoundsIndex, Texture);

		if (BoundsIndex != INDEX_NONE)
		{
			RemovedBoundsIndices.AddUnique(BoundsIndex);
		}

		if (Texture)
		{
			RemovedTextures.AddUnique(Texture);
		}

	};

	for(int32 I = 0; I < RemovedBoundsIndices.Num(); ++I)
	{
		RemoveBounds(RemovedBoundsIndices[I]);
	}
}

void FTextureInstanceManager::UpdateBounds(const UPrimitiveComponent* Component)
{
	int32* ComponentLink = ComponentMap.Find(Component);
	if (ComponentLink)
	{
		int32 BoundsIndex = Elements[*ComponentLink].BoundsIndex;

		// For primitives that can update bounds, there is only one bound per component.
		Bounds4[BoundsIndex / 4].Set(BoundsIndex % 4, Component->Bounds, Component->LastRenderTime);
	}
}

void FTextureInstanceManager::IncrementalUpdateBounds(float Percentage)
{
	const int32 LastIndex = FMath::Min(Bounds4Components.Num(), FirstDirtyComponentIndex + FMath::CeilToInt((float)Bounds4Components.Num() * Percentage));
	for (int32 Index = FirstDirtyComponentIndex; Index < LastIndex; ++Index)
	{
		const UPrimitiveComponent* Component = Bounds4Components[Index];
		if (Component)
		{
			Bounds4[Index / 4].Set(Index % 4, Component->Bounds, Component->LastRenderTime);
		}
	}
	FirstDirtyComponentIndex = LastIndex;
}

/*
void FTextureInstanceManager::SanityCheck()
{
	// FreeBoundIndices does not contain duplicate entries

}
*/

void FTextureBoundsVisibility::Set(const FTextureInstanceManager& Instances)
{
	QUICK_SCOPE_CYCLE_COUNTER(FTextureBoundsVisibility_Set);

	// Data must be valid at this point.
	check(Instances.FirstDirtyComponentIndex == Instances.Bounds4Components.Num());

	Bounds4 = Instances.Bounds4;
	Elements = Instances.Elements;
	TextureMap = Instances.TextureMap;
}

void FTextureBoundsVisibility::QuickSet(FTextureInstanceManager& Instances)
{
	QUICK_SCOPE_CYCLE_COUNTER(FTextureBoundsVisibility_QuickSet);

	// Data must be valid at this point.
	check(Instances.FirstDirtyComponentIndex >= Instances.Bounds4Components.Num());
	check(!Instances.bAllowNullComponents);

	// Set the two arrays to the same size then swap the buffers.
	const int32 DeltaNum = Instances.Bounds4.Num() - Bounds4.Num();
	if (DeltaNum > 0) // Bounds4 is too small!
	{
		Bounds4.AddUninitialized(DeltaNum);
	}
	else if (DeltaNum < 0) // Bounds4 is too big
	{
		Bounds4.RemoveAt(Instances.Bounds4.Num(), -DeltaNum);
	}
	FMemory::Memswap(&Bounds4, &Instances.Bounds4, sizeof(Bounds4));
	Instances.FirstDirtyComponentIndex = 0; // Expecting an incremental update.

	Elements = Instances.Elements;

	// Swap the map and keep the clean version
	TextureMap = Instances.TextureMap;
	FMemory::Memswap(&TextureMap , &Instances.TextureMap, sizeof(TextureMap));
}

void FTextureBoundsVisibility::ComputeBoundsViewInfos(const TArray<FStreamingViewInfo>& ViewInfos, bool bUseAABB, float MaxEffectiveScreenSize)
{
	const int32 NumViews = ViewInfos.Num();
	const VectorRegister One4 = VectorSet(1.f, 1.f, 1.f, 1.f);

	BoundsViewInfo4.Empty(NumViews);
	for (int32 ViewIndex = 0; ViewIndex < NumViews; ++ViewIndex)
	{
		BoundsViewInfo4.AddDefaulted();
		BoundsViewInfo4[ViewIndex].AddUninitialized(Bounds4.Num());
	}

	for (int32 Bounds4Index = 0; Bounds4Index < Bounds4.Num(); ++Bounds4Index)
	{
		const FBounds4& CurrentBounds4 = Bounds4[Bounds4Index];

		// Calculate distance of viewer to bounding sphere.
		const VectorRegister OriginX = VectorLoadAligned( &CurrentBounds4.OriginX );
		const VectorRegister OriginY = VectorLoadAligned( &CurrentBounds4.OriginY );
		const VectorRegister OriginZ = VectorLoadAligned( &CurrentBounds4.OriginZ );
		const VectorRegister ExtentX = VectorLoadAligned( &CurrentBounds4.ExtentX );
		const VectorRegister ExtentY = VectorLoadAligned( &CurrentBounds4.ExtentY );
		const VectorRegister ExtentZ = VectorLoadAligned( &CurrentBounds4.ExtentZ );
		const VectorRegister Radius = VectorLoadAligned( &CurrentBounds4.Radius );
		const VectorRegister MinDistanceSq = VectorLoadAligned( &CurrentBounds4.MinDistanceSq );
		const VectorRegister MaxDistanceSq = VectorLoadAligned( &CurrentBounds4.MaxDistanceSq );

		for (int32 ViewIndex = 0; ViewIndex < NumViews; ++ViewIndex)
		{
			const FStreamingViewInfo& ViewInfo = ViewInfos[ViewIndex];

			const float EffectiveScreenSize = (MaxEffectiveScreenSize > 0.0f) ? FMath::Min(MaxEffectiveScreenSize, ViewInfo.ScreenSize) : ViewInfo.ScreenSize;
			const float ScreenSizeFloat = EffectiveScreenSize * ViewInfo.BoostFactor;

			const VectorRegister ScreenSize = VectorLoadFloat1( &ScreenSizeFloat );
			const VectorRegister ViewOriginX = VectorLoadFloat1( &ViewInfo.ViewOrigin.X );
			const VectorRegister ViewOriginY = VectorLoadFloat1( &ViewInfo.ViewOrigin.Y );
			const VectorRegister ViewOriginZ = VectorLoadFloat1( &ViewInfo.ViewOrigin.Z );

			//const float DistSq = FVector::DistSquared( ViewInfo.ViewOrigin, TextureInstance.BoundingSphere.Center );
			VectorRegister Temp = VectorSubtract( ViewOriginX, OriginX );
			VectorRegister DistSq = VectorMultiply( Temp, Temp );
			Temp = VectorSubtract( ViewOriginY, OriginY );
			DistSq = VectorMultiplyAdd( Temp, Temp, DistSq );
			Temp = VectorSubtract( ViewOriginZ, OriginZ );
			DistSq = VectorMultiplyAdd( Temp, Temp, DistSq );

			VectorRegister ClampedDistSq = VectorMax( MinDistanceSq, DistSq );
			ClampedDistSq = VectorMin( MaxDistanceSq, ClampedDistSq );
			const VectorRegister InRangeMask = VectorCompareEQ(DistSq, ClampedDistSq);

			VectorRegister DistSqMinusRadiusSq = VectorZero();
			if (bUseAABB)
			{
				// In this case DistSqMinusRadiusSq will contain the distance to the box^2
				Temp = VectorSubtract( ViewOriginX, OriginX );
				Temp = VectorAbs( Temp );
				VectorRegister BoxRef = VectorMin( Temp, ExtentX );
				Temp = VectorSubtract( Temp, BoxRef );
				DistSqMinusRadiusSq = VectorMultiply( Temp, Temp );

				Temp = VectorSubtract( ViewOriginY, OriginY );
				Temp = VectorAbs( Temp );
				BoxRef = VectorMin( Temp, ExtentY );
				Temp = VectorSubtract( Temp, BoxRef );
				DistSqMinusRadiusSq = VectorMultiplyAdd( Temp, Temp, DistSqMinusRadiusSq );

				Temp = VectorSubtract( ViewOriginZ, OriginZ );
				Temp = VectorAbs( Temp );
				BoxRef = VectorMin( Temp, ExtentZ );
				Temp = VectorSubtract( Temp, BoxRef );
				DistSqMinusRadiusSq = VectorMultiplyAdd( Temp, Temp, DistSqMinusRadiusSq );
			}
			else
			{
				DistSqMinusRadiusSq = VectorMultiply( Radius, Radius );
				DistSqMinusRadiusSq = VectorSubtract( DistSq, DistSqMinusRadiusSq );
			}
			DistSqMinusRadiusSq = VectorMax(DistSqMinusRadiusSq, One4); // Prevents / 0
			VectorRegister ScreenSizeOverDistance = VectorReciprocalSqrt(DistSqMinusRadiusSq);
			ScreenSizeOverDistance = VectorMultiply(ScreenSizeOverDistance, ScreenSize);

			// Store results
			FBoundsViewInfo4& BoundsVieWInfo = BoundsViewInfo4[ViewIndex][Bounds4Index];
			VectorStoreAligned(ScreenSizeOverDistance, &BoundsVieWInfo.ScreenSizeOverDistance);
			VectorStoreAligned(DistSqMinusRadiusSq, &BoundsVieWInfo.DistanceSq);
			VectorStoreAligned(InRangeMask, &BoundsVieWInfo.InRangeMask);
		}
	}
}

bool FTextureBoundsVisibility::GetTexelSize(const UTexture2D* InTexture, float& MaxSize_InRange, float& MinDistanceSq_InRange, float& MaxSize_AnyRange, float& MinDistanceSq_AnyRange) const
{
	MaxSize_InRange = 0;
	MaxSize_AnyRange = 0;
	MinDistanceSq_InRange = MAX_FLT;
	MinDistanceSq_AnyRange = MAX_FLT;

	const int32* TextureLink = TextureMap.Find(InTexture);
	if (TextureLink)
	{
		int32 ElementIndex = *TextureLink;
		while (ElementIndex != INDEX_NONE)
		{
			const FElement& Element = Elements[ElementIndex];
			const int32 BoundsIndex = Element.BoundsIndex;

			for (int32 ViewIndex = 0; ViewIndex < BoundsViewInfo4.Num(); ++ViewIndex)
			{
				FBoundsViewInfo4& BoundsVieWInfo = const_cast<FBoundsViewInfo4&>(BoundsViewInfo4[ViewIndex][BoundsIndex / 4]);
				const float ScreenSizeOverDistance = BoundsVieWInfo.ScreenSizeOverDistance.Component(BoundsIndex % 4);
				const float DistanceSq = BoundsVieWInfo.DistanceSq.Component(BoundsIndex % 4);
				const int32 InRangeMask = reinterpret_cast<const int32&>(BoundsVieWInfo.InRangeMask.Component(BoundsIndex % 4));

#if 0
				//@TODO (for static instances)
				// Used with TEXTUREGROUP_Terrain_Heightmap
				// To check Forced LOD...
				if (Element.TexelFactor < 0)
				{
					VectorRegister MinForcedLODs = VectorLoadAligned( &TextureInstance.TexelFactor );
					bool AllForcedLOD = !!VectorAllLesserThan(MinForcedLODs, VectorZero());
					MinForcedLODs = VectorMin( MinForcedLODs, VectorSwizzle(MinForcedLODs, 2, 3, 0, 1) );
					MinForcedLODs = VectorMin( MinForcedLODs, VectorReplicate(MinForcedLODs, 1) );
					float MinLODValue;
					VectorStoreFloat1( MinForcedLODs, &MinLODValue );

					if (MinLODValue <= 0)
					{
						WantedMipCount = FMath::Max(WantedMipCount, FFloatMipLevel::FromMipLevel(StreamingTexture.MaxAllowedMips - 13 - FMath::FloorToInt(MinLODValue)));
						if (WantedMipCount >= FFloatMipLevel::FromMipLevel(StreamingTexture.MaxAllowedMips))
					{
						MinDistance = 1.0f;
						bShouldAbortLoop = true;
					}
				}
#endif

				float Size = Element.TexelFactor * ScreenSizeOverDistance;

				MaxSize_AnyRange = FMath::Max(Size, MaxSize_AnyRange);
				MinDistanceSq_AnyRange = FMath::Min(DistanceSq, MinDistanceSq_AnyRange);
				if (InRangeMask)
				{
					MaxSize_InRange = FMath::Max(Size, MaxSize_InRange);
					MinDistanceSq_InRange = FMath::Min(DistanceSq, MinDistanceSq_InRange);

					if (MinDistanceSq_InRange <= 1)
					{
						return true; // Already at max
					}
				}
			}

			ElementIndex = Element.NextTextureLink;
		}
	}

	return TextureLink != nullptr;
}


void FDynamicComponentTextureManager::Add(const UPrimitiveComponent* Component, EDynamicPrimitiveType DynamicType, FRemovedTextureArray& RemovedTextures)
{
	FComponentState* State = ComponentStates.Find(Component);

	if (!State)
	{
		ComponentStates.Add(Component, FComponentState(DynamicType));
	}
	else
	{
		if (State->bHasTextures) // If we reinsert without having cleared the old data correctly, make sure to clear it now.
		{
			TextureInstances.RemoveComponent(Component, RemovedTextures);
			State->bHasTextures = false;
		}
		State->bToDelete = false;
	}
	DirtyComponents.Add(Component);
}

// Remove the component immediately, removing all links. Only keep the dynamic type as reference.
void FDynamicComponentTextureManager::Remove(const UPrimitiveComponent* Component, FRemovedTextureArray& RemovedTextures)
{
	FComponentState* State = ComponentStates.Find(Component);

	if (State)
	{
		if (State->bHasTextures) // When a component becomes invalid, remove any texture and bound references.
		{
			TextureInstances.RemoveComponent(Component, RemovedTextures);
			State->bHasTextures = false;
		}
		State->bToDelete = true;
		DirtyComponents.Add(Component);
	}

}

void FDynamicComponentTextureManager::Update(const UPrimitiveComponent* Component, FRemovedTextureArray& RemovedTextures)
{
	FComponentState* State = ComponentStates.Find(Component);

	if (State && !State->bToDelete && State->bHasTextures)
	{
		// If we reinserted without having cleared the old data correctly, make sure to clear it now.
		TextureInstances.RemoveComponent(Component, RemovedTextures);
		State->bHasTextures = false;
		DirtyComponents.Add(Component);
	}
}

void FDynamicComponentTextureManager::IncrementalTick(FRemovedTextureArray& RemovedTextures, float Percentage)
{
	QUICK_SCOPE_CYCLE_COUNTER(FDynamicComponentTextureManager_IncrementalTick);

	TextureInstances.IncrementalUpdateBounds(Percentage);

	TSet<const UPrimitiveComponent*> NextDirtyComponents; // Components that have a dirty states (to accelerate the update).
	for (TSet<const UPrimitiveComponent*>::TIterator It(DirtyComponents); It; ++It)
	{
		const UPrimitiveComponent* Component = *It;

		FComponentState* State = ComponentStates.Find(Component);
		if (!State) continue;

		if (State->bToDelete)
		{
			check(!State->bHasTextures);
			ComponentStates.Remove(Component);
		}
		else
		{
			bool bIsInScene = Component->IsRegistered();
			if (!bIsInScene)
			{
				if (State->bHasTextures)
				{
					TextureInstances.RemoveComponent(Component, RemovedTextures);
					State->bHasTextures = false;
				}
				NextDirtyComponents.Add(Component);
			}
			else if (bIsInScene && !State->bHasTextures)
			{
				TextureInstances.AddComponent(Component);
				State->bHasTextures = true;
			}
			else
			{
				TextureInstances.UpdateBounds(Component);
			}
		}
	}

	FMemory::Memswap(&DirtyComponents, &NextDirtyComponents, sizeof(DirtyComponents));
}
