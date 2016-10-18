// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SlateCorePrivatePCH.h"
#include "ReflectionMetadata.h"

DECLARE_CYCLE_STAT(TEXT("FSlateDrawElement::Make Time"), STAT_SlateDrawElementMakeTime, STATGROUP_SlateVerbose);

DEFINE_STAT(STAT_SlateBufferPoolMemory);

FSlateShaderResourceManager* FSlateDataPayload::ResourceManager;

void FSlateDataPayload::SetTextPayloadProperties( FSlateWindowElementList& ElementList, const FString& InText, const FSlateFontInfo& InFontInfo, const FLinearColor& InTint, const int32 InStartIndex, const int32 InEndIndex )
{
	Tint = InTint;
	FontInfo = InFontInfo;
	SIZE_T Count = InText.Len() + 1;
	int32 StartIndex = FMath::Min<int32>(InStartIndex, Count - 1);
	int32 EndIndex = FMath::Min<int32>(InEndIndex, Count - 1);
	Count = 1 + ((EndIndex > StartIndex) ? EndIndex - StartIndex : 0);
	ImmutableText = (TCHAR*)ElementList.Alloc(sizeof(TCHAR) * Count, ALIGNOF(TCHAR));
	if (Count > 1)
	{
		FCString::Strncpy(ImmutableText, InText.GetCharArray().GetData() + StartIndex, Count);
		check(!ImmutableText[Count - 1]);
	}
	else
	{
		ImmutableText[0] = 0;
	}
}


void FSlateDrawElement::Init(uint32 InLayer, const FPaintGeometry& PaintGeometry, const FSlateRect& InClippingRect, ESlateDrawEffect::Type InDrawEffects)
{
	RenderTransform = PaintGeometry.GetAccumulatedRenderTransform();
	Position = PaintGeometry.DrawPosition;
	LocalSize = PaintGeometry.GetLocalSize();
	ClippingRect = InClippingRect;
	Layer = InLayer;
	Scale = PaintGeometry.DrawScale;
	DrawEffects = InDrawEffects;
	extern SLATECORE_API TOptional<FShortRect> GSlateScissorRect;
	ScissorRect = GSlateScissorRect;

	DataPayload.BatchFlags = ESlateBatchDrawFlag::None;
	if ( InDrawEffects & ESlateDrawEffect::NoBlending )
	{
		DataPayload.BatchFlags |= ESlateBatchDrawFlag::NoBlending;
	}
	if ( InDrawEffects & ESlateDrawEffect::PreMultipliedAlpha )
	{
		DataPayload.BatchFlags |= ESlateBatchDrawFlag::PreMultipliedAlpha;
	}
	if ( InDrawEffects & ESlateDrawEffect::NoGamma )
	{
		DataPayload.BatchFlags |= ESlateBatchDrawFlag::NoGamma;
	}
}

void FSlateDrawElement::MakeDebugQuad( FSlateWindowElementList& ElementList, uint32 InLayer, const FPaintGeometry& PaintGeometry, const FSlateRect& InClippingRect)
{
	SCOPE_CYCLE_COUNTER( STAT_SlateDrawElementMakeTime )
	PaintGeometry.CommitTransformsIfUsingLegacyConstructor();
	FSlateDrawElement& DrawElt = ElementList.AddUninitialized();
	DrawElt.Init(InLayer, PaintGeometry, InClippingRect, ESlateDrawEffect::None);
	DrawElt.ElementType = ET_DebugQuad;
}


void FSlateDrawElement::MakeBox( 
	FSlateWindowElementList& ElementList,
	uint32 InLayer, 
	const FPaintGeometry& PaintGeometry, 
	const FSlateBrush* InBrush, 
	const FSlateRect& InClippingRect, 
	ESlateDrawEffect::Type InDrawEffects, 
	const FLinearColor& InTint )
{
	SCOPE_CYCLE_COUNTER( STAT_SlateDrawElementMakeTime )
	PaintGeometry.CommitTransformsIfUsingLegacyConstructor();
	FSlateDrawElement& DrawElt = ElementList.AddUninitialized();
	DrawElt.Init(InLayer, PaintGeometry, InClippingRect, InDrawEffects);
	DrawElt.ElementType = (InBrush->DrawAs == ESlateBrushDrawType::Border) ? ET_Border : ET_Box;
	DrawElt.DataPayload.SetBoxPayloadProperties( InBrush, InTint );
}

void FSlateDrawElement::MakeBox( 
	FSlateWindowElementList& ElementList,
	uint32 InLayer, 
	const FPaintGeometry& PaintGeometry, 
	const FSlateBrush* InBrush, 
	const FSlateResourceHandle& InRenderingHandle,
	const FSlateRect& InClippingRect, 
	ESlateDrawEffect::Type InDrawEffects, 
	const FLinearColor& InTint )
{
	SCOPE_CYCLE_COUNTER(STAT_SlateDrawElementMakeTime)

	// Ignore invalid rendering handles.
	if ( !InRenderingHandle.IsValid() )
	{
		return;
	}

	FSlateShaderResourceProxy* RenderingProxy = InRenderingHandle.Data->Proxy;

	PaintGeometry.CommitTransformsIfUsingLegacyConstructor();
	FSlateDrawElement& DrawElt = ElementList.AddUninitialized();
	DrawElt.Init(InLayer, PaintGeometry, InClippingRect, InDrawEffects);
	DrawElt.ElementType = (InBrush->DrawAs == ESlateBrushDrawType::Border) ? ET_Border : ET_Box;
	DrawElt.DataPayload.SetBoxPayloadProperties( InBrush, InTint, RenderingProxy );
}

void FSlateDrawElement::MakeRotatedBox( 
	FSlateWindowElementList& ElementList,
	uint32 InLayer, 
	const FPaintGeometry& PaintGeometry, 
	const FSlateBrush* InBrush, 
	const FSlateRect& InClippingRect, 
	ESlateDrawEffect::Type InDrawEffects, 
	float Angle,
	TOptional<FVector2D> InRotationPoint,
	ERotationSpace RotationSpace,
	const FLinearColor& InTint )
{
	SCOPE_CYCLE_COUNTER( STAT_SlateDrawElementMakeTime )
	PaintGeometry.CommitTransformsIfUsingLegacyConstructor();
	FSlateDrawElement& DrawElt = ElementList.AddUninitialized();
	DrawElt.Init(InLayer, PaintGeometry, InClippingRect, InDrawEffects);
	DrawElt.ElementType = (InBrush->DrawAs == ESlateBrushDrawType::Border) ? ET_Border : ET_Box;

	FVector2D RotationPoint = GetRotationPoint( PaintGeometry, InRotationPoint, RotationSpace );
	DrawElt.DataPayload.SetRotatedBoxPayloadProperties( InBrush, Angle, RotationPoint, InTint );
}


void FSlateDrawElement::MakeText( FSlateWindowElementList& ElementList, uint32 InLayer, const FPaintGeometry& PaintGeometry, const FString& InText, const int32 StartIndex, const int32 EndIndex, const FSlateFontInfo& InFontInfo, const FSlateRect& InClippingRect,ESlateDrawEffect::Type InDrawEffects, const FLinearColor& InTint )
{
	SCOPE_CYCLE_COUNTER( STAT_SlateDrawElementMakeTime )
	PaintGeometry.CommitTransformsIfUsingLegacyConstructor();
	FSlateDrawElement& DrawElt = ElementList.AddUninitialized();
	DrawElt.Init(InLayer, PaintGeometry, InClippingRect, InDrawEffects);
	DrawElt.ElementType = ET_Text;
	DrawElt.DataPayload.SetTextPayloadProperties( ElementList, InText, InFontInfo, InTint, StartIndex, EndIndex );
}


void FSlateDrawElement::MakeText( FSlateWindowElementList& ElementList, uint32 InLayer, const FPaintGeometry& PaintGeometry, const FString& InText, const FSlateFontInfo& InFontInfo, const FSlateRect& InClippingRect,ESlateDrawEffect::Type InDrawEffects, const FLinearColor& InTint )
{
	SCOPE_CYCLE_COUNTER( STAT_SlateDrawElementMakeTime )
	PaintGeometry.CommitTransformsIfUsingLegacyConstructor();
	FSlateDrawElement& DrawElt = ElementList.AddUninitialized();
	DrawElt.Init(InLayer, PaintGeometry, InClippingRect, InDrawEffects);
	DrawElt.ElementType = ET_Text;
	DrawElt.DataPayload.SetTextPayloadProperties(  ElementList, InText, InFontInfo, InTint );
}


void FSlateDrawElement::MakeText( FSlateWindowElementList& ElementList, uint32 InLayer, const FPaintGeometry& PaintGeometry, const FText& InText, const FSlateFontInfo& InFontInfo, const FSlateRect& InClippingRect,ESlateDrawEffect::Type InDrawEffects, const FLinearColor& InTint )
{
	SCOPE_CYCLE_COUNTER( STAT_SlateDrawElementMakeTime )
	PaintGeometry.CommitTransformsIfUsingLegacyConstructor();
	FSlateDrawElement& DrawElt = ElementList.AddUninitialized();
	DrawElt.Init(InLayer, PaintGeometry, InClippingRect, InDrawEffects);
	DrawElt.ElementType = ET_Text;
	//fixme, alloc here 
	DrawElt.DataPayload.SetTextPayloadProperties(  ElementList, InText.ToString(), InFontInfo, InTint );
}

void FSlateDrawElement::MakeShapedText( FSlateWindowElementList& ElementList, uint32 InLayer, const FPaintGeometry& PaintGeometry, const FShapedGlyphSequenceRef& InShapedGlyphSequence, const FSlateRect& InClippingRect, ESlateDrawEffect::Type InDrawEffects, const FLinearColor& InTint )
{
	SCOPE_CYCLE_COUNTER( STAT_SlateDrawElementMakeTime )
	PaintGeometry.CommitTransformsIfUsingLegacyConstructor();
	FSlateDrawElement& DrawElt = ElementList.AddUninitialized();
	DrawElt.Init(InLayer, PaintGeometry, InClippingRect, InDrawEffects);
	DrawElt.ElementType = ET_ShapedText;
	DrawElt.DataPayload.SetShapedTextPayloadProperties(InShapedGlyphSequence, InTint);
}

void FSlateDrawElement::MakeGradient( FSlateWindowElementList& ElementList, uint32 InLayer, const FPaintGeometry& PaintGeometry, TArray<FSlateGradientStop> InGradientStops, EOrientation InGradientType, const FSlateRect& InClippingRect, ESlateDrawEffect::Type InDrawEffects )
{
	SCOPE_CYCLE_COUNTER( STAT_SlateDrawElementMakeTime )
	PaintGeometry.CommitTransformsIfUsingLegacyConstructor();
	FSlateDrawElement& DrawElt = ElementList.AddUninitialized();
	DrawElt.Init(InLayer, PaintGeometry, InClippingRect, InDrawEffects);
	DrawElt.ElementType = ET_Gradient;
	DrawElt.DataPayload.SetGradientPayloadProperties( InGradientStops, InGradientType );
}


void FSlateDrawElement::MakeSpline( FSlateWindowElementList& ElementList, uint32 InLayer, const FPaintGeometry& PaintGeometry, const FVector2D& InStart, const FVector2D& InStartDir, const FVector2D& InEnd, const FVector2D& InEndDir, const FSlateRect InClippingRect, float InThickness, ESlateDrawEffect::Type InDrawEffects, const FLinearColor& InTint )
{
	SCOPE_CYCLE_COUNTER( STAT_SlateDrawElementMakeTime )
	PaintGeometry.CommitTransformsIfUsingLegacyConstructor();
	FSlateDrawElement& DrawElt = ElementList.AddUninitialized();
	DrawElt.Init(InLayer, PaintGeometry, InClippingRect, InDrawEffects);
	DrawElt.ElementType = ET_Spline;
	DrawElt.DataPayload.SetSplinePayloadProperties( InStart, InStartDir, InEnd, InEndDir, InThickness, InTint );
}


void FSlateDrawElement::MakeDrawSpaceSpline( FSlateWindowElementList& ElementList, uint32 InLayer, const FVector2D& InStart, const FVector2D& InStartDir, const FVector2D& InEnd, const FVector2D& InEndDir, const FSlateRect InClippingRect, float InThickness, ESlateDrawEffect::Type InDrawEffects, const FLinearColor& InTint )
{
	MakeSpline( ElementList, InLayer, FPaintGeometry(), InStart, InStartDir, InEnd, InEndDir, InClippingRect, InThickness, InDrawEffects, InTint );
}

void FSlateDrawElement::MakeDrawSpaceGradientSpline( FSlateWindowElementList& ElementList, uint32 InLayer, const FVector2D& InStart, const FVector2D& InStartDir, const FVector2D& InEnd, const FVector2D& InEndDir, const FSlateRect InClippingRect, const TArray<FSlateGradientStop>& InGradientStops, float InThickness, ESlateDrawEffect::Type InDrawEffects )
{
	SCOPE_CYCLE_COUNTER( STAT_SlateDrawElementMakeTime )
	const FPaintGeometry PaintGeometry;
	PaintGeometry.CommitTransformsIfUsingLegacyConstructor();
	FSlateDrawElement& DrawElt = ElementList.AddUninitialized();
	DrawElt.Init(InLayer, PaintGeometry, InClippingRect, InDrawEffects);
	DrawElt.ElementType = ET_Spline;
	DrawElt.DataPayload.SetGradientSplinePayloadProperties( InStart, InStartDir, InEnd, InEndDir, InThickness, InGradientStops );
}

void FSlateDrawElement::MakeLines(FSlateWindowElementList& ElementList, uint32 InLayer, const FPaintGeometry& PaintGeometry, const TArray<FVector2D>& Points, const FSlateRect InClippingRect, ESlateDrawEffect::Type InDrawEffects, const FLinearColor& InTint, bool bAntialias, float Thickness)
{
	SCOPE_CYCLE_COUNTER( STAT_SlateDrawElementMakeTime )
	PaintGeometry.CommitTransformsIfUsingLegacyConstructor();
	FSlateDrawElement& DrawElt = ElementList.AddUninitialized();
	DrawElt.Init(InLayer, PaintGeometry, InClippingRect, InDrawEffects);
	DrawElt.ElementType = ET_Line;
	DrawElt.DataPayload.SetLinesPayloadProperties( Points, InTint, bAntialias, ESlateLineJoinType::Sharp, Thickness );
}


void FSlateDrawElement::MakeViewport( FSlateWindowElementList& ElementList, uint32 InLayer, const FPaintGeometry& PaintGeometry, TSharedPtr<const ISlateViewport> Viewport, const FSlateRect& InClippingRect, ESlateDrawEffect::Type InDrawEffects, const FLinearColor& InTint )
{
	SCOPE_CYCLE_COUNTER( STAT_SlateDrawElementMakeTime )
	PaintGeometry.CommitTransformsIfUsingLegacyConstructor();
	FSlateDrawElement& DrawElt = ElementList.AddUninitialized();
	DrawElt.Init(InLayer, PaintGeometry, InClippingRect, InDrawEffects);
	DrawElt.ElementType = ET_Viewport;
	DrawElt.DataPayload.SetViewportPayloadProperties( Viewport, InTint );
}


void FSlateDrawElement::MakeCustom( FSlateWindowElementList& ElementList, uint32 InLayer, TSharedPtr<ICustomSlateElement, ESPMode::ThreadSafe> CustomDrawer )
{
	SCOPE_CYCLE_COUNTER( STAT_SlateDrawElementMakeTime )
	FSlateDrawElement& DrawElt = ElementList.AddUninitialized();
	DrawElt.Init(InLayer, FPaintGeometry(), FSlateRect(1,1,1,1), ESlateDrawEffect::None);
	DrawElt.RenderTransform = FSlateRenderTransform();
	DrawElt.ElementType = ET_Custom;
	DrawElt.DataPayload.SetCustomDrawerPayloadProperties( CustomDrawer );
}


void FSlateDrawElement::MakeCustomVerts(FSlateWindowElementList& ElementList, uint32 InLayer, const FSlateResourceHandle& InRenderResourceHandle, const TArray<FSlateVertex>& InVerts, const TArray<SlateIndex>& InIndexes, ISlateUpdatableInstanceBuffer* InInstanceData, uint32 InInstanceOffset, uint32 InNumInstances)
{
	SCOPE_CYCLE_COUNTER(STAT_SlateDrawElementMakeTime)

	FSlateDrawElement& DrawElt = ElementList.AddUninitialized();
	DrawElt.Init(InLayer, FPaintGeometry(),/*Clip rect is ignored*/ FSlateRect(1,1,1,1), ESlateDrawEffect::None);
	DrawElt.RenderTransform = FSlateRenderTransform();
	DrawElt.ElementType = ET_CustomVerts;

	FSlateShaderResourceProxy* RenderingProxy = InRenderResourceHandle.Data->Proxy;

	DrawElt.DataPayload.SetCustomVertsPayloadProperties(RenderingProxy, InVerts, InIndexes, InInstanceData, InInstanceOffset, InNumInstances);
}

void FSlateDrawElement::MakeCachedBuffer(FSlateWindowElementList& ElementList, uint32 InLayer, TSharedPtr<FSlateRenderDataHandle, ESPMode::ThreadSafe>& CachedRenderDataHandle, const FVector2D& Offset)
{
	SCOPE_CYCLE_COUNTER(STAT_SlateDrawElementMakeTime)
	FSlateDrawElement& DrawElt = ElementList.AddUninitialized();
	DrawElt.Init(InLayer, FPaintGeometry(), FSlateRect(1, 1, 1, 1), ESlateDrawEffect::None);
	DrawElt.RenderTransform = FSlateRenderTransform();
	DrawElt.ElementType = ET_CachedBuffer;
	DrawElt.DataPayload.SetCachedBufferPayloadProperties(CachedRenderDataHandle.Get(), Offset);

	ElementList.BeginUsingCachedBuffer(CachedRenderDataHandle);
}

void FSlateDrawElement::MakeLayer(FSlateWindowElementList& ElementList, uint32 InLayer, TSharedPtr<FSlateDrawLayerHandle, ESPMode::ThreadSafe>& DrawLayerHandle)
{
	SCOPE_CYCLE_COUNTER(STAT_SlateDrawElementMakeTime)
	FSlateDrawElement& DrawElt = ElementList.AddUninitialized();
	DrawElt.Init(InLayer, FPaintGeometry(), FSlateRect(1, 1, 1, 1), ESlateDrawEffect::None);
	DrawElt.RenderTransform = FSlateRenderTransform();
	DrawElt.ElementType = ET_Layer;
	DrawElt.DataPayload.SetLayerPayloadProperties(DrawLayerHandle.Get());
}

FVector2D FSlateDrawElement::GetRotationPoint(const FPaintGeometry& PaintGeometry, const TOptional<FVector2D>& UserRotationPoint, ERotationSpace RotationSpace)
{
	FVector2D RotationPoint(0, 0);

	const FVector2D& LocalSize = PaintGeometry.GetLocalSize();

	switch (RotationSpace)
	{
	case RelativeToElement:
	{
		// If the user did not specify a rotation point, we rotate about the center of the element
		RotationPoint = UserRotationPoint.Get(LocalSize * 0.5f);
	}
		break;
	case RelativeToWorld:
	{
		// its in world space, must convert the point to local space.
		RotationPoint = TransformPoint(Inverse(PaintGeometry.GetAccumulatedRenderTransform()), UserRotationPoint.Get(FVector2D::ZeroVector));
	}
		break;
	default:
		check(0);
		break;
	}

	return RotationPoint;
}

void FSlateBatchData::Reset()
{
	RenderBatches.Reset();
	DynamicOffset = FVector2D(0, 0);
	
	// note: LayerToElementBatches is not reset here as the same layers are 
	// more than likely reused and we can save memory allocations by not resetting the map every frame

	NumBatchedVertices = 0;
	NumBatchedIndices = 0;
	NumLayers = 0;

	RenderDataHandle.Reset();
}

#define MAX_VERT_ARRAY_RECYCLE (200)
#define MAX_INDEX_ARRAY_RECYCLE (500)

void FSlateBatchData::AssignVertexArrayToBatch( FSlateElementBatch& Batch )
{
	// Get a free vertex array
	if (VertexArrayFreeList.Num() > 0)
	{
		Batch.VertexArrayIndex = VertexArrayFreeList.Pop(/*bAllowShrinking=*/ false);
	}
	else
	{
		// There are no free vertex arrays so we must add one		
		uint32 NewIndex = BatchVertexArrays.Add(FSlateVertexArray());
		ResetVertexArray(BatchVertexArrays[NewIndex]);

		Batch.VertexArrayIndex = NewIndex;
	}
}

void FSlateBatchData::AssignIndexArrayToBatch( FSlateElementBatch& Batch )
{
	// Get a free index array
	if (IndexArrayFreeList.Num() > 0)
	{
		Batch.IndexArrayIndex = IndexArrayFreeList.Pop(/*bAllowShrinking=*/ false);
	}
	else
	{
		// There are no free index arrays so we must add one
		uint32 NewIndex = BatchIndexArrays.Add(FSlateIndexArray());
		ResetIndexArray(BatchIndexArrays[NewIndex]);

		Batch.IndexArrayIndex = NewIndex;
	}

}

void FSlateBatchData::FillVertexAndIndexBuffer(uint8* VertexBuffer, uint8* IndexBuffer)
{
	int32 IndexOffset = 0;
	int32 VertexOffset = 0;
	for ( const FSlateRenderBatch& Batch : RenderBatches )
	{
		// Ignore foreign batches that are inserted into our render set.
		if ( RenderDataHandle != Batch.CachedRenderHandle )
		{
			continue;
		}

		if ( Batch.VertexArrayIndex != INDEX_NONE && Batch.IndexArrayIndex != INDEX_NONE )
		{
			FSlateVertexArray& Vertices = BatchVertexArrays[Batch.VertexArrayIndex];
			FSlateIndexArray& Indices = BatchIndexArrays[Batch.IndexArrayIndex];

			if ( Vertices.Num() && Indices.Num() )
			{
				uint32 RequiredVertexSize = Vertices.Num() * Vertices.GetTypeSize();
				uint32 RequiredIndexSize = Indices.Num() * Indices.GetTypeSize();

				FMemory::Memcpy(VertexBuffer + VertexOffset, Vertices.GetData(), RequiredVertexSize);
				FMemory::Memcpy(IndexBuffer + IndexOffset, Indices.GetData(), RequiredIndexSize);

				IndexOffset += ( Indices.Num()*sizeof(SlateIndex) );
				VertexOffset += ( Vertices.Num()*sizeof(FSlateVertex) );

				Vertices.Reset();
				Indices.Reset();

				if ( Vertices.GetSlack() > MAX_VERT_ARRAY_RECYCLE )
				{
					ResetVertexArray(Vertices);
				}

				if ( Indices.GetSlack() > MAX_INDEX_ARRAY_RECYCLE )
				{
					ResetIndexArray(Indices);
				}
			}

			VertexArrayFreeList.Add(Batch.VertexArrayIndex);
			IndexArrayFreeList.Add(Batch.IndexArrayIndex);
		}
	}
}

void FSlateBatchData::CreateRenderBatches(FElementBatchMap& LayerToElementBatches)
{
	checkSlow(IsInRenderingThread());

	uint32 VertexOffset = 0;
	uint32 IndexOffset = 0;

	FPlatformMisc::BeginNamedEvent(FColor::Magenta, "SlateRT::CreateRenderBatches");

	Merge(LayerToElementBatches, VertexOffset, IndexOffset);

	FPlatformMisc::EndNamedEvent();

	// 
	if ( RenderDataHandle.IsValid() )
	{
		RenderDataHandle->SetRenderBatches(&RenderBatches);
	}
}

void FSlateBatchData::AddRenderBatch(uint32 InLayer, const FSlateElementBatch& InElementBatch, int32 InNumVertices, int32 InNumIndices, int32 InVertexOffset, int32 InIndexOffset)
{
	NumBatchedVertices += InNumVertices;
	NumBatchedIndices += InNumIndices;

	const int32 Index = RenderBatches.Add(FSlateRenderBatch(InLayer, InElementBatch, RenderDataHandle, InNumVertices, InNumIndices, InVertexOffset, InIndexOffset));
	RenderBatches[Index].DynamicOffset = DynamicOffset;
}

void FSlateBatchData::ResetVertexArray(FSlateVertexArray& InOutVertexArray)
{
	InOutVertexArray.Empty(MAX_VERT_ARRAY_RECYCLE);
}

void FSlateBatchData::ResetIndexArray(FSlateIndexArray& InOutIndexArray)
{
	InOutIndexArray.Empty(MAX_INDEX_ARRAY_RECYCLE);
}


void FSlateBatchData::Merge(FElementBatchMap& InLayerToElementBatches, uint32& VertexOffset, uint32& IndexOffset)
{
	InLayerToElementBatches.Sort();

	const bool bExpandLayersAndCachedHandles = RenderDataHandle.IsValid() == false;

	InLayerToElementBatches.ForEachLayer([&] (uint32 Layer, FElementBatchArray& ElementBatches)
	{
		++NumLayers;
		for ( FElementBatchArray::TIterator BatchIt(ElementBatches); BatchIt; ++BatchIt )
		{
			FSlateElementBatch& ElementBatch = *BatchIt;

			if ( ElementBatch.GetCustomDrawer().IsValid() )
			{
				AddRenderBatch(Layer, ElementBatch, 0, 0, 0, 0);
			}
			else
			{
				if ( bExpandLayersAndCachedHandles )
				{
					if ( FSlateRenderDataHandle* RenderHandle = ElementBatch.GetCachedRenderHandle().Get() )
					{
						DynamicOffset += ElementBatch.GetCachedRenderDataOffset();

						if ( TArray<FSlateRenderBatch>* ForeignBatches = RenderHandle->GetRenderBatches() )
						{
							TArray<FSlateRenderBatch>& ForeignBatchesRef = *ForeignBatches;
							for ( int32 i = 0; i < ForeignBatches->Num(); i++ )
							{
								TSharedPtr<FSlateDrawLayerHandle, ESPMode::ThreadSafe> LayerHandle = ForeignBatchesRef[i].LayerHandle.Pin();
								if ( LayerHandle.IsValid() )
								{
									// If a record was added for a layer, but nothing was ever drawn for it, the batch map will be null.
									if ( LayerHandle->BatchMap )
									{
										Merge(*LayerHandle->BatchMap, VertexOffset, IndexOffset);
										LayerHandle->BatchMap = nullptr;
									}
								}
								else
								{
									const int32 Index = RenderBatches.Add(ForeignBatchesRef[i]);
									RenderBatches[Index].DynamicOffset = DynamicOffset;
								}
							}
						}

						DynamicOffset -= ElementBatch.GetCachedRenderDataOffset();

						continue;
					}
				}
				else
				{
					// Insert if we're not expanding
					if ( FSlateDrawLayerHandle* LayerHandle = ElementBatch.GetLayerHandle().Get() )
					{
						AddRenderBatch(Layer, ElementBatch, 0, 0, 0, 0);
						continue;
					}
				}
					
				if ( ElementBatch.VertexArrayIndex != INDEX_NONE && ElementBatch.IndexArrayIndex != INDEX_NONE )
				{
					FSlateVertexArray& BatchVertices = GetBatchVertexList(ElementBatch);
					FSlateIndexArray& BatchIndices = GetBatchIndexList(ElementBatch);

					// We should have at least some vertices and indices in the batch or none at all
					check((BatchVertices.Num() > 0 && BatchIndices.Num() > 0) || (BatchVertices.Num() == 0 && BatchIndices.Num() == 0));

					if ( BatchVertices.Num() > 0 && BatchIndices.Num() > 0 )
					{
						const int32 NumVertices = BatchVertices.Num();
						const int32 NumIndices = BatchIndices.Num();

						AddRenderBatch(Layer, ElementBatch, NumVertices, NumIndices, VertexOffset, IndexOffset);

						VertexOffset += BatchVertices.Num();
						IndexOffset += BatchIndices.Num();
					}
					else
					{
						VertexArrayFreeList.Add(ElementBatch.VertexArrayIndex);
						IndexArrayFreeList.Add(ElementBatch.IndexArrayIndex);
					}
				}
			}
		}

		ElementBatches.Reset();
	});

	InLayerToElementBatches.Reset();
}


FSlateWindowElementList::FDeferredPaint::FDeferredPaint( const TSharedRef<const SWidget>& InWidgetToPaint, const FPaintArgs& InArgs, const FGeometry InAllottedGeometry, const FSlateRect InMyClippingRect, const FWidgetStyle& InWidgetStyle, bool InParentEnabled )
	: WidgetToPaintPtr( InWidgetToPaint )
	, Args( InArgs )
	, AllottedGeometry( InAllottedGeometry )
	, MyClippingRect( InMyClippingRect )
	, WidgetStyle( InWidgetStyle )
	, bParentEnabled( InParentEnabled )
{
}

FSlateWindowElementList::FDeferredPaint::FDeferredPaint(const FDeferredPaint& Copy, const FPaintArgs& InArgs)
	: WidgetToPaintPtr(Copy.WidgetToPaintPtr)
	, Args(InArgs)
	, AllottedGeometry(Copy.AllottedGeometry)
	, MyClippingRect(Copy.MyClippingRect)
	, WidgetStyle(Copy.WidgetStyle)
	, bParentEnabled(Copy.bParentEnabled)
{
}


int32 FSlateWindowElementList::FDeferredPaint::ExecutePaint( int32 LayerId, FSlateWindowElementList& OutDrawElements ) const
{
	TSharedPtr<const SWidget> WidgetToPaint = WidgetToPaintPtr.Pin();
	if ( WidgetToPaint.IsValid() )
	{
		return WidgetToPaint->Paint( Args, AllottedGeometry, OutDrawElements.GetWindow()->GetClippingRectangleInWindow(), OutDrawElements, LayerId, WidgetStyle, bParentEnabled );
	}

	return LayerId;
}

FSlateWindowElementList::FDeferredPaint FSlateWindowElementList::FDeferredPaint::Copy(const FPaintArgs& InArgs)
{
	return FDeferredPaint(*this, InArgs);
}


void FSlateWindowElementList::QueueDeferredPainting( const FDeferredPaint& InDeferredPaint )
{
	DeferredPaintList.Add(MakeShareable(new FDeferredPaint(InDeferredPaint)));
}

int32 FSlateWindowElementList::PaintDeferred(int32 LayerId)
{
	bNeedsDeferredResolve = false;

	int32 ResolveIndex = ResolveToDeferredIndex.Pop(false);

	for ( int32 i = ResolveIndex; i < DeferredPaintList.Num(); ++i )
	{
		LayerId = DeferredPaintList[i]->ExecutePaint(LayerId, *this);
	}

	for ( int32 i = DeferredPaintList.Num() - 1; i >= ResolveIndex; --i )
	{
		DeferredPaintList.RemoveAt(i, 1, false);
	}

	return LayerId;
}

void FSlateWindowElementList::BeginDeferredGroup()
{
	ResolveToDeferredIndex.Add(DeferredPaintList.Num());
}

void FSlateWindowElementList::EndDeferredGroup()
{
	bNeedsDeferredResolve = true;
}



FSlateWindowElementList::FVolatilePaint::FVolatilePaint(const TSharedRef<const SWidget>& InWidgetToPaint, const FPaintArgs& InArgs, const FGeometry InAllottedGeometry, const FSlateRect InMyClippingRect, int32 InLayerId, const FWidgetStyle& InWidgetStyle, bool InParentEnabled)
	: WidgetToPaintPtr(InWidgetToPaint)
	, Args(InArgs.EnableCaching(InArgs.GetLayoutCache(), InArgs.GetParentCacheNode(), false, true))
	, AllottedGeometry(InAllottedGeometry)
	, MyClippingRect(InMyClippingRect)
	, LayerId(InLayerId)
	, WidgetStyle(InWidgetStyle)
	, bParentEnabled(InParentEnabled)
{
}

int32 FSlateWindowElementList::FVolatilePaint::ExecutePaint(FSlateWindowElementList& OutDrawElements) const
{
	static const FName InvalidationPanelName(TEXT("SInvalidationPanel"));

	TSharedPtr<const SWidget> WidgetToPaint = WidgetToPaintPtr.Pin();
	if ( WidgetToPaint.IsValid() )
	{
		//FPlatformMisc::BeginNamedEvent(FColor::Red, *FReflectionMetaData::GetWidgetDebugInfo(WidgetToPaint));

		// Have to run a slate pre-pass for all volatile elements, some widgets cache information like 
		// the STextBlock.  This may be all kinds of terrible an idea to do during paint.
		SWidget* MutableWidget = const_cast<SWidget*>( WidgetToPaint.Get() );

		if ( MutableWidget->GetType() != InvalidationPanelName )
		{
			MutableWidget->SlatePrepass(AllottedGeometry.Scale);
		}

		const int32 NewLayer = WidgetToPaint->Paint(Args, AllottedGeometry, MyClippingRect, OutDrawElements, LayerId, WidgetStyle, bParentEnabled);
		
		//FPlatformMisc::EndNamedEvent();

		return NewLayer;
	}

	return LayerId;
}

void FSlateWindowElementList::QueueVolatilePainting(const FVolatilePaint& InVolatilePaint)
{
	TSharedPtr< FSlateDrawLayerHandle, ESPMode::ThreadSafe > LayerHandle = MakeShareable(new FSlateDrawLayerHandle());

	FSlateDrawElement::MakeLayer(*this, InVolatilePaint.GetLayerId(), LayerHandle);

	const int32 NewEntryIndex = VolatilePaintList.Add(MakeShareable(new FVolatilePaint(InVolatilePaint)));
	VolatilePaintList[NewEntryIndex]->LayerHandle = LayerHandle;
}

int32 FSlateWindowElementList::PaintVolatile(FSlateWindowElementList& OutElementList)
{
	int32 MaxLayerId = 0;

	for ( int32 VolatileIndex = 0; VolatileIndex < VolatilePaintList.Num(); ++VolatileIndex )
	{
		const TSharedPtr<FVolatilePaint>& Args = VolatilePaintList[VolatileIndex];

		OutElementList.BeginLogicalLayer(Args->LayerHandle);
		MaxLayerId = FMath::Max(MaxLayerId, Args->ExecutePaint(OutElementList));
		OutElementList.EndLogicalLayer();
	}

	return MaxLayerId;
}

void FSlateWindowElementList::BeginLogicalLayer(const TSharedPtr<FSlateDrawLayerHandle, ESPMode::ThreadSafe>& LayerHandle)
{
	// Don't attempt to begin logical layers inside a cached view of the data.
	checkSlow(!IsCachedRenderDataInUse());

	//FPlatformMisc::BeginNamedEvent(FColor::Orange, "FindLayer");
	TSharedPtr<FSlateDrawLayer> Layer = DrawLayers.FindRef(LayerHandle);
	//FPlatformMisc::EndNamedEvent();

	if ( !Layer.IsValid() )
	{
		if ( DrawLayerPool.Num() > 0 )
		{
			Layer = DrawLayerPool.Pop(false);
		}
		else
		{
			Layer = MakeShareable(new FSlateDrawLayer());
		}

		//FPlatformMisc::BeginNamedEvent(FColor::Orange, "AddLayer");
		DrawLayers.Add(LayerHandle, Layer);
		//FPlatformMisc::EndNamedEvent();
	}

	//FPlatformMisc::BeginNamedEvent(FColor::Orange, "PushLayer");
	DrawStack.Push(Layer.Get());
	//FPlatformMisc::EndNamedEvent();
}

void FSlateWindowElementList::EndLogicalLayer()
{
	DrawStack.Pop();
}

FSlateRenderDataHandle::FSlateRenderDataHandle(const ILayoutCache* InCacher, ISlateRenderDataManager* InManager)
	: Cacher(InCacher)
	, Manager(InManager)
	, RenderBatches(nullptr)
	, UsageCount(0)
{
}

FSlateRenderDataHandle::~FSlateRenderDataHandle()
{
	if ( Manager )
	{
		Manager->BeginReleasingRenderData(this);
	}
}

void FSlateRenderDataHandle::Disconnect()
{
	Manager = nullptr;
	RenderBatches = nullptr;
}

TSharedRef<FSlateRenderDataHandle, ESPMode::ThreadSafe> FSlateWindowElementList::CacheRenderData(const ILayoutCache* Cacher)
{
	// Don't attempt to use this slate window element list if the cache is still being used.
	checkSlow(!IsCachedRenderDataInUse());

	TSharedPtr<FSlateRenderer> Renderer = FSlateApplicationBase::Get().GetRenderer();

	TSharedRef<FSlateRenderDataHandle, ESPMode::ThreadSafe> CachedRenderDataHandleRef = Renderer->CacheElementRenderData(Cacher, *this);
	CachedRenderDataHandle = CachedRenderDataHandleRef;

	return CachedRenderDataHandleRef;
}

void FSlateWindowElementList::PreDraw_ParallelThread()
{
	check(IsInParallelRenderingThread());

	for ( auto& Entry : DrawLayers )
	{
		checkSlow(Entry.Key->BatchMap == nullptr);
		Entry.Key->BatchMap = &Entry.Value->GetElementBatchMap();
	}
}

void FSlateWindowElementList::PostDraw_ParallelThread()
{
	check(IsInParallelRenderingThread());

	for ( auto& Entry : DrawLayers )
	{
		Entry.Key->BatchMap = nullptr;
	}

	for ( TSharedPtr<FSlateRenderDataHandle, ESPMode::ThreadSafe>& Handle : CachedRenderHandlesInUse )
	{
		Handle->EndUsing();
	}

	CachedRenderHandlesInUse.Reset();
}

DECLARE_MEMORY_STAT(TEXT("FSlateWindowElementList MemManager"), STAT_FSlateWindowElementListMemManager, STATGROUP_SlateVerbose);
DECLARE_DWORD_COUNTER_STAT(TEXT("FSlateWindowElementList MemManager Count"), STAT_FSlateWindowElementListMemManagerCount, STATGROUP_SlateVerbose);

void FSlateWindowElementList::ResetBuffers()
{
	// Don't attempt to use this slate window element list if the cache is still being used.
	checkSlow(!IsCachedRenderDataInUse());

	DeferredPaintList.Reset();
	VolatilePaintList.Reset();
	BatchData.Reset();

#if SLATE_POOL_DRAW_ELEMENTS
	DrawElementFreePool.Append(RootDrawLayer.DrawElements);
#endif

	// Reset the draw elements on the root draw layer
	RootDrawLayer.DrawElements.Reset();

	// Return child draw layers to the pool, and reset their draw elements.
	for ( auto& Entry : DrawLayers )
	{
#if SLATE_POOL_DRAW_ELEMENTS
		TArray<FSlateDrawElement*>& DrawElements = Entry.Value->DrawElements;
		DrawElementFreePool.Append(DrawElements);
#else
		TArray<FSlateDrawElement>& DrawElements = Entry.Value->DrawElements;
#endif

		DrawElements.Reset();
		DrawLayerPool.Add(Entry.Value);
	}

	DrawLayers.Reset();

	DrawStack.Reset();
	DrawStack.Push(&RootDrawLayer);

	INC_DWORD_STAT(STAT_FSlateWindowElementListMemManagerCount);
	INC_MEMORY_STAT_BY(STAT_FSlateWindowElementListMemManager, MemManager.GetByteCount());

	MemManager.Flush();
}
