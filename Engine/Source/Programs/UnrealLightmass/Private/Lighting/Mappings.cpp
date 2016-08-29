// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "LightmassPCH.h"
#include "Importer.h"


namespace Lightmass
{
/** A static indicating that debug borders should be used around padded mappings. */
bool FStaticLightingMapping::s_bShowLightmapBorders = false;

void FStaticLightingMapping::Import( class FLightmassImporter& Importer )
{
	Importer.ImportData( (FStaticLightingMappingData*) this );
	Mesh = Importer.GetStaticMeshInstances().FindRef(Guid);
}

void FStaticLightingTextureMapping::Import( class FLightmassImporter& Importer )
{
	FStaticLightingMapping::Import( Importer );
	Importer.ImportData( (FStaticLightingTextureMappingData*) this );
	CachedSizeX = SizeX;
	CachedSizeY = SizeY;
	IrradiancePhotonCacheSizeX = 0;
	IrradiancePhotonCacheSizeY = 0;
}

} //namespace Lightmass
