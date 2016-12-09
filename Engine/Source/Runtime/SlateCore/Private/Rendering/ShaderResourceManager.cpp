// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Rendering/ShaderResourceManager.h"

FSlateResourceHandle FSlateShaderResourceManager::GetResourceHandle( const FSlateBrush& InBrush )
{
	FSlateShaderResourceProxy* Proxy = GetShaderResource( InBrush );

	FSlateResourceHandle NewHandle;
	if( Proxy )
	{
		if( !Proxy->HandleData.IsValid() )
		{
			Proxy->HandleData = MakeShareable( new FSlateSharedHandleData( Proxy ) );
		}

		NewHandle.Data = Proxy->HandleData;
	}

	return NewHandle;
}
