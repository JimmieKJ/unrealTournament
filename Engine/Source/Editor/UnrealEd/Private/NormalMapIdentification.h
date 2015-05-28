// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

namespace NormalMapIdentification
{
	/**
	 * Handle callback when an asset is imported.
	 * @param	InFactory	The texture factory being used.
	 * @param	InTexture	The texture that was imported.
	 */
	void HandleAssetPostImport( UTextureFactory* InFactory, UTexture* InTexture );
}
