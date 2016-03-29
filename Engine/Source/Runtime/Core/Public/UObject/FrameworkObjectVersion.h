// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

// Custom serialization version for changes made in Dev-Framework stream
struct CORE_API FFrameworkObjectVersion
{
	enum Type
	{
		// Before any version changes were made
		BeforeCustomVersionWasAdded = 0,

		// BodySetup's default instance collision profile is used by default when creating a new instance.
		UseBodySetupCollisionProfile,

		// Regenerate subgraph arrays correctly in animation blueprints to remove duplicates and add
		// missing graphs that appear read only when edited
		AnimBlueprintSubgraphFix,

		// Static and skeletal mesh sockets now use the specified scale
		MeshSocketScaleUtilization,

		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	// The GUID for this custom version number
	const static FGuid GUID;

private:
	FFrameworkObjectVersion() {}
};
