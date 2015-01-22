// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

namespace NormalMapIdentification
{
	/**
	 * Handle callback when an asset is imported.
	 * @param	InFactory	The factory being used.
	 * @param	InObject	The object that was imported.
	 */
	void HandleAssetPostImport( UFactory* InFactory, UObject* InObject );
}
