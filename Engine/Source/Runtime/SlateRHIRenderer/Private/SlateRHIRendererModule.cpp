// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlateRHIRendererPrivatePCH.h"
#include "Slate3DRenderer.h"

class FSlateRHIFontAtlasFactory : public ISlateFontAtlasFactory
{
public:
	FSlateRHIFontAtlasFactory()
	{
		if (GIsEditor)
		{
			AtlasSize = 2048;
		}
		else
		{
			AtlasSize = 1024;
			if (GConfig)
			{
				GConfig->GetInt(TEXT("SlateRenderer"), TEXT("FontAtlasSize"), AtlasSize, GEngineIni);
				AtlasSize = FMath::Clamp(AtlasSize, 0, 2048);
			}
		}
	}

	virtual ~FSlateRHIFontAtlasFactory()
	{
	}

	virtual FIntPoint GetAtlasSize() const override
	{
		return FIntPoint(AtlasSize, AtlasSize);
	}

	virtual TSharedRef<FSlateFontAtlas> CreateFontAtlas() const override
	{
		return MakeShareable(new FSlateFontAtlasRHI(AtlasSize, AtlasSize));
	}

private:
	/** Size of each font texture, width and height */
	int32 AtlasSize;
};


/**
 * Implements the Slate RHI Renderer module.
 */
class FSlateRHIRendererModule
	: public ISlateRHIRendererModule
{
public:

	// ISlateRHIRendererModule interface
	virtual TSharedRef<FSlateRenderer> CreateSlateRHIRenderer( ) override
	{
		ConditionalCreateResources();

		return MakeShareable( new FSlateRHIRenderer( ResourceManager, FontCache, FontMeasure ) );
	}

	virtual TSharedRef<ISlate3DRenderer> CreateSlate3DRenderer() override
	{
		ConditionalCreateResources();

		return MakeShareable( new FSlate3DRenderer( ResourceManager, FontCache ) );
	}

	virtual TSharedRef<ISlateFontAtlasFactory> CreateSlateFontAtlasFactory() override
	{
		return MakeShareable(new FSlateRHIFontAtlasFactory);
	}

	virtual void StartupModule( ) override { }
	virtual void ShutdownModule( ) override { }

private:
	/** Creates resource managers if they do not exist */
	void ConditionalCreateResources()
	{
		if( !ResourceManager.IsValid() )
		{
			ResourceManager = MakeShareable( new FSlateRHIResourceManager );
		}

		if( !FontCache.IsValid() )
		{
			FontCache = MakeShareable(new FSlateFontCache(MakeShareable(new FSlateRHIFontAtlasFactory)));
		}

		if( !FontMeasure.IsValid() )
		{
			FontMeasure = FSlateFontMeasure::Create(FontCache.ToSharedRef());
		}

	}
private:
	/** Resource manager used for all renderers */
	TSharedPtr<FSlateRHIResourceManager> ResourceManager;

	/** Font cache used for all renderers */
	TSharedPtr<FSlateFontCache> FontCache;

	/** Font measure interface used for all renderers */
	TSharedPtr<FSlateFontMeasure> FontMeasure;
};


IMPLEMENT_MODULE( FSlateRHIRendererModule, SlateRHIRenderer ) 
