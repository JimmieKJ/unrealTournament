// Copyright 2015 Allegorithmic All Rights Reserved.

#pragma once

// Custom serialization version for all packages containing Paper2D asset types
struct FSubstanceCoreCustomVersion
{
	enum Type
	{
		// Before any version changes were made in the plugin
		BeforeCustomVersionWasAdded = 0,

		// Fixed freezing (non dynamic graphs) system for Substance Graph Instance
		FixedGraphFreeze = 1,

		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	// The GUID for this custom version number
	const static FGuid GUID;

private:
	FSubstanceCoreCustomVersion() {}
};
