// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	TextureInstanceManager.cpp: Implementation of content streaming classes.
=============================================================================*/

#include "EnginePrivate.h"
#include "TextureInstanceManager.h"

FTextureInstanceState::FBounds4::FBounds4()
:	OriginX( 0, 0, 0, 0 )
,	OriginY( 0, 0, 0, 0 )
,	OriginZ( 0, 0, 0, 0 )
,	RangeOriginX( 0, 0, 0, 0 )
,	RangeOriginY( 0, 0, 0, 0 )
,	RangeOriginZ( 0, 0, 0, 0 )
,	ExtentX( 0, 0, 0, 0 )
,	ExtentY( 0, 0, 0, 0 )
,	ExtentZ( 0, 0, 0, 0 )
,	Radius( 0, 0, 0, 0 )
,	MinDistanceSq( 0, 0, 0, 0 )
,	MinRangeSq( 0, 0, 0, 0 )
,	MaxRangeSq( FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX )
,	LastRenderTime( -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX)
{
}


void FTextureInstanceState::FBounds4::Clear(int32 Index)
{
	check(Index >= 0 && Index < 4);

	OriginX.Component(Index) = 0;
	OriginY.Component(Index) = 0;
	OriginZ.Component(Index) = 0;
	RangeOriginX.Component(Index) = 0;
	RangeOriginY.Component(Index) = 0;
	RangeOriginZ.Component(Index) = 0;
	ExtentX.Component(Index) = 0;
	ExtentY.Component(Index) = 0;
	ExtentZ.Component(Index) = 0;
	Radius.Component(Index) = 0;
	MinDistanceSq.Component(Index) = 0;
	MinRangeSq.Component(Index) = 0;
	MaxRangeSq.Component(Index) = FLT_MAX;
	LastRenderTime.Component(Index) = -FLT_MAX;
}

void FTextureInstanceState::FBounds4::Set(int32 Index, const FBoxSphereBounds& Bounds, float InLastRenderTime, const FVector& RangeOrigin, float InMinDistance, float InMinRange, float InMaxRange)
{
	check(Index >= 0 && Index < 4);

	OriginX.Component(Index) = Bounds.Origin.X;
	OriginY.Component(Index) = Bounds.Origin.Y;
	OriginZ.Component(Index) = Bounds.Origin.Z;
	RangeOriginX.Component(Index) = RangeOrigin.X;
	RangeOriginY.Component(Index) = RangeOrigin.Y;
	RangeOriginZ.Component(Index) = RangeOrigin.Z;
	ExtentX.Component(Index) = Bounds.BoxExtent.X;
	ExtentY.Component(Index) = Bounds.BoxExtent.Y;
	ExtentZ.Component(Index) = Bounds.BoxExtent.Z;
	Radius.Component(Index) = Bounds.SphereRadius;
	MinDistanceSq.Component(Index) = InMinDistance * InMinDistance;
	MinRangeSq.Component(Index) = InMinRange * InMinRange;
	MaxRangeSq.Component(Index) = InMaxRange != FLT_MAX ? (InMaxRange * InMaxRange) : FLT_MAX;
	LastRenderTime.Component(Index) = InLastRenderTime;
}

void FTextureInstanceState::FBounds4::Update(int32 Index, const FBoxSphereBounds& Bounds, float InLastRenderTime)
{
	check(Index >= 0 && Index < 4);

	OriginX.Component(Index) = Bounds.Origin.X;
	OriginY.Component(Index) = Bounds.Origin.Y;
	OriginZ.Component(Index) = Bounds.Origin.Z;
	RangeOriginX.Component(Index) = Bounds.Origin.X;
	RangeOriginY.Component(Index) = Bounds.Origin.Y;
	RangeOriginZ.Component(Index) = Bounds.Origin.Z;
	ExtentX.Component(Index) = Bounds.BoxExtent.X;
	ExtentY.Component(Index) = Bounds.BoxExtent.Y;
	ExtentZ.Component(Index) = Bounds.BoxExtent.Z;
	Radius.Component(Index) = Bounds.SphereRadius;
	MinDistanceSq.Component(Index) = 0;
	MinRangeSq.Component(Index) = 0;
	MaxRangeSq.Component(Index) = FLT_MAX;
	LastRenderTime.Component(Index) = InLastRenderTime;
}

void FTextureInstanceState::FBounds4::Update(int32 Index, float InLastRenderTime)
{
	check(Index >= 0 && Index < 4);

	LastRenderTime.Component(Index) = InLastRenderTime;
}

FTextureInstanceState::FElement::FElement()
:	Component(nullptr)
,	Texture(nullptr)
,	BoundsIndex(INDEX_NONE)
,	TexelFactor(0)
,	PrevTextureLink(INDEX_NONE)
,	NextTextureLink(INDEX_NONE)
,	NextComponentLink(INDEX_NONE)
{
}

FTextureInstanceState::FTextureLinkConstIterator::FTextureLinkConstIterator(const FTextureInstanceState& InState, const UTexture2D* InTexture) 
:	State(InState)
,	CurrElementIndex(INDEX_NONE)
{
	const int32* TextureLink = State.TextureMap.Find(InTexture);
	if (TextureLink)
	{
		CurrElementIndex = *TextureLink;
	}
}

FBoxSphereBounds FTextureInstanceState::FTextureLinkConstIterator::GetBounds() const
{ 
	FBoxSphereBounds Bounds(ForceInitToZero);

	int32 BoundsIndex = State.Elements[CurrElementIndex].BoundsIndex; 
	if (BoundsIndex != INDEX_NONE)
	{
		const FBounds4& TheBounds4 = State.Bounds4[BoundsIndex / 4];
		int32 Index = BoundsIndex % 4;

		Bounds.Origin.X = TheBounds4.OriginX[Index];
		Bounds.Origin.Y = TheBounds4.OriginY[Index];
		Bounds.Origin.Z = TheBounds4.OriginZ[Index];

		Bounds.BoxExtent.X = TheBounds4.ExtentX[Index];
		Bounds.BoxExtent.Y = TheBounds4.ExtentY[Index];
		Bounds.BoxExtent.Z = TheBounds4.ExtentZ[Index];

		Bounds.SphereRadius = TheBounds4.Radius[Index];
	}
	return Bounds;
}

void FTextureInstanceState::FTextureLinkConstIterator::OutputToLog(float MaxNormalizedSize, float MaxNormalizedSize_VisibleOnly, const TCHAR* Prefix) const
{
#if !UE_BUILD_SHIPPING
	const UPrimitiveComponent* Component = GetComponent();
	FBoxSphereBounds Bounds = GetBounds();
	float TexelFactor = GetTexelFactor();

	// Log the component reference.
	if (Component)
	{
		UE_LOG(LogContentStreaming, Log, TEXT("  %sReference= %s"), Prefix, *Component->GetFullName());
	}
	else
	{
		UE_LOG(LogContentStreaming, Log, TEXT("  %sReference"), Prefix);
	}
	
	// Log the wanted mips.
	if (TexelFactor == FLT_MAX)
	{
		UE_LOG(LogContentStreaming, Log, TEXT("    Forced FullyLoad"));
	}
	else if (TexelFactor >= 0)
	{
		if (GIsEditor) // In editor, visibility information is unreliable and we only consider the max.
		{
			UE_LOG(LogContentStreaming, Log, TEXT("    Size=%f, BoundIndex=%d"), TexelFactor * FMath::Max(MaxNormalizedSize, MaxNormalizedSize_VisibleOnly), GetBoundsIndex());
		}
		else if (MaxNormalizedSize_VisibleOnly > 0)
		{
			UE_LOG(LogContentStreaming, Log, TEXT("    OnScreenSize=%f, BoundIndex=%d"), TexelFactor * MaxNormalizedSize_VisibleOnly, GetBoundsIndex());
		}
		else
		{
			const int32 BoundIndex = GetBoundsIndex();
			if (State.Bounds4.IsValidIndex(BoundIndex / 4))
			{
				float LastRenderTime = State.Bounds4[BoundIndex / 4].LastRenderTime[BoundIndex % 4];
				UE_LOG(LogContentStreaming, Log, TEXT("    OffScreenSize=%f, LastRenderTime= %.3f, BoundIndex=%d"), TexelFactor * MaxNormalizedSize, LastRenderTime, BoundIndex);
			}
			else
			{
				UE_LOG(LogContentStreaming, Log, TEXT("    OffScreenSize=%f, BoundIndex=Invalid"), TexelFactor * MaxNormalizedSize);
			}
		}
	}
	else // Negative texel factors relate to forced specific resolution.
	{
		UE_LOG(LogContentStreaming, Log, TEXT("    ForcedSize=%f"), -TexelFactor);
	}

	// Log the bounds
	const bool bUseNewMetrics = CVarStreamingUseNewMetrics.GetValueOnAnyThread() != 0;
	if (bUseNewMetrics)
	{
		if (TexelFactor >= 0 && TexelFactor < FLT_MAX)
		{
			UE_LOG(LogContentStreaming, Log, TEXT("    Origin=(%s), BoxExtent=(%s), TexelSize=%f"), *Bounds.Origin.ToString(), *Bounds.BoxExtent.ToString(), TexelFactor);
		}
		else
		{
			UE_LOG(LogContentStreaming, Log, TEXT("    Origin=(%s), BoxExtent=(%s)"), *Bounds.Origin.ToString(), *Bounds.BoxExtent.ToString());
		}
	}
	else
	{
		if (TexelFactor >= 0 && TexelFactor < FLT_MAX)
		{
			UE_LOG(LogContentStreaming, Log, TEXT("    Origin=(%s), SphereRadius=%f, TexelSize=%f"),  *Bounds.Origin.ToString(), Bounds.SphereRadius, TexelFactor);
		}
		else
		{
			UE_LOG(LogContentStreaming, Log, TEXT("    Origin=(%s), SphereRadius=%f"),  *Bounds.Origin.ToString(), Bounds.SphereRadius);
		}
	}
#endif // !UE_BUILD_SHIPPING
}


int32 FTextureInstanceState::AddBounds(const FBoxSphereBounds& Bounds, const UPrimitiveComponent* InComponent, float LastRenderTime, const FVector4& RangeOrigin, float MinDistance, float MinRange, float MaxRange)
{
	check(InComponent);

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
		FreeBoundIndices.Push(BoundsIndex + 3);
		FreeBoundIndices.Push(BoundsIndex + 2);
		FreeBoundIndices.Push(BoundsIndex + 1);
	}

	Bounds4[BoundsIndex / 4].Set(BoundsIndex % 4, Bounds, LastRenderTime, RangeOrigin, MinDistance, MinRange, MaxRange);
	Bounds4Components[BoundsIndex] = InComponent;

	return BoundsIndex;
}

void FTextureInstanceState::RemoveBounds(int32 BoundsIndex)
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

void FTextureInstanceState::AddElement(const UPrimitiveComponent* InComponent, const UTexture2D* InTexture, int InBoundsIndex, float InTexelFactor, int32*& ComponentLink)
{
	check(InComponent && InTexture);
	CompiledTextureMap.Empty(); // Data is invalid now!

	int32* TextureLink = TextureMap.Find(InTexture);

	// Since textures are processed per component, if there are already elements for this component-texture,
	// they will be in the first entries (as we push to head). If such pair use the same bound, merge the texel factors.
	// The first step is to find if such a duplicate entries exists.

	if (TextureLink)
	{
		int32 ElementIndex = *TextureLink;
		while (ElementIndex != INDEX_NONE)
		{
			FElement& TextureElement = Elements[ElementIndex];

			if (TextureElement.Component == InComponent)
			{
				if (TextureElement.BoundsIndex == InBoundsIndex)
				{
					if (InTexelFactor >= 0 && TextureElement.TexelFactor >= 0)
					{
						// Abort inserting a new element, and merge the 2 entries together.
						TextureElement.TexelFactor = FMath::Max(TextureElement.TexelFactor, InTexelFactor);
						return;
					}
					else if (InTexelFactor < 0 && TextureElement.TexelFactor < 0)
					{
						// Negative are forced resolution.
						TextureElement.TexelFactor = FMath::Min(TextureElement.TexelFactor, InTexelFactor);
						return;
					}
				}

				// Check the next bounds for this component.
				ElementIndex = TextureElement.NextTextureLink;
			}
			else
			{
				break;
			}
		}
	}

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

	if (TextureLink)
	{
		FElement& TextureLinkElement = Elements[*TextureLink];

		// The new inserted element as the head element.
		Element.NextTextureLink = *TextureLink;
		TextureLinkElement.PrevTextureLink = ElementIndex;
		*TextureLink = ElementIndex;
	}
	else
	{
		TextureMap.Add(InTexture, ElementIndex);
	}

	// Simple sanity check to ensure that the component link passed in param is the right one
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

void FTextureInstanceState::RemoveElement(int32 ElementIndex, int32& NextComponentLink, int32& BoundsIndex, const UTexture2D*& Texture)
{
	CompiledTextureMap.Empty(); // Data is invalid now!

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

static bool operator==(const FBoxSphereBounds& A, const FBoxSphereBounds& B)
{
	return A.Origin == B.Origin && A.BoxExtent == B.BoxExtent &&  A.SphereRadius == B.SphereRadius;
}

void FTextureInstanceState::AddComponent(const UPrimitiveComponent* Component, FStreamingTextureLevelContext& LevelContext)
{
	TArray<FStreamingTexturePrimitiveInfo> TextureInstanceInfos;
	Component->GetStreamingTextureInfoWithNULLRemoval(LevelContext, TextureInstanceInfos);
	// Texture entries are garantied to be relevant here!

	if (TextureInstanceInfos.Num())
	{
		const UPrimitiveComponent* LODParent = Component->GetLODParentPrimitive();

		// AddElement will handle duplicate texture-bound-component. Here we just have to prevent creating identical bound-component.
		TArray<int32, TInlineAllocator<12> > BoundsIndices;

		int32* ComponentLink = ComponentMap.Find(Component);
		for (int32 TextureIndex = 0; TextureIndex  < TextureInstanceInfos.Num(); ++TextureIndex)
		{
			const FStreamingTexturePrimitiveInfo& Info = TextureInstanceInfos[TextureIndex];

			int32 BoundsIndex = INDEX_NONE;

			// Find an identical bound, or create a new entry.
			{
				for (int32 TestIndex = TextureIndex - 1; TestIndex >= 0; --TestIndex)
				{
					const FStreamingTexturePrimitiveInfo& TestInfo = TextureInstanceInfos[TestIndex];
					if (Info.Bounds == TestInfo.Bounds && BoundsIndices[TestIndex] != INDEX_NONE)
					{
						BoundsIndex = BoundsIndices[TestIndex];
						break;
					}
				}

				if (BoundsIndex == INDEX_NONE)
				{
					// In the engine, the MinDistance is computed from the component bound center to the viewpoint.
					// The streaming computes the distance as the distance from viewpoint to the edge of the texture bound box.
					// The implementation also handles MinDistance by bounding the distance to it so that if the viewpoint gets closer the screen size will stop increasing at some point.
					// The fact that the primitive will disappear is not so relevant as this will be handled by the visibility logic, normally streaming one less mip than requested.
					// The important mather is to control the requested mip by limiting the distance, since at close up, the distance becomes very small and all mips are streamer (even after the 1 mip bias).

					float MinDistance = FMath::Max<float>(0, Component->MinDrawDistance - (Info.Bounds.Origin - Component->Bounds.Origin).Size() - Info.Bounds.SphereRadius);
					float MinRange = FMath::Max<float>(0, Component->MinDrawDistance);
					float MaxRange = FLT_MAX;
					if (LODParent)
					{
						// Max distance when HLOD becomes visible.
						MaxRange = LODParent->MinDrawDistance + (Component->Bounds.Origin - LODParent->Bounds.Origin).Size();
					}
					BoundsIndex = AddBounds(Info.Bounds, Component, Component->LastRenderTimeOnScreen, Component->Bounds.Origin, MinDistance, MinRange, MaxRange);
				}
				BoundsIndices.Push(BoundsIndex);
			}

			// Handle force mip streaming by over scaling the texel factor. 
			AddElement(Component, Info.Texture, BoundsIndex, Component->bForceMipStreaming ? FLT_MAX : Info.TexelFactor, ComponentLink);
		}
	}
}

void FTextureInstanceState::AddComponent(const UPrimitiveComponent* Component)
{
	FStreamingTextureLevelContext LevelContext;
	TArray<FStreamingTexturePrimitiveInfo> TextureInstanceInfos;
	Component->GetStreamingTextureInfoWithNULLRemoval(LevelContext, TextureInstanceInfos);

	if (TextureInstanceInfos.Num())
	{
		int32 BoundsIndex = AddBounds(Component->Bounds, Component, Component->LastRenderTimeOnScreen);
		int32* ComponentLink = ComponentMap.Find(Component);
		for (const FStreamingTexturePrimitiveInfo& Info : TextureInstanceInfos)
		{
			AddElement(Component, Info.Texture, BoundsIndex, Info.TexelFactor, ComponentLink);
		}
	}
}

void FTextureInstanceState::RemoveComponent(const UPrimitiveComponent* Component, FRemovedTextureArray& RemovedTextures)
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

	for (int32 I = 0; I < RemovedBoundsIndices.Num(); ++I)
	{
		RemoveBounds(RemovedBoundsIndices[I]);
	}
}

void FTextureInstanceState::RemoveComponentReferences(const UPrimitiveComponent* Component) 
{ 
	// Because the async task could be running, we can't change the async view state. 
	// We limit ourself to clearing the component ptr to avoid invalid access when updating visibility.

	int32* ComponentLink = ComponentMap.Find(Component);
	if (ComponentLink)
	{
		int32 ElementIndex = *ComponentLink;
		while (ElementIndex != INDEX_NONE)
		{
			FElement& Element = Elements[ElementIndex];
			if (Element.BoundsIndex != INDEX_NONE)
			{
				Bounds4Components[Element.BoundsIndex] = nullptr;
			}
			Element.Component = nullptr;

			ElementIndex = Element.NextComponentLink;
		}
	}
	ComponentMap.Remove(Component);
}

void FTextureInstanceState::UpdateBounds(const UPrimitiveComponent* Component)
{
	int32* ComponentLink = ComponentMap.Find(Component);
	if (ComponentLink)
	{
		int32 ElementIndex = *ComponentLink;
		while (ElementIndex != INDEX_NONE)
		{
			const FElement& Element = Elements[ElementIndex];
			if (Element.BoundsIndex != INDEX_NONE)
			{
				Bounds4[Element.BoundsIndex / 4].Update(Element.BoundsIndex % 4, Component->Bounds, Component->LastRenderTimeOnScreen);
			}
			ElementIndex = Element.NextComponentLink;
		}
	}
}

void FTextureInstanceState::UpdateBounds(int32 BoundIndex)
{
	const UPrimitiveComponent* Component = Bounds4Components[BoundIndex];
	if (Component)
	{
		Bounds4[BoundIndex / 4].Update(BoundIndex % 4, Component->Bounds, Component->LastRenderTimeOnScreen);
	}
}

void FTextureInstanceState::UpdateLastRenderTime(int32 BoundIndex)
{
	const UPrimitiveComponent* Component = Bounds4Components[BoundIndex];
	if (Component)
	{
		Bounds4[BoundIndex / 4].Update(BoundIndex % 4, Component->LastRenderTimeOnScreen);
	}
}

uint32 FTextureInstanceState::GetAllocatedSize() const
{
	int32 CompiledElementsSize = 0;
	for (TMap<const UTexture2D*, TArray<FCompiledElement> >::TConstIterator It(CompiledTextureMap); It; ++It)
	{
		CompiledElementsSize += It.Value().GetAllocatedSize();
	}

	return Bounds4.GetAllocatedSize() +
		Bounds4Components.GetAllocatedSize() +
		Elements.GetAllocatedSize() +
		FreeBoundIndices.GetAllocatedSize() +
		FreeElementIndices.GetAllocatedSize() +
		TextureMap.GetAllocatedSize() +
		CompiledTextureMap.GetAllocatedSize() + CompiledElementsSize +
		ComponentMap.GetAllocatedSize();
}

void FStaticTextureInstanceManager::IncrementalUpdate(float Percentage)
{
	const int32 LastIndex = FMath::Min(State->NumBounds(), DirtyIndex + FMath::CeilToInt((float)State->NumBounds() * Percentage));
	for (int32 Index = DirtyIndex; Index < LastIndex; ++Index)
	{
		State->UpdateLastRenderTime(Index);
	}
	DirtyIndex = LastIndex;
}

void FStaticTextureInstanceManager::NormalizeLightmapTexelFactor()
{
	TArray<float> TexelFactors;

	for (auto TextureIt = State->GetTextureIterator(); TextureIt; ++TextureIt)
	{
		const UTexture2D* Texture = *TextureIt;

		if (Texture->LODGroup == TEXTUREGROUP_Lightmap || Texture->LODGroup == TEXTUREGROUP_Shadowmap)
		{
			TexelFactors.Reset();
			for (auto ElementIt = State->GetElementIterator(Texture); ElementIt ; ++ElementIt)
			{
				TexelFactors.Push(ElementIt.GetTexelFactor());
			}
			TexelFactors.Sort();

			const float MinTexelFactor = TexelFactors[FMath::FloorToInt(TexelFactors.Num() * 0.2f)];
			const float MaxTexelFactor = TexelFactors[FMath::FloorToInt(TexelFactors.Num() * 0.8f)];

			for (auto ElementIt = State->GetElementIterator(Texture); ElementIt ; ++ElementIt)
			{
				ElementIt.ClampTexelFactor(MinTexelFactor, MaxTexelFactor);
			}
		}
	}
}

void FTextureInstanceState::CompileElements()
{
	CompiledTextureMap.Empty();

	// First create an entry for all elements, so that there are no reallocs when inserting each compiled elements.
	for (TMap<const UTexture2D*, int32>::TConstIterator TextureIt(TextureMap); TextureIt; ++TextureIt)
	{
		const UTexture2D* Texture = TextureIt.Key();
		CompiledTextureMap.Add(Texture);
	}

	// Then fill in each array.
	for (TMap<const UTexture2D*, TArray<FCompiledElement> >::TIterator It(CompiledTextureMap); It; ++It)
	{
		const UTexture2D* Texture = It.Key();
		TArray<FCompiledElement>& CompiledElemements = It.Value();

		int32 CompiledElementCount = 0;
		for (auto ElementIt = GetElementIterator(Texture); ElementIt; ++ElementIt)
		{
			++CompiledElementCount;
		}
		CompiledElemements.AddUninitialized(CompiledElementCount);

		CompiledElementCount = 0;
		for (auto ElementIt = GetElementIterator(Texture); ElementIt; ++ElementIt)
		{
			CompiledElemements[CompiledElementCount].BoundsIndex = ElementIt.GetBoundsIndex();
			CompiledElemements[CompiledElementCount].TexelFactor = ElementIt.GetTexelFactor();
			++CompiledElementCount;
		}
	}
}

void FDynamicTextureInstanceManager::IncrementalUpdate(float Percentage)
{
	const int32 LastIndex = FMath::Min(State->NumBounds(), DirtyIndex + FMath::CeilToInt((float)State->NumBounds() * Percentage));
	for (int32 Index = DirtyIndex; Index < LastIndex; ++Index)
	{
		State->UpdateBounds(Index);
	}
	DirtyIndex = LastIndex;
}

TRefCountPtr<const FTextureInstanceState> FDynamicTextureInstanceManager::GetAsyncState()
{
	QUICK_SCOPE_CYCLE_COUNTER(FDynamicTextureInstanceManager_GetAsyncState);

	FTextureInstanceState* AsyncState = new FTextureInstanceState();

	AsyncState->Bounds4.AddUninitialized(State->Bounds4.Num());
	// Pass the bounds to the async state and mark the state bounds as dirty.
	FMemory::Memswap(&AsyncState->Bounds4, &State->Bounds4, sizeof(AsyncState->Bounds4));
	DirtyIndex = 0;

	// Copy the map, and keep the lean version
	AsyncState->TextureMap = State->TextureMap;
	FMemory::Memswap(&AsyncState->TextureMap , &State->TextureMap, sizeof(AsyncState->TextureMap));

	// Copy the elements
	AsyncState->Elements = State->Elements;

	return TRefCountPtr<const FTextureInstanceState>(AsyncState);
}

void FTextureInstanceAsyncView::UpdateBoundSizes_Async(const TArray<FStreamingViewInfo>& ViewInfos, float LastUpdateTime, const FTextureStreamingSettings& Settings)
{
	if (!State.IsValid())  return;

	const int32 NumViews = ViewInfos.Num();
	const VectorRegister One4 = VectorSet(1.f, 1.f, 1.f, 1.f);
	const int32 NumBounds4 = State->NumBounds4();

	const VectorRegister LastUpdateTime4 = VectorSet(LastUpdateTime, LastUpdateTime, LastUpdateTime, LastUpdateTime);

	BoundsViewInfo.Empty(NumBounds4 * 4);
	BoundsViewInfo.AddUninitialized(NumBounds4 * 4);

	for (int32 Bounds4Index = 0; Bounds4Index < NumBounds4; ++Bounds4Index)
	{
		const FTextureInstanceState::FBounds4& CurrentBounds4 = State->GetBounds4(Bounds4Index);

		// Calculate distance of viewer to bounding sphere.
		const VectorRegister OriginX = VectorLoadAligned( &CurrentBounds4.OriginX );
		const VectorRegister OriginY = VectorLoadAligned( &CurrentBounds4.OriginY );
		const VectorRegister OriginZ = VectorLoadAligned( &CurrentBounds4.OriginZ );
		const VectorRegister RangeOriginX = VectorLoadAligned( &CurrentBounds4.RangeOriginX );
		const VectorRegister RangeOriginY = VectorLoadAligned( &CurrentBounds4.RangeOriginY );
		const VectorRegister RangeOriginZ = VectorLoadAligned( &CurrentBounds4.RangeOriginZ );
		const VectorRegister ExtentX = VectorLoadAligned( &CurrentBounds4.ExtentX );
		const VectorRegister ExtentY = VectorLoadAligned( &CurrentBounds4.ExtentY );
		const VectorRegister ExtentZ = VectorLoadAligned( &CurrentBounds4.ExtentZ );
		const VectorRegister Radius = VectorLoadAligned( &CurrentBounds4.Radius );
		const VectorRegister MinDistanceSq = VectorLoadAligned( &CurrentBounds4.MinDistanceSq );
		const VectorRegister MinRangeSq = VectorLoadAligned( &CurrentBounds4.MinRangeSq );
		const VectorRegister MaxRangeSq = VectorLoadAligned(&CurrentBounds4.MaxRangeSq);
		const VectorRegister LastRenderTime = VectorLoadAligned(&CurrentBounds4.LastRenderTime);
		
		VectorRegister MaxNormalizedSize = VectorZero();
		VectorRegister MaxNormalizedSize_VisibleOnly = VectorZero();

		for (int32 ViewIndex = 0; ViewIndex < NumViews; ++ViewIndex)
		{
			const FStreamingViewInfo& ViewInfo = ViewInfos[ViewIndex];

			const float EffectiveScreenSize = (Settings.MaxEffectiveScreenSize > 0.0f) ? FMath::Min(Settings.MaxEffectiveScreenSize, ViewInfo.ScreenSize) : ViewInfo.ScreenSize;
			const float ScreenSizeFloat = EffectiveScreenSize * ViewInfo.BoostFactor * .5f; // Multiply by half since the ratio factors map to half the screen only

			const VectorRegister ScreenSize = VectorLoadFloat1( &ScreenSizeFloat );
			const VectorRegister ViewOriginX = VectorLoadFloat1( &ViewInfo.ViewOrigin.X );
			const VectorRegister ViewOriginY = VectorLoadFloat1( &ViewInfo.ViewOrigin.Y );
			const VectorRegister ViewOriginZ = VectorLoadFloat1( &ViewInfo.ViewOrigin.Z );

			VectorRegister DistSqMinusRadiusSq = VectorZero();
			if (Settings.bUseNewMetrics)
			{
				// In this case DistSqMinusRadiusSq will contain the distance to the box^2
				VectorRegister Temp = VectorSubtract( ViewOriginX, OriginX );
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
				VectorRegister Temp = VectorSubtract( ViewOriginX, OriginX );
				VectorRegister DistSq = VectorMultiply( Temp, Temp );
				Temp = VectorSubtract( ViewOriginY, OriginY );
				DistSq = VectorMultiplyAdd( Temp, Temp, DistSq );
				Temp = VectorSubtract( ViewOriginZ, OriginZ );
				DistSq = VectorMultiplyAdd( Temp, Temp, DistSq );

				DistSqMinusRadiusSq = VectorMultiply( Radius, Radius );
				DistSqMinusRadiusSq = VectorSubtract( DistSq, DistSqMinusRadiusSq );
				// This can be negative here!!!
			}

			// If the bound is not visible up close, limit the distance to it's minimal possible range.
			VectorRegister ClampedDistSq = VectorMax( MinDistanceSq, DistSqMinusRadiusSq );

			// Compute in range  Squared distance between range.
			VectorRegister InRangeMask;
			{
				VectorRegister Temp = VectorSubtract( ViewOriginX, RangeOriginX );
				VectorRegister RangeDistSq = VectorMultiply( Temp, Temp );
				Temp = VectorSubtract( ViewOriginY, RangeOriginY );
				RangeDistSq = VectorMultiplyAdd( Temp, Temp, RangeDistSq );
				Temp = VectorSubtract( ViewOriginZ, RangeOriginZ );
				RangeDistSq = VectorMultiplyAdd( Temp, Temp, RangeDistSq );

				VectorRegister ClampedRangeDistSq = VectorMax( MinRangeSq, RangeDistSq );
				ClampedRangeDistSq = VectorMin( MaxRangeSq, ClampedRangeDistSq );
				InRangeMask = VectorCompareEQ( RangeDistSq, ClampedRangeDistSq); // If the clamp dist is equal, then it was in range.
			}

			ClampedDistSq = VectorMax(ClampedDistSq, One4); // Prevents / 0
			VectorRegister ScreenSizeOverDistance = VectorReciprocalSqrt(ClampedDistSq);
			ScreenSizeOverDistance = VectorMultiply(ScreenSizeOverDistance, ScreenSize);

			MaxNormalizedSize = VectorMax(ScreenSizeOverDistance, MaxNormalizedSize);

			// Now mask to zero if not in range, or not seen recently.
			ScreenSizeOverDistance = VectorSelect(InRangeMask, ScreenSizeOverDistance, VectorZero());
			ScreenSizeOverDistance = VectorSelect(VectorCompareGT(LastRenderTime, LastUpdateTime4), ScreenSizeOverDistance, VectorZero());

			MaxNormalizedSize_VisibleOnly = VectorMax(ScreenSizeOverDistance, MaxNormalizedSize_VisibleOnly);
		}

		// Store results
		FBoundsViewInfo* BoundsVieWInfo = &BoundsViewInfo[Bounds4Index * 4];
		for (int32 SubIndex = 0; SubIndex < 4; ++SubIndex)
		{
			BoundsVieWInfo[SubIndex].MaxNormalizedSize = VectorGetComponent(MaxNormalizedSize, SubIndex);
			BoundsVieWInfo[SubIndex].MaxNormalizedSize_VisibleOnly = VectorGetComponent(MaxNormalizedSize_VisibleOnly, SubIndex);
		}
	}
}

void FTextureInstanceAsyncView::ProcessElement(const FBoundsViewInfo& BoundsVieWInfo, float TexelFactor, float& MaxSize, float& MaxSize_VisibleOnly) const
{
	if (TexelFactor == FLT_MAX) // If this is a forced load component.
	{
		MaxSize = BoundsVieWInfo.MaxNormalizedSize > 0 ? FLT_MAX : MaxSize;
		MaxSize_VisibleOnly = BoundsVieWInfo.MaxNormalizedSize_VisibleOnly > 0 ? FLT_MAX : MaxSize_VisibleOnly;
	}
	else if (TexelFactor >= 0)
	{
		MaxSize = FMath::Max(MaxSize, TexelFactor * BoundsVieWInfo.MaxNormalizedSize);
		MaxSize_VisibleOnly = FMath::Max(MaxSize_VisibleOnly, TexelFactor * BoundsVieWInfo.MaxNormalizedSize_VisibleOnly);
	}
	else // Negative texel factors map to fixed resolution. Currently used for lanscape.
	{
		MaxSize = FMath::Max(MaxSize, -TexelFactor);
		MaxSize_VisibleOnly = FMath::Max(MaxSize_VisibleOnly, -TexelFactor);
	}
}

void FTextureInstanceAsyncView::GetTexelSize(const UTexture2D* InTexture, float& MaxSize, float& MaxSize_VisibleOnly, const TCHAR* LogPrefix) const
{
	// No need to iterate more if texture is already at maximum resolution.

	int32 CurrCount = 0;

	if (State.IsValid())
	{
		// Use the fast path if available, about twice as fast when there are a lot of elements.
		if (State->HasCompiledElements() && !LogPrefix)
		{
			const TArray<FTextureInstanceState::FCompiledElement>* CompiledElements = State->GetCompiledElements(InTexture);
			if (CompiledElements)
			{
				const int32 NumCompiledElements = CompiledElements->Num();
				const FTextureInstanceState::FCompiledElement* CompiledElementData = CompiledElements->GetData();

				int32 CompiledElementIndex = 0;
				while (CompiledElementIndex < NumCompiledElements && MaxSize_VisibleOnly < MAX_TEXTURE_SIZE)
				{
					const FTextureInstanceState::FCompiledElement& CompiledElement = CompiledElementData[CompiledElementIndex];
					ProcessElement(BoundsViewInfo[CompiledElement.BoundsIndex], CompiledElement.TexelFactor, MaxSize, MaxSize_VisibleOnly);
					++CompiledElementIndex;
				}

				if (MaxSize_VisibleOnly >= MAX_TEXTURE_SIZE && CompiledElementIndex > 1)
				{
					// This does not realloc anything but moves the close by element to the first entry, making the next update find it immediately.
					FTextureInstanceState::FCompiledElement* SwapElementData = const_cast<FTextureInstanceState::FCompiledElement*>(CompiledElementData);
					Swap<FTextureInstanceState::FCompiledElement>(SwapElementData[0], SwapElementData[CompiledElementIndex - 1]);
				}
			}
		}
		else
		{
			for (auto It = State->GetElementIterator(InTexture); It && (MaxSize_VisibleOnly < MAX_TEXTURE_SIZE || LogPrefix); ++It)
			{
				const FBoundsViewInfo& BoundsVieWInfo = BoundsViewInfo[It.GetBoundsIndex()];
				ProcessElement(BoundsVieWInfo, It.GetTexelFactor(), MaxSize, MaxSize_VisibleOnly);
				if (LogPrefix)
				{
					It.OutputToLog(BoundsVieWInfo.MaxNormalizedSize, BoundsVieWInfo.MaxNormalizedSize_VisibleOnly, LogPrefix);
				}
			}
		}
	}
}

bool FTextureInstanceAsyncView::HasTextureReferences(const UTexture2D* InTexture) const
{
	return State.IsValid() && (bool)State->GetElementIterator(InTexture);
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
			DynamicInstances.RemoveComponent(Component, RemovedTextures);
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
			DynamicInstances.RemoveComponent(Component, RemovedTextures);
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
		DynamicInstances.RemoveComponent(Component, RemovedTextures);
		State->bHasTextures = false;
		DirtyComponents.Add(Component);
	}
}

void FDynamicComponentTextureManager::IncrementalUpdate(FRemovedTextureArray& RemovedTextures, float Percentage)
{
	QUICK_SCOPE_CYCLE_COUNTER(FDynamicComponentTextureManager_IncrementalUpdate);

	// This incrementally updates the bounds for all components.
	// Unrelated to the dirty component logics, which recomputes the texture for each components.
	DynamicInstances.IncrementalUpdate(Percentage);

	TSet<const UPrimitiveComponent*> NextDirtyComponents; // Components that have a dirty states (to accelerate the update).
	for (TSet<const UPrimitiveComponent*>::TIterator It(DirtyComponents); It; ++It)
	{
		const UPrimitiveComponent* Component = *It;

		FComponentState* State = ComponentStates.Find(Component);
		if (!State) continue;

		if (State->bToDelete)
		{
			check(!State->bHasTextures); // Textures are removed when the bToDelete flag is set.
			ComponentStates.Remove(Component);
		}
		else
		{
			bool bIsInScene = Component->IsRegistered();
			if (!bIsInScene)
			{
				if (State->bHasTextures)
				{
					DynamicInstances.RemoveComponent(Component, RemovedTextures);
					State->bHasTextures = false;
				}
				NextDirtyComponents.Add(Component);
			}
			else if (bIsInScene && !State->bHasTextures)
			{
				DynamicInstances.AddComponent(Component);
				State->bHasTextures = true;
			}
			else
			{
				DynamicInstances.UpdateBounds(Component);
			}
		}
	}

	FMemory::Memswap(&DirtyComponents, &NextDirtyComponents, sizeof(DirtyComponents));
}

uint32 FDynamicComponentTextureManager::GetAllocatedSize() const
{
	return ComponentStates.GetAllocatedSize() + DirtyComponents.GetAllocatedSize() + DynamicInstances.GetAllocatedSize();
}

void FLevelTextureManager::Remove(FDynamicComponentTextureManager& DynamicComponentManager, FRemovedTextureArray& RemovedTextures)
{ 
	// Mark all static textures for removal.
	for (auto It = StaticInstances.GetTextureIterator(); It; ++It)
	{
		RemovedTextures.Push(*It);
	}
	StaticInstances = FStaticTextureInstanceManager();

	// Mark all dynamic textures for removal.
	for (const UPrimitiveComponent* Component : DynamicComponents)
	{
		DynamicComponentManager.Remove(Component, RemovedTextures);
	}	
	DynamicComponents.Empty();

	bToDelete = true; 
}

float FLevelTextureManager::GetWorldTime() const
{
	if (!GIsEditor && Level)
	{
		UWorld* World = Level->GetWorld();

		// When paused, updating the world time sometimes break visibility logic.
		if (World && !World->IsPaused())
		{
			return World->GetTimeSeconds();
		}
	}
	return 0;
}

void FLevelTextureManager::IncrementalUpdate(FDynamicComponentTextureManager& DynamicComponentManager, FRemovedTextureArray& RemovedTextures, float Percentage, bool bUseDynamicStreaming) 
{
	QUICK_SCOPE_CYCLE_COUNTER(FStaticComponentTextureManager_IncrementalUpdate);

	check(!bToDelete);

	StaticInstances.IncrementalUpdate(Percentage);

	if (!bHasTextures)
	{
		FStreamingTextureLevelContext LevelContext(Level);

		// Process all components of the level.
		TArray<UObject*> ObjectsInOuter;
		GetObjectsWithOuter(Level, ObjectsInOuter);

		for (UObject* LevelObject : ObjectsInOuter)
		{
			UPrimitiveComponent* Component = Cast<UPrimitiveComponent>(LevelObject);

			// Skip non primitive components, default objects and non registered objects.
			if (!Component || Component->IsTemplate(RF_ClassDefaultObject) || !Component->IsRegistered()) 
				continue;

			const AActor* const Owner = Component->GetOwner();
			const bool bIsStatic = (!Owner || Component->Mobility == EComponentMobility::Static || Component->Mobility == EComponentMobility::Stationary);

			if (bIsStatic)
			{
				StaticInstances.AddComponent(Component, LevelContext);
			}
			else if (bUseDynamicStreaming)
			{
				DynamicComponentManager.Add(Component, DPT_Level, RemovedTextures);

				// This will be used when the level is removed. Never dereferenced.
				DynamicComponents.Add(Component);
			}

		}

		StaticInstances.NormalizeLightmapTexelFactor();
		StaticInstances.CompiledElements();

		bHasTextures = true;
	}
}

uint32 FLevelTextureManager::GetAllocatedSize() const
{
	return StaticInstances.GetAllocatedSize() + DynamicComponents.GetAllocatedSize();
}
