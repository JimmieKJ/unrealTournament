// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


class SWindow;
class FSlateViewportInterface;


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

	// Box Data
	FVector2D RotationPoint;
	float Angle;

	// Spline/Line Data
	float Thickness;

	// Font data
	FSlateFontInfo FontInfo;
	FString Text;
	
	// Gradient data
	TArray<FSlateGradientStop> GradientStops;
	EOrientation GradientType;

	// Line data
	TArray<FVector2D> Points;

	// Viewport data (intentionally weak to allow the source element to be destructed after this element has been added for drawing)
	TWeakPtr<const ISlateViewport> Viewport;

	// Misc data
	bool bGammaCorrect;
	bool bAllowBlending;

	// Custom drawer data
	TWeakPtr<ICustomSlateElement, ESPMode::ThreadSafe> CustomDrawer;

	// Line data
	ESlateLineJoinType::Type SegmentJoinType;
	bool bAntialias;

	FSlateDataPayload()
		: Tint(FLinearColor::White)
		, BrushResource(nullptr)
		, RotationPoint(FVector2D::ZeroVector)
		, Viewport(nullptr)
	{ }

	void SetBoxPayloadProperties( const FSlateBrush* InBrush, const FLinearColor& InTint )
	{
		Tint = InTint;
		BrushResource = InBrush;
		Angle = 0.0f;
	}

	void SetRotatedBoxPayloadProperties( const FSlateBrush* InBrush, float InAngle, const FVector2D& LocalRotationPoint, const FLinearColor& InTint )
	{
		Tint = InTint;
		BrushResource = InBrush;
		RotationPoint = LocalRotationPoint;
		RotationPoint.DiagnosticCheckNaN();
		Angle = InAngle;
	}

	void SetTextPayloadProperties( const FString& InText, const FSlateFontInfo& InFontInfo, const FLinearColor& InTint )
	{
		Tint = InTint;
		FontInfo = InFontInfo;
		Text = InText;
	}

	void SetGradientPayloadProperties( const TArray<FSlateGradientStop>& InGradientStops, EOrientation InGradientType, bool bInGammaCorrect )
	{
		GradientStops = InGradientStops;
		GradientType = InGradientType;
		bGammaCorrect = bInGammaCorrect;
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

	void SetLinesPayloadProperties( const TArray<FVector2D>& InPoints, const FLinearColor& InTint, bool bInAntialias, ESlateLineJoinType::Type InJoinType )
	{
		Tint = InTint;
		Thickness = 0.0f;
		Points = InPoints;
		SegmentJoinType = InJoinType;
		bAntialias = bInAntialias;
	}

	void SetViewportPayloadProperties( const TSharedPtr<const ISlateViewport>& InViewport, const FLinearColor& InTint, bool bInGammaCorrect, bool bInAllowBlending )
	{
		Tint = InTint;
		Viewport = InViewport;
		bGammaCorrect = bInGammaCorrect;
		bAllowBlending = bInAllowBlending;
	}

	void SetCustomDrawerPayloadProperties( const TSharedPtr<ICustomSlateElement, ESPMode::ThreadSafe>& InCustomDrawer )
	{
		CustomDrawer = InCustomDrawer;
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
		ET_Spline,
		ET_Line,
		ET_Gradient,
		ET_Viewport,
		ET_Border,
		ET_Custom,
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
	SLATECORE_API static void MakeGradient( FSlateWindowElementList& ElementList, uint32 InLayer, const FPaintGeometry& PaintGeometry, TArray<FSlateGradientStop> InGradientStops, EOrientation InGradientType, const FSlateRect& InClippingRect, ESlateDrawEffect::Type InDrawEffects = ESlateDrawEffect::None, bool bGammaCorrect = true );

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
	SLATECORE_API static void MakeDrawSpaceSpline( FSlateWindowElementList& ElementList, uint32 InLayer, const FVector2D& InStart, const FVector2D& InStartDir, const FVector2D& InEnd, const FVector2D& InEndDir, const FSlateRect InClippingRect, float InThickness = 0.0f, ESlateDrawEffect::Type InDrawEffects = ESlateDrawEffect::None, const FColor& InTint=FColor(255,255,255) );

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
	 */
	SLATECORE_API static void MakeLines( FSlateWindowElementList& ElementList, uint32 InLayer, const FPaintGeometry& PaintGeometry, const TArray<FVector2D>& Points, const FSlateRect InClippingRect, ESlateDrawEffect::Type InDrawEffects = ESlateDrawEffect::None, const FLinearColor& InTint=FLinearColor::White, bool bAntialias = true );

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
	SLATECORE_API static void MakeViewport( FSlateWindowElementList& ElementList, uint32 InLayer, const FPaintGeometry& PaintGeometry, TSharedPtr<const ISlateViewport> Viewport, const FSlateRect& InClippingRect, bool bGammaCorrect = true, bool bAllowBlending = true, ESlateDrawEffect::Type InDrawEffects = ESlateDrawEffect::None, const FLinearColor& InTint=FLinearColor::White );

	/**
	 * Creates a custom element which can be used to manually draw into the Slate render target with graphics API calls rather than Slate elements
	 *
	 * @param ElementList		   The list in which to add elements
	 * @param InLayer                  The layer to draw the element on
	 * @param PaintGeometry            DrawSpace position and dimensions; see FPaintGeometry
	 * @param CustomDrawer		   Interface to a drawer which will be called when Slate renders this element
	 */
	SLATECORE_API static void MakeCustom( FSlateWindowElementList& ElementList, uint32 InLayer, TSharedPtr<ICustomSlateElement, ESPMode::ThreadSafe> CustomDrawer );


	EElementType GetElementType() const { return ElementType; }
	uint32 GetLayer() const { return Layer; }
	const FSlateRenderTransform& GetRenderTransform() const { return RenderTransform; }
	const FVector2D& GetPosition() const { return Position; }
	const FVector2D& GetLocalSize() const { return LocalSize; }
	float GetScale() const { return Scale; }
	const FSlateRect& GetClippingRect() const { return ClippingRect; }
	const FSlateDataPayload& GetDataPayload() const { return DataPayload; }
	uint32 GetDrawEffects() const { return DrawEffects; }
	const TOptional<FShortRect>& GetScissorRect() const { return ScissorRect; }

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

/** 
 * Represents an element batch for rendering. 
 */
class FSlateElementBatch
{
public: 
	FSlateElementBatch( const FSlateShaderResource* InShaderResource, const FShaderParams& InShaderParams, ESlateShader::Type ShaderType, ESlateDrawPrimitive::Type PrimitiveType, ESlateDrawEffect::Type DrawEffects, ESlateBatchDrawFlag::Type DrawFlags, const TOptional<FShortRect>& ScissorRect )
		: BatchKey( InShaderParams, ShaderType, PrimitiveType, DrawEffects, DrawFlags, ScissorRect )
		, ShaderResource(InShaderResource)
		, VertexOffset(0)
		, IndexOffset(0)
		, NumVertices(0)
		, NumIndices(0)
		, NumElementsInBatch(0)
		, VertexArrayIndex(INDEX_NONE)
		, IndexArrayIndex(INDEX_NONE)
	{}

	FSlateElementBatch( TWeakPtr<ICustomSlateElement, ESPMode::ThreadSafe> InCustomDrawer, const TOptional<FShortRect>& ScissorRect )
		: BatchKey( InCustomDrawer, ScissorRect )
		, ShaderResource( nullptr )
		, VertexOffset(0)
		, IndexOffset(0)
		, NumVertices(0)
		, NumIndices(0)
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
private:
	struct FBatchKey
	{
		const TWeakPtr<ICustomSlateElement, ESPMode::ThreadSafe> CustomDrawer;
		const FShaderParams ShaderParams;
		const ESlateBatchDrawFlag::Type DrawFlags;	
		const ESlateShader::Type ShaderType;
		const ESlateDrawPrimitive::Type DrawPrimitiveType;
		const ESlateDrawEffect::Type DrawEffects;
		const TOptional<FShortRect> ScissorRect;

		FBatchKey( const FShaderParams& InShaderParams, ESlateShader::Type InShaderType, ESlateDrawPrimitive::Type InDrawPrimitiveType, ESlateDrawEffect::Type InDrawEffects, ESlateBatchDrawFlag::Type InDrawFlags, const TOptional<FShortRect>& InScissorRect )
			: ShaderParams( InShaderParams )
			, DrawFlags( InDrawFlags )
			, ShaderType( InShaderType )
			, DrawPrimitiveType( InDrawPrimitiveType )
			, DrawEffects( InDrawEffects )
			, ScissorRect( InScissorRect )
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
				;
		}

		/** Compute an efficient hash for this type for use in hash containers. */
		friend uint32 GetTypeHash( const FBatchKey& InBatchKey )
		{
			// NOTE: Assumes these enum types are 8 bits.
			uint32 RunningHash = (uint32)InBatchKey.DrawFlags << 24 | (uint32)InBatchKey.ShaderType << 16 | (uint32)InBatchKey.DrawPrimitiveType << 8 | (uint32)InBatchKey.DrawEffects << 0;
			RunningHash = InBatchKey.CustomDrawer.IsValid() ? PointerHash(InBatchKey.CustomDrawer.Pin().Get(), RunningHash) : RunningHash;
			RunningHash = HashCombine(GetTypeHash(InBatchKey.ShaderParams.PixelParams), RunningHash);
			// NOTE: Assumes this type is 64 bits, no padding.
			RunningHash = InBatchKey.ScissorRect.IsSet() ? HashCombine(GetTypeHash(*reinterpret_cast<const uint64*>(&InBatchKey.ScissorRect.GetValue())), RunningHash) : RunningHash;
			return RunningHash;
			//return FCrc::MemCrc32(&InBatchKey.ShaderParams, sizeof(FShaderParams)) ^ ((InBatchKey.ShaderType << 16) | (InBatchKey.DrawFlags+InBatchKey.ShaderType+InBatchKey.DrawPrimitiveType+InBatchKey.DrawEffects));
		}
	};

	/** A secondary key which represents elements needed to make a batch unique */
	FBatchKey BatchKey;
	/** Shader resource to use with this batch.  Used as a primary key.  No batch can have multiple textures */
	const FSlateShaderResource* ShaderResource;

public:
	/** How far into the vertex buffer is this batch*/
	uint32 VertexOffset;
	/** How far into the index buffer this batch is*/	
	uint32 IndexOffset;
	/** Number of vertices in the batch */
	uint32 NumVertices;
	/** Number of indices in the batch */
	uint32 NumIndices;
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
	FSlateRenderBatch( const FSlateElementBatch& InBatch )
		: ShaderParams( InBatch.GetShaderParams() )
		, Texture( InBatch.GetShaderResource() )
		, CustomDrawer( InBatch.GetCustomDrawer() )
		, DrawFlags( InBatch.GetDrawFlags() )
		, ShaderType( InBatch.GetShaderType() )
		, DrawPrimitiveType( InBatch.GetPrimitiveType() )
		, DrawEffects( InBatch.GetDrawEffects() )
		, ScissorRect( InBatch.GetScissorRect() )
		, VertexOffset( InBatch.VertexOffset )
		, IndexOffset( InBatch.IndexOffset )
		, NumVertices( InBatch.NumVertices )
		, NumIndices( InBatch.NumIndices )
	{}

public:
	const FShaderParams ShaderParams;

	/** Texture to use with this batch.  */
	const FSlateShaderResource* Texture;

	const TWeakPtr<ICustomSlateElement, ESPMode::ThreadSafe> CustomDrawer;

	const ESlateBatchDrawFlag::Type DrawFlags;	

	const ESlateShader::Type ShaderType;

	const ESlateDrawPrimitive::Type DrawPrimitiveType;

	const ESlateDrawEffect::Type DrawEffects;

	const TOptional<FShortRect> ScissorRect;

	/** How far into the vertex buffer is this batch*/
	const uint32 VertexOffset;
	/** How far into the index buffer this batch is*/	
	const uint32 IndexOffset;
	/** Number of vertices in the batch */
	const uint32 NumVertices;
	/** Number of indices in the batch */
	const uint32 NumIndices;
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
	{		
	}
	
	/** @return Get the window that we will be painting */
	const TSharedPtr<SWindow> GetWindow() const
	{
		return TopLevelWindow.Pin();
	}
	
	/** @return Get the window that we will be painting */
	TSharedPtr<SWindow> GetWindow()
	{
		return TopLevelWindow.Pin();
	}
	
	/** @return Get the draw elements that we want to render into this window */
	const TArray<FSlateDrawElement>& GetDrawElements() const
	{
		return DrawElements;
	}
	
	/**
	 * Add a draw element to the list
	 *
	 * @param InDrawElement  The draw element to add
	 */
	void AddItem( const FSlateDrawElement& InDrawElement )
	{
		DrawElements.Add( InDrawElement );
	}

	FSlateDrawElement& AddUninitialized()
	{
		const int32 InsertIdx = DrawElements.AddUninitialized();
		return *(new(DrawElements.GetData() + InsertIdx) FSlateDrawElement());
	}

	/**
	* Some widgets may want to paint their children after after another, loosely-related widget finished painting.
	* Or they may want to paint "after everyone".
	*/
	struct FDeferredPaint
	{
	public:
		SLATECORE_API FDeferredPaint( const TSharedRef<const SWidget>& InWidgetToPaint, const FPaintArgs& InArgs, const FGeometry InAllottedGeometry, const FSlateRect InMyClippingRect, const FWidgetStyle& InWidgetStyle, bool InParentEnabled );

		int32 ExecutePaint( int32 LayerId, FSlateWindowElementList& OutDrawElements ) const;

	private:
		const TWeakPtr<const SWidget> WidgetToPaintPtr;
		const FPaintArgs Args;
		const FGeometry AllottedGeometry;
		const FSlateRect MyClippingRect;
		const FWidgetStyle WidgetStyle;
		const bool bParentEnabled;
	};

	SLATECORE_API void QueueDeferredPainting( const FDeferredPaint& InDeferredPaint );

	int32 PaintDeferred( int32 LayerId );
	
	/**
	 * Remove all the elements from this draw list.
	 */
	void Empty()
	{
		DrawElements.Empty();
		RenderBatches.Empty();
		BatchedVertices.Empty();
		BatchedIndices.Empty();
	}
	
	/**
	 * Returns a list of element batches for this window
	 */
	const TArray<FSlateRenderBatch>& GetRenderBatches() const { return RenderBatches; }

	/**
	 * Returns all the vertices for every draw element in this window
	 */
	const TArray<FSlateVertex>& GetBatchedVertices() const { return BatchedVertices; }

	/**
	 * Returns all the indices for every draw element in this window
	 */
	const TArray<SlateIndex>& GetBatchedIndices() const { return BatchedIndices; }

protected:
	/** The top level window which these elements are being drawn on */
	TWeakPtr<SWindow> TopLevelWindow;
	
	/** List of draw elements for the window */
	TArray<FSlateDrawElement> DrawElements;

	/** List of element batches sorted by later for use in rendering (for threaded renderers, can only be accessed from the render thread)*/
	TArray<FSlateRenderBatch> RenderBatches;

	/** All the vertices for every batch. */
	TArray<FSlateVertex> BatchedVertices;

	/** All the indices for every batch. */
	TArray<SlateIndex> BatchedIndices;

	/**
	 * Some widgets want their logical children to appear at a different "layer" in the physical hierarchy.
	 * We accomplish this by deferring their painting.
	 */
	TArray< TSharedRef<FDeferredPaint> > DeferredPaintList;
};
