// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

// Custom serialization version for changes made in Dev-Core stream
struct CORE_API FReleaseObjectVersion
{
	enum Type
	{
		// Before any version changes were made
		BeforeCustomVersionWasAdded = 0,

		// Static Mesh extended bounds radius fix
		StaticMeshExtendedBoundsFix,

		//Physics asset bodies are either in the sync scene or the async scene, but not both
		NoSyncAsyncPhysAsset,

		// ULevel was using TTransArray incorrectly (serializing the entire array in addition to individual mutations).
		// converted to a TArray:
		LevelTransArrayConvertedToTArray,

		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	// The GUID for this custom version number
	const static FGuid GUID;

private:
	FReleaseObjectVersion() {}
};
