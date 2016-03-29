// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once


class FSlateShaderResource;

/**
 * A class which batches Slate elements for rendering
 */
class SLATECORE_API FSlateElementBatcher
{
public:

	FSlateElementBatcher( TSharedRef<FSlateRenderingPolicy> InRenderingPolicy );
	~FSlateElementBatcher();

	/** 
	 * Batches elements to be rendered 
	 * 
	 * @param DrawElements	The elements to batch
	 */
	void AddElements( FSlateWindowElementList& ElementList );

	/**
	 * Returns true if the elements in this batcher require v-sync.
	 */
	bool RequiresVsync() const { return bRequiresVsync; }

	/** 
	 * Resets all stored data accumulated during the batching process
	 */
	void ResetBatches();

private:
	void AddElements(FSlateDrawLayer& InDrawLayer);
	
	FColor PackVertexColor(const FLinearColor& InLinearColor);

	/** 
	 * Creates vertices necessary to draw a Quad element 
	 */
	void AddQuadElement( const FSlateDrawElement& DrawElement, FColor Color = FColor::White);

	/** 
	 * Creates vertices necessary to draw a 3x3 element
	 */
	void AddBoxElement( const FSlateDrawElement& DrawElement );

	/** 
	 * Creates vertices necessary to draw a string (one quad per character)
	 */
	void AddTextElement( const FSlateDrawElement& DrawElement );

	/** 
	 * Creates vertices necessary to draw a shaped glyph sequence (one quad per glyph)
	 */
	void AddShapedTextElement( const FSlateDrawElement& DrawElement );

	/** 
	 * Creates vertices necessary to draw a gradient box (horizontal or vertical)
	 */
	void AddGradientElement( const FSlateDrawElement& DrawElement );

	/** 
	 * Creates vertices necessary to draw a spline (Bezier curve)
	 */
	void AddSplineElement( const FSlateDrawElement& DrawElement );

	/** 
	 * Creates vertices necessary to draw a series of attached line segments
	 */
	void AddLineElement( const FSlateDrawElement& DrawElement );
	
	/** 
	 * Creates vertices necessary to draw a viewport (just a textured quad)
	 */
	void AddViewportElement( const FSlateDrawElement& DrawElement );

	/** 
	 * Creates vertices necessary to draw a border element
	 */
	void AddBorderElement( const FSlateDrawElement& DrawElement );

	/**
	 * Batches a custom slate drawing element
	 *
	 * @param Position		The top left screen space position of the element
	 * @param Size			The size of the element
	 * @param Scale			The amount to scale the element by
	 * @param InPayload		The data payload for this element
	 * @param DrawEffects	DrawEffects to apply
	 * @param Layer			The layer to draw this element in
	 */
	void AddCustomElement( const FSlateDrawElement& DrawElement );

	void AddCustomVerts( const FSlateDrawElement& DrawElement );

	void AddCachedBuffer( const FSlateDrawElement& DrawElement );

	void AddLayer(const FSlateDrawElement& DrawElement);

	/** 
	 * Finds an batch for an element based on the passed in parameters
	 * Elements with common parameters and layers will batched together.
	 *
	 * @param Layer			The layer where this element should be drawn (signifies draw order)
	 * @param ShaderParams	The shader params for this element
	 * @param InTexture		The texture to use in the batch
	 * @param PrimitiveType	The primitive type( triangles, lines ) to use when drawing the batch
	 * @param ShaderType	The shader to use when rendering this batch
	 * @param DrawFlags		Any optional draw flags for this batch
	 */	
	FSlateElementBatch& FindBatchForElement( uint32 Layer, 
											 const FShaderParams& ShaderParams, 
											 const FSlateShaderResource* InTexture, 
											 ESlateDrawPrimitive::Type PrimitiveType, 
											 ESlateShader::Type ShaderType, 
											 ESlateDrawEffect::Type DrawEffects, 
											 ESlateBatchDrawFlag::Type DrawFlags,
											 const TOptional<FShortRect>& ScissorRect);
private:
	/** Batch data currently being filled in */
	FSlateBatchData* BatchData;

	/** The draw layer currently being accumulated */
	FSlateDrawLayer* DrawLayer;

	/** Rendering policy we were created from */
	FSlateRenderingPolicy* RenderingPolicy;

	/** Track the number of drawn batches from the previous frame to report to stats. */
	int32 NumDrawnBatchesStat;

	/** Track the number of drawn boxes from the previous frame to report to stats. */
	int32 NumDrawnBoxesStat;

	/** Track the number of drawn texts from the previous frame to report to stats. */
	int32 NumDrawnTextsStat;

	/** Offset to use when supporting 1:1 texture to pixel snapping */
	const float PixelCenterOffset;

	/** Are the vertex colors expected to be in sRGB space? */
	const bool bSRGBVertexColor;

	// true if any element in the batch requires vsync.
	bool bRequiresVsync;
};
