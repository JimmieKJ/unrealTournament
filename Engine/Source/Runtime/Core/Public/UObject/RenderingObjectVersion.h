// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

// Custom serialization version for changes made in Dev-Rendering stream
struct CORE_API FRenderingObjectVersion
{
	enum Type
	{
		// Before any version changes were made
		BeforeCustomVersionWasAdded = 0,

		// Added support for 3 band SH in the ILC
		IndirectLightingCache3BandSupport,

		// Allows specifying resolution for reflection capture probes
		CustomReflectionCaptureResolutionSupport,

		RemovedTextureStreamingLevelData,

		// translucency is now a property which matters for materials with the decal domain
		IntroducedMeshDecals,

		// Reflection captures are no longer prenormalized
		ReflectionCapturesStoreAverageBrightness,

		ChangedPlanarReflectionFadeDefaults,

		RemovedRenderTargetSize,

		// Particle Cutout (SubUVAnimation) data is now stored in the ParticleRequired Module
		MovedParticleCutoutsToRequiredModule,

		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	// The GUID for this custom version number
	const static FGuid GUID;

private:
	FRenderingObjectVersion() {}
};
