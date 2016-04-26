// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SlateCorePrivatePCH.h"

/* FSlateRenderingPolicy interface
 *****************************************************************************/

TSharedRef<class FSlateFontCache> FSlateRenderingPolicy::GetFontCache() const
{
	return FontServices->GetFontCache();
}

TSharedRef<class FSlateFontServices> FSlateRenderingPolicy::GetFontServices() const
{
	return FontServices;
}
