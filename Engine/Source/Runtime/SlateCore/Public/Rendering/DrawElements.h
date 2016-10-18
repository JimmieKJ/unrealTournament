// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UniqueObj.h"
#include "RenderingCommon.h"

class SWindow;
class FSlateViewportInterface;

DECLARE_MEMORY_STAT_EXTERN(TEXT("Vertex/Index Buffer Pool Memory (CPU)"), STAT_SlateBufferPoolMemory, STATGROUP_SlateMemory, SLATECORE_API );


struct FSlateGradientStop
{
	FVector2D Position;
	FLinearColor Color;

	FSlateGradientStop( const FVector2D& InPosition, const FLinearColor& InColor )
		: Position(InPosition)
		, Color(InColor)
	{

	}
};

template <> struct TIsPODType<FSlateGradientStop> { enum { Value = true }; };


class FSlateDrawLayerHandle;


class FSlateDataPayload
{
public:
	// Element tint
	FLinearColor Tint;

	// Spline Data
	FVector2D StartPt;
	FVector2D StartDir;
	FVector2D EndPt;
	FVector2D EndDir;

	// Brush data
	const FSlateBrush* BrushResource;
	const FSlateShaderResourceProxy* ResourceProxy;

	// Box Data
	FVector2D RotationPoint;
	float Angle;

	// Spline/Line Data
	float Thickness;

	// Font data
	FSlateFontInfo FontInfo;
	TCHAR* ImmutableText;

	// Shaped text data
	FShapedGlyphSequencePtr ShapedGlyphSequence;

	// Gradient data (fixme, this should be allocated with FSlateWindowElementList::Alloc)
	TArray<FSlateGradientStop> GradientStops;
	EOrientation GradientType;

	// Line data (fixme, this should be allocated with FSlateWindowElementList::Alloc)
	TArray<FVector2D> Points;

	// Viewport data
	FSlateShaderResource* ViewportRenderTargetTexture;
	bool bAllowViewportScaling;
	bool bViewportTextureAlphaOnly;
	bool bRequiresVSync;

	// Misc data
	ESlateBatchDrawFlag::Type BatchFlags;

	// Custom drawer data
	TWeakPtr<ICustomSlateElement, ESPMode::ThreadSafe> CustomDrawer;

	// Custom verts data
	TArray<FSlateVertex> CustomVertsData;
	TArray<SlateIndex> CustomVertsIndexData;

	// Instancing support
	ISlateUpdatableInstanceBuffer* InstanceData;
	uint32 InstanceOffset;
	uint32 NumInstances;

	// Cached render data
	class FSlateRenderDataHandle* CachedRenderData;
	FVector2D CachedRenderDataOffset;

	// Layer handle
	FSlateDrawLayerHandle* LayerHandle;

	// Line data
	ESlateLineJoinType::Type SegmentJoinType;
	bool bAntialias;

	SLATECORE_API static FSlateShaderResourceManager* ResourceManager;

	FSlateDataPayload()
		: Tint(FLinearColor::White)
		, BrushResource(nullptr)
		, ResourceProxy(nullptr)
		, RotationPoint(FVector2D::ZeroVector)
		, ImmutableText(nullptr)
		, ViewportRenderTargetTexture(nullptr)
		, bViewportTextureAlphaOnly(false)
		, bRequiresVSync(false)
		, BatchFlags(ESlateBatchDrawFlag::None)
		, CustomDrawer()
		, InstanceData(nullptr)
		, InstanceOffset(0)
		, NumInstances(0)
		//, CachedRenderDataOffset(FVector2D::ZeroVector)
		//, LayerHandle(nullptr)
		//, SegmentJoinType(ESlateLineJoinType::Sharp)
		//, bAntialias(true)
	{ }

	void SetBoxPayloadProperties( const FSlateBrush* InBrush, const FLinearColor& InTint, FSlateShaderResourceProxy* InResourceProxy = nullptr, ISlateUpdatableInstanceBuffer* InInstanceData = nullptr )
	{
		Tint = InTint;

		InstanceData = InInstanceData;

		BrushResource = InBrush;
		if( InResourceProxy )
		{
			ResourceProxy = InResourceProxy;
		}
		else
		{
			ResourceProxy = ResourceManager->GetShaderResource(*InBrush);
		}
		Angle = 0.0f;
	}

	void SetRotatedBoxPayloadProperties( const FSlateBrush* InBrush, float InAngle, const FVector2D& LocalRotationPoint, const FLinearColor& InTint, FSlateShaderResourceProxy* InResourceProxy = nullptr )
	{
		SetBoxPayloadProperties( InBrush, InTint, InResourceProxy );
		RotationPoint = LocalRotationPoint;
		RotationPoint.DiagnosticCheckNaN();
		Angle = InAngle;
	}

	void SetTextPayloadProperties( FSlateWindowElementList& DrawBuffer, const FString& InText, const FSlateFontInfo& InFontInfo, const FLinearColor& InTint, const int32 StartIndex = 0, const int32 EndIndex = MAX_int32 );

	void SetShapedTextPayloadProperties( const FShapedGlyphSequenceRef& InShapedGlyphSequence, const FLinearColor& InTint )
	{
		Tint = InTint;
		ShapedGlyphSequence = InShapedGlyphSequence;
	}

	void SetGradientPayloadProperties( const TArray<FSlateGradientStop>& InGradientStops, EOrientation InGradientType )
	{
		GradientStops = InGradientStops;
		GradientType = InGradientType;
	}

	void SetSplinePayloadProperties( const FVector2D& InStart, const FVector2D& InStartDir, const FVector2D& InEnd, const FVector2D& InEndDir, float InThickness, const FLinearColor& InTint )
	{
		Tint = InTint;
		StartPt = InStart;
		StartDir = InStartDir;
		EndPt = InEnd;
		EndDir = InEndDir;
		BrushResource = nullptr;
		Thickness = InThickness;
	}

	void SetGradientSplinePayloadProperties( const FVector2D& InStart, const FVector2D& InStartDir, const FVector2D& InEnd, const FVector2D& InEndDir, float InThickness, const TArray<FSlateGradientStop>& InGradientStops )
	{
		StartPt = InStart;
		StartDir = InStartDir;
		EndPt = InEnd;
		EndDir = InEndDir;
		BrushResource = nullptr;
		Thickness = InThickness;
		GradientStops = InGradientStops;
	}

	void SetLinesPayloadProperties( const TArray<FVector2D>& InPoints, const FLinearColor& InTint, bool bInAntialias, ESlateLineJoinType::Type InJoinType, float InThickness )
	{
		Tint = InTint;
		Thickness = InThickness;
		Points = InPoints;
		SegmentJoinType = InJoinType;
		bAntialias = bInAntialias;
	}

	void SetViewportPayloadProperties( const TSharedPtr<const ISlateViewport>& InViewport, const FLinearColor& InTint )
	{
		Tint = InTint;
		ViewportRenderTargetTexture = InViewport->GetViewportRenderTargetTexture();
		bAllowViewportScaling = InViewport->AllowScaling();
		bViewportTextureAlphaOnly = InViewport->IsViewportTextureAlphaOnly();
		bRequiresVSync = InViewport->RequiresVsync();
	}

	void SetCustomDrawerPayloadProperties( const TSharedPtr<ICustomSlateElement, ESPMode::ThreadSafe>& InCustomDrawer )
	{
		CustomDrawer = InCustomDrawer;
	}

	void SetCustomVertsPayloadProperties(const FSlateShaderResourceProxy* InRenderProxy, const TArray<FSlateVertex>& InVerts, const TArray<SlateIndex>& InIndexes, ISlateUpdatableInstanceBuffer* InInstanceData, uint32 InInstanceOffset, uint32 InNumInstances)
	{
		ResourceProxy = InRenderProxy;
		CustomVertsData = InVerts;
		CustomVertsIndexData = InIndexes;
		InstanceData = InInstanceData;
		InstanceOffset = InInstanceOffset;
		NumInstances = InNumInstances;
	}

	void SetCachedBufferPayloadProperties(FSlateRenderDataHandle* InRenderDataHandle, const FVector2D& Offset)
	{
		CachedRenderData = InRenderDataHandle;
		CachedRenderDataOffset = Offset;
		checkSlow(CachedRenderData);
	}

	void SetLayerPayloadProperties(FSlateDrawLayerHandle* InLayerHandle)
	{
		LayerHandle = InLayerHandle;
		checkSlow(LayerHandle);
	}
};

/**
 * FSlateDrawElement is the building block for Slate's rendering interface.
 * Slate describes its visual output as an ordered list of FSlateDrawElement s
 */
class FSlateDrawElement
{
public:
	enum EElementType
	{
		ET_Box,
		ET_DebugQuad,
		ET_Text,
		ET_ShapedText,
		ET_Spline,
		ET_Line,
		ET_Gradient,
		ET_Viewport,
		ET_Border,
		ET_Custom,
		ET_CustomVerts,
		ET_CachedBuffer,
		ET_Layer,
		ET_Count,
	};

	enum ERotationSpace
	{
		/** Relative to the element.  (0,0) is the upper left corner of the element */
		RelativeToElement,
		/** Relative to the alloted paint geometry.  (0,0) is the upper left corner of the paint geometry */
		RelativeToWorld,
	};

	/**
	 * Creates a wireframe quad for debug purposes
	 *
	 * @param ElementList			The list in which to add elements
	 * @param InLayer				The layer to draw the element on
	 * @param PaintGeometry         DrawSpace position and dimensions; see FPaintGeometry
	 * @param InClippingRect		Parts of the element are clipped if it falls outside of this rectangle
	 */
	SLATECORE_API static void MakeDebugQuad( FSlateWindowElementList& ElementList, uint32 InLayer, const FPaintGeometry& PaintGeometry, const FSlateRect& InClippingRect);

	/**
	 * Creates a box element based on the following diagram.  Allows for this element to be resized while maintain the border of the image
	 * If there are no margins the resulting box is simply a quad
	 *     ___LeftMargin    ___RightMargin
	 *    /                /
	 *  +--+-------------+--+
	 *  |  |c1           |c2| ___TopMargin
	 *  +--o-------------o--+
	 *  |  |             |  |
	 *  |  |c3           |c4|
	 *  +--o-------------o--+
	 *  |  |             |  | ___BottomMargin
	 *  +--+-------------+--+
	 *
	 * @param ElementList			The list in which to add elements
	 * @param InLayer               The layer to draw the element on
	 * @param PaintGeometry         DrawSpace position and dimensions; see FPaintGeometry
	 * @param InBrush               Brush to apply to this element
	 * @param InClippingRect        Parts of the element are clipped if it falls outside of this rectangle
	 * @param InDrawEffects         Optional draw effects to apply
	 * @param InTint                Color to tint the element
	 */
	SLATECORE_API static void MakeBox( 
		FSlateWindowElementList& ElementList,
		uint32 InLayer, 
		const FPaintGeometry& PaintGeometry, 
		const FSlateBrush* InBrush, 
		const FSlateRect& InClippingRect, 
		ESlateDrawEffect::Type InDrawEffects = ESlateDrawEffect::None, 
		const FLinearColor& InTint = FLinearColor::White );

	SLATECORE_API static void MakeBox(
		FSlateWindowElementList& ElementList,
		uint32 InLayer, 
		const FPaintGeometry& PaintGeometry, 
		const FSlateBrush* InBrush, 
		const FSlateResourceHandle& InRenderingHandle, 
		const FSlateRect& InClippingRect, 
		ESlateDrawEffect::Type InDrawEffects = ESlateDrawEffect::None, 
		const FLinearColor& InTint = FLinearColor::White );
	
	// !!! DEPRECATED !!! Use a render transform om your widget instead.
	SLATECORE_API static void MakeRotatedBox(
		FSlateWindowElementList& ElementList,
		uint32 InLayer, 
		const FPaintGeometry& PaintGeometry, 
		const FSlateBrush* InBrush, 
		const FSlateRect& InClippingRect, 
		ESlateDrawEffect::Type InDrawEffects = ESlateDrawEffect::None, 
		float Angle = 0.0f,
		TOptional<FVector2D> InRotationPoint = TOptional<FVector2D>(),
		ERotationSpace RotationSpace = RelativeToElement,
		const FLinearColor& InTint = FLinearColor::White );
	/**
	 * Creates a text element which displays a string of a rendered in a certain font on the screen
	 *
	 * @param ElementList			The list in which to add elements
	 * @param InLayer               The layer to draw the element on
	 * @param PaintGeometry         DrawSpace position and dimensions; see FPaintGeometry
	 * @param InText                The string to draw
	 * @param StartIndex            Inclusive index to start rendering from on the specified text
	 * @param EndIndex				Exclusive index to stop rendering on the specified text
	 * @param InFontInfo            The font to draw the string with
	 * @param InClippingRect        Parts of the element are clipped if it falls outside of this rectangle
	 * @param InDrawEffects         Optional draw effects to apply
	 * @param InTint                Color to tint the element
	 */
	SLATECORE_API static void MakeText( FSlateWindowElementList& ElementList, uint32 InLayer, const FPaintGeometry& PaintGeometry, const FString& InText, const int32 StartIndex, const int32 EndIndex, const FSlateFontInfo& InFontInfo, const FSlateRect& InClippingRect,ESlateDrawEffect::Type InDrawEffects = ESlateDrawEffect::None, const FLinearColor& InTint =FLinearColor::White );
	
	SLATECORE_API static void MakeText( FSlateWindowElementList& ElementList, uint32 InLayer, const FPaintGeometry& PaintGeometry, const FString& InText, const FSlateFontInfo& InFontInfo, const FSlateRect& InClippingRect,ESlateDrawEffect::Type InDrawEffects = ESlateDrawEffect::None, const FLinearColor& InTint =FLinearColor::White );

	SLATECORE_API static void MakeText( FSlateWindowElementList& ElementList, uint32 InLayer, const FPaintGeometry& PaintGeometry, const FText& InText, const FSlateFontInfo& InFontInfo, const FSlateRect& InClippingRect,ESlateDrawEffect::Type InDrawEffects = ESlateDrawEffect::None, const FLinearColor& InTint =FLinearColor::White );

	/**
	 * Creates a text element which displays a series of shaped glyphs on the screen
	 *
	 * @param ElementList			The list in which to add elements
	 * @param InLayer               The layer to draw the element on
	 * @param PaintGeometry         DrawSpace position and dimensions; see FPaintGeometry
	 * @param InShapedGlyphSequence The shaped glyph sequence to draw
	 * @param InClippingRect        Parts of the element are clipped if it falls outside of this rectangle
	 * @param InDrawEffects         Optional draw effects to apply
	 * @param InTint                Color to tint the element
	 */
	SLATECORE_API static void MakeShapedText( FSlateWindowElementList& ElementList, uint32 InLayer, const FPaintGeometry& PaintGeometry, const FShapedGlyphSequenceRef& InShapedGlyphSequence, const FSlateRect& InClippingRect, ESlateDrawEffect::Type InDrawEffects = ESlateDrawEffect::None, const FLinearColor& InTint = FLinearColor::White );

	/**
	 * Creates a gradient element
	 *
	 * @param ElementList			   The list in which to add elements
	 * @param InLayer                  The layer to draw the element on
	 * @param PaintGeometry            DrawSpace position and dimensions; see FPaintGeometry
	 * @param InGradientStops          List of gradient stops which define the element
	 * @param InGradientType           The type of gradient (I.E Horizontal, vertical)
	 * @param InClippingRect           Parts of the element are clipped if it falls outside of this rectangle
	 * @param InDrawEffects            Optional draw effects to apply
	 */
	SLATECORE_API static void MakeGradient( FSlateWindowElementList& ElementList, uint32 InLayer, const FPaintGeometry& PaintGeometry, TArray<FSlateGradientStop> InGradientStops, EOrientation InGradientType, const FSlateRect& InClippingRect, ESlateDrawEffect::Type InDrawEffects = ESlateDrawEffect::None );

	/**
	 * Creates a spline element
	 *
	 * @param ElementList			The list in which to add elements
	 * @param InLayer               The layer to draw the element on
	 * @param PaintGeometry         DrawSpace position and dimensions; see FPaintGeometry
	 * @param InStart               The start point of the spline (local space)
	 * @param InStartDir            The direction of the spline from the start point
	 * @param InEnd                 The end point of the spline (local space)
	 * @param InEndDir              The direction of the spline to the end point
	 * @param InClippingRect        Parts of the element are clipped if it falls outside of this rectangle
	 * @param InDrawEffects         Optional draw effects to apply
	 * @param InTint                Color to tint the element
	 */
	SLATECORE_API static void MakeSpline( FSlateWindowElementList& ElementList, uint32 InLayer, const FPaintGeometry& PaintGeometry, const FVector2D& InStart, const FVector2D& InStartDir, const FVector2D& InEnd, const FVector2D& InEndDir, const FSlateRect InClippingRect, float InThickness = 0.0f, ESlateDrawEffect::Type InDrawEffects = ESlateDrawEffect::None, const FLinearColor& InTint=FLinearColor::White );

	/** Just like MakeSpline but in draw-space coordinates. This is useful for connecting already-transformed widgets together. */
	SLATECORE_API static void MakeDrawSpaceSpline(FSlateWindowElementList& ElementList, uint32 InLayer, const FVector2D& InStart, const FVector2D& InStartDir, const FVector2D& InEnd, const FVector2D& InEndDir, const FSlateRect InClippingRect, float InThickness = 0.0f, ESlateDrawEffect::Type InDrawEffects = ESlateDrawEffect::None, const FLinearColor& InTint=FLinearColor::White);

	/** Just like MakeSpline but in draw-space coordinates. This is useful for connecting already-transformed widgets together. */
	SLATECORE_API static void MakeDrawSpaceGradientSpline( FSlateWindowElementList& ElementList, uint32 InLayer, const FVector2D& InStart, const FVector2D& InStartDir, const FVector2D& InEnd, const FVector2D& InEndDir, const FSlateRect InClippingRect, const TArray<FSlateGradientStop>& InGradientStops, float InThickness = 0.0f, ESlateDrawEffect::Type InDrawEffects = ESlateDrawEffect::None );

	/**
	 * Creates a line defined by the provided points
	 *
	 * @param ElementList			   The list in which to add elements
	 * @param InLayer                  The layer to draw the element on
	 * @param PaintGeometry            DrawSpace position and dimensions; see FPaintGeometry
	 * @param Points                   Points that make up the lines.  The points are joined together. I.E if Points has A,B,C there the line is A-B-C.  To draw non-joining line segments call MakeLines multiple times
	 * @param InClippingRect           Parts of the element are clipped if it falls outside of this rectangle
	 * @param InDrawEffects            Optional draw effects to apply
	 * @param InTint                   Color to tint the element
	 * @param bAntialias               Should antialiasing be applied to the line?
	 * @param Thickness                The thickness of the line
	 */
	SLATECORE_API static void MakeLines( FSlateWindowElementList& ElementList, uint32 InLayer, const FPaintGeometry& PaintGeometry, const TArray<FVector2D>& Points, const FSlateRect InClippingRect, ESlateDrawEffect::Type InDrawEffects = ESlateDrawEffect::None, const FLinearColor& InTint=FLinearColor::White, bool bAntialias = true, float Thickness = 1.0f );

	/**
	 * Creates a viewport element which is useful for rendering custom data in a texture into Slate
	 *
	 * @param ElementList		   The list in which to add elements
	 * @param InLayer                  The layer to draw the element on
	 * @param PaintGeometry            DrawSpace position and dimensions; see FPaintGeometry
	 * @param Viewport                 Interface for drawing the viewport
	 * @param InClippingRect           Parts of the element are clipped if it falls outside of this rectangle
	 * @param InScale                  Draw scale to apply to the entire element
	 * @param InDrawEffects            Optional draw effects to apply
	 * @param InTint                   Color to tint the element
	 */
	SLATECORE_API static void MakeViewport( FSlateWindowElementList& ElementList, uint32 InLayer, const FPaintGeometry& PaintGeometry, TSharedPtr<const ISlateViewport> Viewport, const FSlateRect& InClippingRect, ESlateDrawEffect::Type InDrawEffects = ESlateDrawEffect::None, const FLinearColor& InTint=FLinearColor::White );

	/**
	 * Creates a custom element which can be used to manually draw into the Slate render target with graphics API calls rather than Slate elements
	 *
	 * @param ElementList		   The list in which to add elements
	 * @param InLayer                  The layer to draw the element on
	 * @param PaintGeometry            DrawSpace position and dimensions; see FPaintGeometry
	 * @param CustomDrawer		   Interface to a drawer which will be called when Slate renders this element
	 */
	SLATECORE_API static void MakeCustom( FSlateWindowElementList& ElementList, uint32 InLayer, TSharedPtr<ICustomSlateElement, ESPMode::ThreadSafe> CustomDrawer );
	
	SLATECORE_API static void MakeCustomVerts(FSlateWindowElementList& ElementList, uint32 InLayer, const FSlateResourceHandle& InRenderResourceHandle, const TArray<FSlateVertex>& InVerts, const TArray<SlateIndex>& InIndexes, ISlateUpdatableInstanceBuffer* InInstanceData, uint32 InInstanceOffset, uint32 InNumInstances);

	SLATECORE_API static void MakeCachedBuffer(FSlateWindowElementList& ElementList, uint32 InLayer, TSharedPtr<FSlateRenderDataHandle, ESPMode::ThreadSafe>& CachedRenderDataHandle, const FVector2D& Offset);

	SLATECORE_API static void MakeLayer(FSlateWindowElementList& ElementList, uint32 InLayer, TSharedPtr<FSlateDrawLayerHandle, ESPMode::ThreadSafe>& DrawLayerHandle);


	FORCEINLINE EElementType GetElementType() const { return ElementType; }
	FORCEINLINE uint32 GetLayer() const { return Layer; }
	FORCEINLINE const FSlateRenderTransform& GetRenderTransform() const { return RenderTransform; }
	FORCEINLINE const FVector2D& GetPosition() const { return Position; }
	FORCEINLINE void SetPosition(const FVector2D& InPosition) { Position = InPosition; }
	FORCEINLINE const FVector2D& GetLocalSize() const { return LocalSize; }
	FORCEINLINE float GetScale() const { return Scale; }
	FORCEINLINE const FSlateRect& GetClippingRect() const { return ClippingRect; }
	FORCEINLINE void SetClippingRect(const FSlateRect& InClippingRect) { ClippingRect = InClippingRect; }
	FORCEINLINE const FSlateDataPayload& GetDataPayload() const { return DataPayload; }
	FORCEINLINE uint32 GetDrawEffects() const { return DrawEffects; }
	FORCEINLINE const TOptional<FShortRect>& GetScissorRect() const { return ScissorRect; }

private:
	void Init(uint32 InLayer, const FPaintGeometry& PaintGeometry, const FSlateRect& InClippingRect, ESlateDrawEffect::Type InDrawEffects);


	static FVector2D GetRotationPoint( const FPaintGeometry& PaintGeometry, const TOptional<FVector2D>& UserRotationPoint, ERotationSpace RotationSpace );

private:
	FSlateDataPayload DataPayload;
	FSlateRenderTransform RenderTransform;
	FSlateRect ClippingRect;
	FVector2D Position;
	FVector2D LocalSize;
	float Scale;
	uint32 Layer;
	uint32 DrawEffects;
	EElementType ElementType;
	TOptional<FShortRect> ScissorRect;
};


/**
 * Shader parameters for slate
 */
struct FShaderParams
{
	/** Pixel shader parameters */
	FVector4 PixelParams;

	FShaderParams()
		: PixelParams( 0,0,0,0 )
	{}

	FShaderParams( const FVector4& InPixelParams )
		: PixelParams( InPixelParams )
	{}

	bool operator==( const FShaderParams& Other ) const
	{
		return PixelParams == Other.PixelParams;
	}

	static FShaderParams MakePixelShaderParams( const FVector4& PixelShaderParams )
	{
		return FShaderParams( PixelShaderParams );
	}
};

class FSlateRenderer;
class FSlateRenderBatch;

class ISlateRenderDataManager
{
public:
	virtual void BeginReleasingRenderData(const FSlateRenderDataHandle* RenderHandle) = 0;
};

class SLATECORE_API FSlateRenderDataHandle : public TSharedFromThis < FSlateRenderDataHandle, ESPMode::ThreadSafe >
{
public:
	FSlateRenderDataHandle(const ILayoutCache* Cacher, ISlateRenderDataManager* InManager);

	virtual ~FSlateRenderDataHandle();
	
	void Disconnect();

	const ILayoutCache* GetCacher() const { return Cacher; }
	//const FSlateRenderer* GetRenderer() const { return Renderer; }

	void SetRenderBatches(TArray<FSlateRenderBatch>* InRenderBatches) { RenderBatches = InRenderBatches; }
	TArray<FSlateRenderBatch>* GetRenderBatches() { return RenderBatches; }

	void BeginUsing() { FPlatformAtomics::InterlockedIncrement(&UsageCount); }
	void EndUsing() { FPlatformAtomics::InterlockedDecrement(&UsageCount); }

	bool IsInUse() const { return UsageCount > 0; }

private:
	const ILayoutCache* Cacher;
	ISlateRenderDataManager* Manager;
	TArray<FSlateRenderBatch>* RenderBatches;

	volatile int32 UsageCount;
};

/** 
 * Represents an element batch for rendering. 
 */
class FSlateElementBatch
{
public: 
	FSlateElementBatch(const FSlateShaderResource* InShaderResource, const FShaderParams& InShaderParams, ESlateShader::Type ShaderType, ESlateDrawPrimitive::Type PrimitiveType, ESlateDrawEffect::Type DrawEffects, ESlateBatchDrawFlag::Type DrawFlags, const TOptional<FShortRect>& ScissorRect, int32 InstanceCount = 0, uint32 InstanceOffset = 0, ISlateUpdatableInstanceBuffer* InstanceData = nullptr)
		: BatchKey(InShaderParams, ShaderType, PrimitiveType, DrawEffects, DrawFlags, ScissorRect, InstanceCount, InstanceOffset, InstanceData)
		, ShaderResource(InShaderResource)
		, NumElementsInBatch(0)
		, VertexArrayIndex(INDEX_NONE)
		, IndexArrayIndex(INDEX_NONE)
	{}

	FSlateElementBatch( TWeakPtr<ICustomSlateElement, ESPMode::ThreadSafe> InCustomDrawer, const TOptional<FShortRect>& ScissorRect )
		: BatchKey( InCustomDrawer, ScissorRect )
		, ShaderResource( nullptr )
		, NumElementsInBatch(0)
		, VertexArrayIndex(INDEX_NONE)
		, IndexArrayIndex(INDEX_NONE)
	{}

	FSlateElementBatch( TSharedPtr<FSlateRenderDataHandle, ESPMode::ThreadSafe> InCachedRenderHandle, FVector2D InCachedRenderDataOffset, const TOptional<FShortRect>& ScissorRect)
		: BatchKey( InCachedRenderHandle, InCachedRenderDataOffset, ScissorRect )
		, ShaderResource( nullptr )
		, NumElementsInBatch(0)
		, VertexArrayIndex(INDEX_NONE)
		, IndexArrayIndex(INDEX_NONE)
	{}

	FSlateElementBatch( TSharedPtr<FSlateDrawLayerHandle, ESPMode::ThreadSafe> InLayerHandle, const TOptional<FShortRect>& ScissorRect)
		: BatchKey( InLayerHandle, ScissorRect )
		, ShaderResource( nullptr )
		, NumElementsInBatch(0)
		, VertexArrayIndex(INDEX_NONE)
		, IndexArrayIndex(INDEX_NONE)
	{}

	bool operator==( const FSlateElementBatch& Other ) const
	{	
		return BatchKey == Other.BatchKey && ShaderResource == Other.ShaderResource;
	}

	friend uint32 GetTypeHash( const FSlateElementBatch& ElementBatch )
	{
		return PointerHash(ElementBatch.ShaderResource, GetTypeHash(ElementBatch.BatchKey));
	}

	const FSlateShaderResource* GetShaderResource() const { return ShaderResource; }
	const FShaderParams& GetShaderParams() const { return BatchKey.ShaderParams; }
	ESlateBatchDrawFlag::Type GetDrawFlags() const { return BatchKey.DrawFlags; }
	ESlateDrawPrimitive::Type GetPrimitiveType() const { return BatchKey.DrawPrimitiveType; }
	ESlateShader::Type GetShaderType() const { return BatchKey.ShaderType; } 
	ESlateDrawEffect::Type GetDrawEffects() const { return BatchKey.DrawEffects; }
	const TOptional<FShortRect>& GetScissorRect() const { return BatchKey.ScissorRect; }
	const TWeakPtr<ICustomSlateElement, ESPMode::ThreadSafe> GetCustomDrawer() const { return BatchKey.CustomDrawer; }
	const TSharedPtr<FSlateRenderDataHandle, ESPMode::ThreadSafe> GetCachedRenderHandle() const { return BatchKey.CachedRenderHandle; }
	FVector2D GetCachedRenderDataOffset() const { return BatchKey.CachedRenderDataOffset; }
	const TSharedPtr<FSlateDrawLayerHandle, ESPMode::ThreadSafe> GetLayerHandle() const { return BatchKey.LayerHandle; }
	int32 GetInstanceCount() const{ return BatchKey.InstanceCount; }
	uint32 GetInstanceOffset() const{ return BatchKey.InstanceOffset; }
	const ISlateUpdatableInstanceBuffer* GetInstanceData() const { return BatchKey.InstanceData; }

private:
	struct FBatchKey
	{
		const TWeakPtr<ICustomSlateElement, ESPMode::ThreadSafe> CustomDrawer;
		const TSharedPtr<FSlateRenderDataHandle, ESPMode::ThreadSafe> CachedRenderHandle;
		const FVector2D CachedRenderDataOffset;
		const TSharedPtr<FSlateDrawLayerHandle, ESPMode::ThreadSafe> LayerHandle;
		const FShaderParams ShaderParams;
		const ESlateBatchDrawFlag::Type DrawFlags;	
		const ESlateShader::Type ShaderType;
		const ESlateDrawPrimitive::Type DrawPrimitiveType;
		const ESlateDrawEffect::Type DrawEffects;
		const TOptional<FShortRect> ScissorRect;
		const int32 InstanceCount;
		const uint32 InstanceOffset;
		const ISlateUpdatableInstanceBuffer* InstanceData;

		FBatchKey( const FShaderParams& InShaderParams, ESlateShader::Type InShaderType, ESlateDrawPrimitive::Type InDrawPrimitiveType, ESlateDrawEffect::Type InDrawEffects, ESlateBatchDrawFlag::Type InDrawFlags, const TOptional<FShortRect>& InScissorRect, int32 InInstanceCount, uint32 InInstanceOffset, ISlateUpdatableInstanceBuffer* InInstanceBuffer)
			: ShaderParams( InShaderParams )
			, DrawFlags( InDrawFlags )
			, ShaderType( InShaderType )
			, DrawPrimitiveType( InDrawPrimitiveType )
			, DrawEffects( InDrawEffects )
			, ScissorRect( InScissorRect )
			, InstanceCount( InInstanceCount )
			, InstanceOffset( InInstanceOffset )
			, InstanceData( InInstanceBuffer )
		{
		}

		FBatchKey( TWeakPtr<ICustomSlateElement, ESPMode::ThreadSafe> InCustomDrawer, const TOptional<FShortRect>& InScissorRect )
			: CustomDrawer( InCustomDrawer )
			, ShaderParams()
			, DrawFlags( ESlateBatchDrawFlag::None )
			, ShaderType( ESlateShader::Default )
			, DrawPrimitiveType( ESlateDrawPrimitive::TriangleList )
			, DrawEffects( ESlateDrawEffect::None )
			, ScissorRect( InScissorRect )
			, InstanceCount(0)
			, InstanceOffset(0)
		{}

		FBatchKey( TSharedPtr<FSlateRenderDataHandle, ESPMode::ThreadSafe> InCachedRenderHandle, FVector2D InCachedRenderDataOffset, const TOptional<FShortRect>& InScissorRect)
			: CachedRenderHandle(InCachedRenderHandle)
			, CachedRenderDataOffset(InCachedRenderDataOffset)
			, ShaderParams()
			, DrawFlags(ESlateBatchDrawFlag::None)
			, ShaderType(ESlateShader::Default)
			, DrawPrimitiveType(ESlateDrawPrimitive::TriangleList)
			, DrawEffects(ESlateDrawEffect::None)
			, ScissorRect(InScissorRect)
			, InstanceCount(0)
			, InstanceOffset(0)
		{}

		FBatchKey(TSharedPtr<FSlateDrawLayerHandle, ESPMode::ThreadSafe> InLayerHandle, const TOptional<FShortRect>& InScissorRect)
			: LayerHandle(InLayerHandle)
			, ShaderParams()
			, DrawFlags(ESlateBatchDrawFlag::None)
			, ShaderType(ESlateShader::Default)
			, DrawPrimitiveType(ESlateDrawPrimitive::TriangleList)
			, DrawEffects(ESlateDrawEffect::None)
			, ScissorRect(InScissorRect)
			, InstanceCount(0)
			, InstanceOffset(0)
		{}

		bool operator==( const FBatchKey& Other ) const
		{
			return DrawFlags == Other.DrawFlags
				&& ShaderType == Other.ShaderType
				&& DrawPrimitiveType == Other.DrawPrimitiveType
				&& DrawEffects == Other.DrawEffects
				&& ShaderParams == Other.ShaderParams
				&& ScissorRect == Other.ScissorRect
				&& CustomDrawer == Other.CustomDrawer
				&& CachedRenderHandle == Other.CachedRenderHandle
				&& LayerHandle == Other.LayerHandle
				&& InstanceCount == Other.InstanceCount
				&& InstanceOffset == Other.InstanceOffset
				&& InstanceData == Other.InstanceData;
		}

		/** Compute an efficient hash for this type for use in hash containers. */
		friend uint32 GetTypeHash( const FBatchKey& InBatchKey )
		{
			// NOTE: Assumes these enum types are 8 bits.
			uint32 RunningHash = (uint32)InBatchKey.DrawFlags << 24 | (uint32)InBatchKey.ShaderType << 16 | (uint32)InBatchKey.DrawPrimitiveType << 8 | (uint32)InBatchKey.DrawEffects << 0;
			RunningHash = InBatchKey.CustomDrawer.IsValid() ? PointerHash(InBatchKey.CustomDrawer.Pin().Get(), RunningHash) : RunningHash;
			RunningHash = InBatchKey.CachedRenderHandle.IsValid() ? PointerHash(InBatchKey.CachedRenderHandle.Get(), RunningHash) : RunningHash;
			RunningHash = HashCombine(GetTypeHash(InBatchKey.ShaderParams.PixelParams), RunningHash);
			// NOTE: Assumes this type is 64 bits, no padding.
			RunningHash = InBatchKey.ScissorRect.IsSet() ? HashCombine(GetTypeHash(*reinterpret_cast<const uint64*>(&InBatchKey.ScissorRect.GetValue())), RunningHash) : RunningHash;
			const bool bHasInstances = InBatchKey.InstanceCount > 0;
			RunningHash = bHasInstances ? HashCombine(InBatchKey.InstanceCount, RunningHash) : RunningHash;
			RunningHash = bHasInstances ? HashCombine(InBatchKey.InstanceOffset, RunningHash) : RunningHash;
			RunningHash = InBatchKey.InstanceData ? HashCombine( PointerHash(InBatchKey.InstanceData), RunningHash ) : RunningHash;

			return RunningHash;
			//return FCrc::MemCrc32(&InBatchKey.ShaderParams, sizeof(FShaderParams)) ^ ((InBatchKey.ShaderType << 16) | (InBatchKey.DrawFlags+InBatchKey.ShaderType+InBatchKey.DrawPrimitiveType+InBatchKey.DrawEffects));
		}
	};

	/** A secondary key which represents elements needed to make a batch unique */
	FBatchKey BatchKey;
	/** Shader resource to use with this batch.  Used as a primary key.  No batch can have multiple textures */
	const FSlateShaderResource* ShaderResource;

public:
	/** Number of elements in the batch */
	uint32 NumElementsInBatch;
	/** Index into an array of vertex arrays where this batches vertices are found (before submitting to the vertex buffer)*/
	int32 VertexArrayIndex;
	/** Index into an array of index arrays where this batches indices are found (before submitting to the index buffer) */
	int32 IndexArrayIndex;
};

class FSlateRenderBatch
{
public:
	FSlateRenderBatch(uint32 InLayer, const FSlateElementBatch& InBatch, TSharedPtr<FSlateRenderDataHandle, ESPMode::ThreadSafe> InRenderHandle, int32 InNumVertices, int32 InNumIndices, int32 InVertexOffset, int32 InIndexOffset)
		: Layer( InLayer )
		, ShaderParams( InBatch.GetShaderParams() )
		, Texture( InBatch.GetShaderResource() )
		, InstanceData( InBatch.GetInstanceData() )
		, InstanceCount( InBatch.GetInstanceCount() )
		, InstanceOffset(InBatch.GetInstanceOffset() )
		, CustomDrawer( InBatch.GetCustomDrawer() )
		, LayerHandle( InBatch.GetLayerHandle() )
		, CachedRenderHandle( InRenderHandle )
		, DrawFlags( InBatch.GetDrawFlags() )
		, ShaderType( InBatch.GetShaderType() )
		, DrawPrimitiveType( InBatch.GetPrimitiveType() )
		, DrawEffects( InBatch.GetDrawEffects() )
		, ScissorRect( InBatch.GetScissorRect() )
		, VertexArrayIndex( InBatch.VertexArrayIndex )
		, IndexArrayIndex( InBatch.IndexArrayIndex )
		, VertexOffset( InVertexOffset )
		, IndexOffset( InIndexOffset )
		, NumVertices( InNumVertices )
		, NumIndices( InNumIndices )
	{}

public:
	/** The layer we need to sort by when  */
	const uint32 Layer;

	/** Dynamically modified offset that occurs when we have relative position stored render batches. */
	FVector2D DynamicOffset;

	const FShaderParams ShaderParams;

	/** Texture to use with this batch.  */
	const FSlateShaderResource* Texture;

	const ISlateUpdatableInstanceBuffer* InstanceData;

	const int32 InstanceCount;

	const uint32 InstanceOffset;

	const TWeakPtr<ICustomSlateElement, ESPMode::ThreadSafe> CustomDrawer;

	const TWeakPtr<FSlateDrawLayerHandle, ESPMode::ThreadSafe> LayerHandle;

	const TSharedPtr<FSlateRenderDataHandle, ESPMode::ThreadSafe> CachedRenderHandle;

	const ESlateBatchDrawFlag::Type DrawFlags;	

	const ESlateShader::Type ShaderType;

	const ESlateDrawPrimitive::Type DrawPrimitiveType;

	const ESlateDrawEffect::Type DrawEffects;

	const TOptional<FShortRect> ScissorRect;
	
	/** Index into the vertex array pool */
	const int32 VertexArrayIndex;
	/** Index into the index array pool */
	const int32 IndexArrayIndex;
	/** How far into the vertex buffer is this batch*/
	const uint32 VertexOffset;
	/** How far into the index buffer this batch is*/
	const uint32 IndexOffset;
	/** Number of vertices in the batch */
	const uint32 NumVertices;
	/** Number of indices in the batch */
	const uint32 NumIndices;
};


typedef TArray<FSlateElementBatch, TInlineAllocator<1>> FElementBatchArray;

class FElementBatchMap
{
public:
	FElementBatchMap()
	{
		Reset();
	}

	FORCEINLINE_DEBUGGABLE int32 Num() const { return Layers.Num() + OverflowLayers.Num(); }
	
	FORCEINLINE_DEBUGGABLE TUniqueObj<FElementBatchArray>* Find(uint32 Layer)
	{
		if ( Layer < (uint32)Layers.Num() )
		{
			if ( ActiveLayers[Layer] )
			{
				return &Layers[Layer];
			}

			return nullptr;
		}
		else
		{
			return OverflowLayers.Find(Layer);
		}
	}

	FORCEINLINE_DEBUGGABLE TUniqueObj<FElementBatchArray>& Add(uint32 Layer)
	{
		if ( Layer < (uint32)Layers.Num() )
		{
			MinLayer = FMath::Min(Layer, MinLayer);
			MaxLayer = FMath::Max(Layer, MaxLayer);
			ActiveLayers[Layer] = true;
			return Layers[Layer];
		}
		else
		{
			return OverflowLayers.Add(Layer);
		}
	}

	FORCEINLINE_DEBUGGABLE void Sort()
	{
		OverflowLayers.KeySort(TLess<uint32>());
	}

	template <typename TFunc>
	FORCEINLINE_DEBUGGABLE void ForEachLayer(const TFunc& Process)
	{
		if ( MinLayer < (uint32)Layers.Num() )
		{
			for ( TBitArray<>::FConstIterator BitIt(ActiveLayers, MinLayer); BitIt; ++BitIt )
			{
				if ( BitIt.GetValue() == 0 )
				{
					continue;
				}

				const int32 BitIndex = BitIt.GetIndex();
				FElementBatchArray& ElementBatches = *Layers[BitIndex];

				if ( ElementBatches.Num() > 0 )
				{
					Process(BitIndex, ElementBatches);
				}

				if ( ((uint32)BitIndex) >= MaxLayer )
				{
					break;
				}
			}
		}

		for ( TMap<uint32, TUniqueObj<FElementBatchArray>>::TIterator It(OverflowLayers); It; ++It )
		{
			uint32 Layer = It.Key();
			FElementBatchArray& ElementBatches = *It.Value();

			if ( ElementBatches.Num() > 0 )
			{
				Process(Layer, ElementBatches);
			}
		}
	}

	FORCEINLINE_DEBUGGABLE void Reset()
	{
		MinLayer = UINT_MAX;
		MaxLayer = 0;
		ActiveLayers.Init(false, Layers.Num());
		OverflowLayers.Reset();
	}

private:
	TBitArray<> ActiveLayers;
	TStaticArray<TUniqueObj<FElementBatchArray>, 256> Layers;
	TMap<uint32, TUniqueObj<FElementBatchArray>> OverflowLayers;
	uint32 MinLayer;
	uint32 MaxLayer;
};

#if STATS

class FSlateStatTrackingMemoryAllocator : public FDefaultAllocator
{
public:
	typedef FDefaultAllocator Super;

	class ForAnyElementType : public FDefaultAllocator::ForAnyElementType
	{
	public:
		typedef FDefaultAllocator::ForAnyElementType Super;

		ForAnyElementType()
			: AllocatedSize(0)
		{

		}

		/** Destructor. */
		~ForAnyElementType()
		{
			if(AllocatedSize)
			{
				DEC_DWORD_STAT_BY(STAT_SlateBufferPoolMemory, AllocatedSize);
			}
		}

		void ResizeAllocation(int32 PreviousNumElements, int32 NumElements, int32 NumBytesPerElement)
		{
			const int32 NewSize = NumElements * NumBytesPerElement;
			INC_DWORD_STAT_BY(STAT_SlateBufferPoolMemory, NewSize - AllocatedSize);
			AllocatedSize = NewSize;

			Super::ResizeAllocation(PreviousNumElements, NumElements, NumBytesPerElement);
		}

	private:
		ForAnyElementType(const ForAnyElementType&);
		ForAnyElementType& operator=(const ForAnyElementType&);
	private:
		int32 AllocatedSize;
	};
};

typedef TArray<FSlateVertex, FSlateStatTrackingMemoryAllocator> FSlateVertexArray;
typedef TArray<SlateIndex, FSlateStatTrackingMemoryAllocator> FSlateIndexArray;

#else

typedef TArray<FSlateVertex> FSlateVertexArray;
typedef TArray<SlateIndex> FSlateIndexArray;

#endif


class FSlateBatchData
{
public:
	FSlateBatchData()
		: DynamicOffset(0, 0)
		, NumBatchedVertices(0)
		, NumBatchedIndices(0)
		, NumLayers(0)
	{}

	void Reset();

	/**
	 * Returns a list of element batches for this window
	 */
	const TArray<FSlateRenderBatch>& GetRenderBatches() const { return RenderBatches; }

	/**
	 * Assigns a vertex array from the pool which is appropriate for the batch.  Creates a new array if needed
	 */
	void AssignVertexArrayToBatch( FSlateElementBatch& Batch );

	/**
	 * Assigns an index array from the pool which is appropriate for the batch.  Creates a new array if needed
	 */
	void AssignIndexArrayToBatch( FSlateElementBatch& Batch );

	/** @return the list of vertices for a batch */
	FSlateVertexArray& GetBatchVertexList( FSlateElementBatch& Batch ) { return BatchVertexArrays[Batch.VertexArrayIndex]; }

	/** @return the list of indices for a batch */
	FSlateIndexArray& GetBatchIndexList( FSlateElementBatch& Batch ) { return BatchIndexArrays[Batch.IndexArrayIndex]; }

	/** @return The total number of batched vertices */
	int32 GetNumBatchedVertices() const { return NumBatchedVertices; }

	/** @return The total number of batched indices */
	int32 GetNumBatchedIndices() const { return NumBatchedIndices; }

	/** @return Total number of batched layers */
	int32 GetNumLayers() const { return NumLayers; }

	/** Set the associated vertex/index buffer handle. */
	void SetRenderDataHandle(TSharedPtr<FSlateRenderDataHandle, ESPMode::ThreadSafe> InRenderDataHandle) { RenderDataHandle = InRenderDataHandle; }

	/** 
	 * Fills batch data into the actual vertex and index buffer
	 *
	 * @param VertexBuffer	Pointer to the actual memory for the vertex buffer 
	 * @param IndexBuffer	Pointer to the actual memory for an index buffer
	 */
	SLATECORE_API void FillVertexAndIndexBuffer( uint8* VertexBuffer, uint8* IndexBuffer );

	/** 
	 * Creates rendering data from batched elements
	 */
	SLATECORE_API void CreateRenderBatches(FElementBatchMap& LayerToElementBatches);

private:
	void Merge(FElementBatchMap& InLayerToElementBatches, uint32& VertexOffset, uint32& IndexOffset);

	void AddRenderBatch(uint32 InLayer, const FSlateElementBatch& InElementBatch, int32 InNumVertices, int32 InNumIndices, int32 InVertexOffset, int32 InIndexOffset);

	/**
	 * Resets an array from the pool of vertex arrays
	 * This will empty the array and give it a reasonable starting memory amount for when it is reused
	 */
	void ResetVertexArray(FSlateVertexArray& InOutVertexArray);

	/**
	* Resets an array from the pool of index arrays
	* This will empty the array and give it a reasonable starting memory amount for when it is reused
	*/
	void ResetIndexArray(FSlateIndexArray& InOutIndexArray);

private:

	// The associated render data handle if these render batches are not in the default vertex/index buffer
	TSharedPtr<FSlateRenderDataHandle, ESPMode::ThreadSafe> RenderDataHandle;

	// Array of vertex lists that are currently free (have no elements in them).
	TArray<uint32> VertexArrayFreeList;

	// Array of index lists that are currently free (have no elements in them).
	TArray<uint32> IndexArrayFreeList;

	// Array of vertex lists for batching vertices. We use this method for quickly resetting the arrays without deleting memory.
	TArray<FSlateVertexArray> BatchVertexArrays;

	// Array of vertex lists for batching indices. We use this method for quickly resetting the arrays without deleting memory.
	TArray<FSlateIndexArray> BatchIndexArrays;

	/** List of element batches sorted by later for use in rendering (for threaded renderers, can only be accessed from the render thread)*/
	TArray<FSlateRenderBatch> RenderBatches;

	FVector2D DynamicOffset;

	int32 NumBatchedVertices;

	int32 NumBatchedIndices;

	int32 NumLayers;

};

class FSlateRenderer;

class FSlateDrawLayer
{
public:
	FElementBatchMap& GetElementBatchMap() { return LayerToElementBatches; }

public:
	// Element batch maps sorted by layer.
	FElementBatchMap LayerToElementBatches;

#if SLATE_POOL_DRAW_ELEMENTS
	/** The elements drawn on this layer */
	TArray<FSlateDrawElement*> DrawElements;
#else
	/** The elements drawn on this layer */
	TArray<FSlateDrawElement> DrawElements;
#endif
};

/**
 * 
 */
class FSlateDrawLayerHandle : public TSharedFromThis < FSlateDrawLayerHandle, ESPMode::ThreadSafe >
{
public:
	FSlateDrawLayerHandle()
		: BatchMap(nullptr)
	{
	}

	FElementBatchMap* BatchMap;
};


/**
 * Represents a top level window and its draw elements.
 */
class FSlateWindowElementList
{
	friend class FSlateElementBatcher;

public:
	/** 
	 * Construct a new list of elements with which to paint a window.
	 *
	 * @param InWindow   The window that we will be painting
	 */
	explicit FSlateWindowElementList( TSharedPtr<SWindow> InWindow = TSharedPtr<SWindow>() )
		: TopLevelWindow( InWindow )
		, bNeedsDeferredResolve( false )
		, ResolveToDeferredIndex()
		, MemManager(0)
	{
		DrawStack.Push(&RootDrawLayer);
	}
	
	/** @return Get the window that we will be painting */
	FORCEINLINE TSharedPtr<SWindow> GetWindow() const
	{
		return TopLevelWindow.Pin();
	}
	
#if SLATE_POOL_DRAW_ELEMENTS
	
	/** @return Get the draw elements that we want to render into this window */
	FORCEINLINE const TArray<FSlateDrawElement*>& GetDrawElements() const
	{
		return RootDrawLayer.DrawElements;
	}

#else

	/** @return Get the draw elements that we want to render into this window */
	FORCEINLINE const TArray<FSlateDrawElement>& GetDrawElements() const
	{
		return RootDrawLayer.DrawElements;
	}

	/**
	 * Add a draw element to the list
	 *
	 * @param InDrawElement  The draw element to add
	 */
	FORCEINLINE void AddItem(const FSlateDrawElement& InDrawElement)
	{
		TArray<FSlateDrawElement>& ActiveDrawElements = DrawStack.Last()->DrawElements;
		ActiveDrawElements.Add(InDrawElement);
	}
#endif

	/**
	 * Creates an uninitialized draw element
	 */
	FORCEINLINE FSlateDrawElement& AddUninitialized()
	{
#if SLATE_POOL_DRAW_ELEMENTS
		FSlateDrawElement* DrawElement = ( DrawElementFreePool.Num() > 0 ) ? DrawElementFreePool.Pop() : new FSlateDrawElement();
		DrawStack.Last()->DrawElements.Push(DrawElement);

		return *DrawElement;
#else
		TArray<FSlateDrawElement>& ActiveDrawElements = DrawStack.Last()->DrawElements;
		const int32 InsertIdx = ActiveDrawElements.AddDefaulted();
		return ActiveDrawElements[InsertIdx];
#endif
	}

#if SLATE_POOL_DRAW_ELEMENTS
	/**
	 * Append draw elements to the list of draw elements
	 */
	FORCEINLINE void AppendDrawElements(const TArray<FSlateDrawElement*>& InDrawElements)
	{
		TArray<FSlateDrawElement*>& ActiveDrawElements = DrawStack.Last()->DrawElements;
		ActiveDrawElements.Append(InDrawElements);
	}
#else
	/**
	* Append draw elements to the list of draw elements
	*/
	FORCEINLINE void AppendDrawElements(const TArray<FSlateDrawElement>& InDrawElements)
	{
		TArray<FSlateDrawElement>& ActiveDrawElements = DrawStack.Last()->DrawElements;
		ActiveDrawElements.Append(InDrawElements);
	}
#endif

	/**
	 * Some widgets may want to paint their children after after another, loosely-related widget finished painting.
	 * Or they may want to paint "after everyone".
	 */
	struct SLATECORE_API FDeferredPaint
	{
	public:
		FDeferredPaint( const TSharedRef<const SWidget>& InWidgetToPaint, const FPaintArgs& InArgs, const FGeometry InAllottedGeometry, const FSlateRect InMyClippingRect, const FWidgetStyle& InWidgetStyle, bool InParentEnabled );

		int32 ExecutePaint( int32 LayerId, FSlateWindowElementList& OutDrawElements ) const;

		FDeferredPaint Copy(const FPaintArgs& InArgs);

	private:
		// Used for making copies.
		FDeferredPaint(const FDeferredPaint& Copy, const FPaintArgs& InArgs);

		const TWeakPtr<const SWidget> WidgetToPaintPtr;
		const FPaintArgs Args;
		const FGeometry AllottedGeometry;
		const FSlateRect MyClippingRect;
		const FWidgetStyle WidgetStyle;
		const bool bParentEnabled;
	};

	SLATECORE_API void QueueDeferredPainting( const FDeferredPaint& InDeferredPaint );

	int32 PaintDeferred(int32 LayerId);

	bool ShouldResolveDeferred() const { return bNeedsDeferredResolve; }

	SLATECORE_API void BeginDeferredGroup();
	SLATECORE_API void EndDeferredGroup();

	TArray< TSharedPtr<FDeferredPaint> > GetDeferredPaintList() const { return DeferredPaintList; }

	struct FVolatilePaint
	{
	public:
		SLATECORE_API FVolatilePaint(const TSharedRef<const SWidget>& InWidgetToPaint, const FPaintArgs& InArgs, const FGeometry InAllottedGeometry, const FSlateRect InMyClippingRect, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool InParentEnabled);

		int32 ExecutePaint(FSlateWindowElementList& OutDrawElements) const;

		FORCEINLINE const SWidget* GetWidget() const { return WidgetToPaintPtr.Pin().Get(); }
		FORCEINLINE FGeometry GetGeometry() const { return AllottedGeometry; }
		FORCEINLINE int32 GetLayerId() const { return LayerId; }

	public:
		TSharedPtr< FSlateDrawLayerHandle, ESPMode::ThreadSafe > LayerHandle;
	 
	private:
		const TWeakPtr<const SWidget> WidgetToPaintPtr;
		const FPaintArgs Args;
		const FGeometry AllottedGeometry;
		const FSlateRect MyClippingRect;
		const int32 LayerId;
		const FWidgetStyle WidgetStyle;
		const bool bParentEnabled;
	};

	SLATECORE_API void QueueVolatilePainting( const FVolatilePaint& InVolatilePaint );

	SLATECORE_API int32 PaintVolatile(FSlateWindowElementList& OutElementList);

	SLATECORE_API void BeginLogicalLayer(const TSharedPtr<FSlateDrawLayerHandle, ESPMode::ThreadSafe>& LayerHandle);
	SLATECORE_API void EndLogicalLayer();

	SLATECORE_API const TArray< TSharedPtr<FVolatilePaint> >& GetVolatileElements() const { return VolatilePaintList; }
	
	/**
	 * Remove all the elements from this draw list.
	 */
	SLATECORE_API void ResetBuffers();

	/**
	 * Allocate memory that remains valid until ResetBuffers is called.
	 */
	FORCEINLINE_DEBUGGABLE void* Alloc(int32 AllocSize, int32 Alignment = MIN_ALIGNMENT)
	{
		return MemManager.Alloc(AllocSize, Alignment);
	}

	/**
	 * Allocate memory for a type that remains valid until ResetBuffers is called.
	 */
	template <typename T>
	FORCEINLINE_DEBUGGABLE void* Alloc()
	{
		return MemManager.Alloc(sizeof(T), ALIGNOF(T));
	}

	FSlateBatchData& GetBatchData() { return BatchData; }

	FSlateDrawLayer& GetRootDrawLayer() { return RootDrawLayer; }

	TMap < TSharedPtr<FSlateDrawLayerHandle, ESPMode::ThreadSafe>, TSharedPtr<FSlateDrawLayer> >& GetChildDrawLayers() { return DrawLayers; }

	/**
	 * Caches this element list on the renderer, generating all needed index and vertex buffers.
	 */
	SLATECORE_API TSharedRef<FSlateRenderDataHandle, ESPMode::ThreadSafe> CacheRenderData(const ILayoutCache* Cacher);

	SLATECORE_API TSharedPtr<FSlateRenderDataHandle, ESPMode::ThreadSafe> GetCachedRenderDataHandle() const
	{
		return CachedRenderDataHandle.Pin();
	}

	void BeginUsingCachedBuffer(TSharedPtr<FSlateRenderDataHandle, ESPMode::ThreadSafe>& InCachedRenderDataHandle)
	{
		InCachedRenderDataHandle->BeginUsing();
		CachedRenderHandlesInUse.Add(InCachedRenderDataHandle);
	}

	SLATECORE_API bool IsCachedRenderDataInUse() const
	{
		TSharedPtr<FSlateRenderDataHandle, ESPMode::ThreadSafe> SafeHandle = CachedRenderDataHandle.Pin();
		return SafeHandle.IsValid() && SafeHandle->IsInUse();
	}

	SLATECORE_API void PreDraw_ParallelThread();

	SLATECORE_API void PostDraw_ParallelThread();

private:

	/** The top level window which these elements are being drawn on */
	TWeakPtr<SWindow> TopLevelWindow;

	/** Batched data used for rendering */
	FSlateBatchData BatchData;

	/** The base draw layer/context. */
	FSlateDrawLayer RootDrawLayer;

	/** */
	TMap < TSharedPtr<FSlateDrawLayerHandle, ESPMode::ThreadSafe>, TSharedPtr<FSlateDrawLayer> > DrawLayers;

	/** */
	TArray< TSharedPtr<FSlateDrawLayer> > DrawLayerPool;

	/** */
	TArray< FSlateDrawLayer* > DrawStack;

#if SLATE_POOL_DRAW_ELEMENTS
	/** List of draw elements for the window */
	TArray<FSlateDrawElement*> DrawElementFreePool;
#endif

	TArray< TSharedPtr<FSlateRenderDataHandle, ESPMode::ThreadSafe> > CachedRenderHandlesInUse;

	/**
	 * Some widgets want their logical children to appear at a different "layer" in the physical hierarchy.
	 * We accomplish this by deferring their painting.
	 */
	TArray< TSharedPtr<FDeferredPaint> > DeferredPaintList;

	bool bNeedsDeferredResolve;
	TArray<int32> ResolveToDeferredIndex;

	/** The widgets be cached for a later paint pass when the invalidation host paints. */
	TArray< TSharedPtr<FVolatilePaint> > VolatilePaintList;

	/**
	 * Handle to the cached render data associated with this element list.  Will only exist if 
	 * this element list is being used for invalidation / caching.
	 */
	mutable TWeakPtr<FSlateRenderDataHandle, ESPMode::ThreadSafe> CachedRenderDataHandle;

	// Mem stack for temp allocations
	FMemStackBase MemManager; 
};
