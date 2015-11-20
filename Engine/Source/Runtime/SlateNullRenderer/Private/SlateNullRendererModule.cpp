// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlateNullRendererPrivatePCH.h"

/** A null font texture resource to represent fonts */
class FSlateFontTextureNull : public FSlateShaderResource
#if WITH_ENGINE
	, public FTextureResource
#endif
{
public:
	FSlateFontTextureNull() {}

	/** FSlateShaderResource interface */
	virtual uint32 GetWidth() const override { return 0; }
	virtual uint32 GetHeight() const override { return 0; }
	virtual ESlateShaderResource::Type GetType() const override
	{
		return ESlateShaderResource::NativeTexture;
	}

#if WITH_ENGINE
	/** FTextureResource interface */
	virtual uint32 GetSizeX() const override { return 0; }
	virtual uint32 GetSizeY() const override { return 0; }
	virtual FString GetFriendlyName() const override { return TEXT("FSlateFontTextureNull"); }
#endif
};

/** A null font atlas store null font textures */
class FSlateFontAtlasNull : public FSlateFontAtlas
{
public:
	FSlateFontAtlasNull(float AtlasSize)
		: FSlateFontAtlas(AtlasSize, AtlasSize)
	{}

	virtual class FSlateShaderResource* GetSlateTexture() override { return &NullFontTexture; }
	virtual class FTextureResource* GetEngineTexture() override
	{
#if WITH_ENGINE
		return &NullFontTexture;
#else
		return nullptr;
#endif
	}
	virtual void ConditionalUpdateTexture()  override {}
	virtual void ReleaseResources() override {}

	static FSlateFontTextureNull NullFontTexture;
};
FSlateFontTextureNull FSlateFontAtlasNull::NullFontTexture;

/** A null font atlas factory to generate a null font atlas */
class FSlateNullFontAtlasFactory : public ISlateFontAtlasFactory
{
public:
	FSlateNullFontAtlasFactory()
		: AtlasSize(2048)
	{}

	virtual ~FSlateNullFontAtlasFactory() {}

	virtual FIntPoint GetAtlasSize() const override
	{
		return FIntPoint(AtlasSize, AtlasSize);
	}

	virtual TSharedRef<FSlateFontAtlas> CreateFontAtlas() const override
	{
		return MakeShareable(new FSlateFontAtlasNull(AtlasSize));
	}

private:
	/** Size of each font texture, width and height. Only used to return sane numbers */
	int32 AtlasSize;
};

/**
 * Implements the Slate Null Renderer module.
 */
class FSlateNullRendererModule
	: public ISlateNullRendererModule
{
public:

	// ISlateNullRendererModule interface
	virtual TSharedRef<FSlateRenderer> CreateSlateNullRenderer( ) override
	{
		ConditionalCreateResources();

		return MakeShareable( new FSlateNullRenderer(FontCache, FontMeasure) );
	}

	virtual TSharedRef<ISlateFontAtlasFactory> CreateSlateFontAtlasFactory() override
	{
		return MakeShareable( new FSlateNullFontAtlasFactory );
	}

	virtual void StartupModule( ) override { }
	virtual void ShutdownModule( ) override { }

private:
	void ConditionalCreateResources()
	{
		if (!FontCache.IsValid())
		{
			FontCache = MakeShareable(new FSlateFontCache(MakeShareable(new FSlateNullFontAtlasFactory)));
		}

		if (!FontMeasure.IsValid())
		{
			FontMeasure = FSlateFontMeasure::Create(FontCache.ToSharedRef());
		}
	}

private:
	TSharedPtr<FSlateFontCache> FontCache;
	TSharedPtr<FSlateFontMeasure> FontMeasure;
};


IMPLEMENT_MODULE( FSlateNullRendererModule, SlateNullRenderer ) 
