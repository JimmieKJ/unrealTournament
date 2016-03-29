// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once


class FSlateElementBatch;
class FSlateShaderResource;
struct FSlateVertex;


/**
 * Abstract base class for Slate rendering policies.
 */
class FSlateRenderingPolicy
{
public:

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InPixelCenterOffset
	 */
	FSlateRenderingPolicy( const TSharedRef<class FSlateFontServices>& InFontServices, float InPixelCenterOffset )
		: FontServices( InFontServices )
		, PixelCenterOffset( InPixelCenterOffset )
	{ }

	/** Virtual destructor. */
	virtual ~FSlateRenderingPolicy( ) { }

	TSharedRef<class FSlateFontCache> GetFontCache() const;
	TSharedRef<class FSlateFontServices> GetFontServices() const;

	virtual TSharedRef<class FSlateShaderResourceManager> GetResourceManager() const = 0;

	virtual bool IsVertexColorInLinearSpace() const = 0;

	float GetPixelCenterOffset() const
	{
		return PixelCenterOffset;
	}

private:

	FSlateRenderingPolicy(const FSlateRenderingPolicy&);
	FSlateRenderingPolicy& operator=(const FSlateRenderingPolicy&);

private:

	TSharedRef<FSlateFontServices> FontServices;
	float PixelCenterOffset;

};
