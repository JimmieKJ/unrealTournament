// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlateCorePrivatePCH.h"


/* FSlateRenderer interface
 *****************************************************************************/

void FSlateRenderer::FlushFontCache( )
{
	FontCache->FlushCache();
	FontMeasure->FlushCache();
}


bool FSlateRenderer::IsViewportFullscreen( const SWindow& Window ) const
{
	check( IsThreadSafeForSlateRendering() );

	bool bFullscreen = false;

	if (FPlatformProperties::SupportsWindowedMode())
	{
		if( GIsEditor)
		{
			bFullscreen = false;
		}
		else
		{
			bFullscreen = Window.GetWindowMode() == EWindowMode::Fullscreen;
		}
	}
	else
	{
		bFullscreen = true;
	}

	return bFullscreen;
}


ISlateAtlasProvider* FSlateRenderer::GetTextureAtlasProvider()
{
	return nullptr;
}


ISlateAtlasProvider* FSlateRenderer::GetFontAtlasProvider()
{
	if( FontCache.IsValid() )
	{
		return FontCache.Get();
	}

	return nullptr;
}


/* Global functions
 *****************************************************************************/

bool IsThreadSafeForSlateRendering( )
{
	return ((GSlateLoadingThreadId != 0) || IsInGameThread());
}
