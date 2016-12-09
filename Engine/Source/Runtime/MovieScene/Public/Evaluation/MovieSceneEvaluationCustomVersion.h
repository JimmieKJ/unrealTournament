// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"

// Custom version for movie scene evaluation serialization
namespace FMovieSceneEvaluationCustomVersion
{
	enum Type
	{
		Initial = 0,

		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	// The GUID for this custom version
	const static FGuid GUID = FGuid(0xA462B7EA, 0xF4994E3A, 0x99C1EC1F, 0x8224E1B2);
}
