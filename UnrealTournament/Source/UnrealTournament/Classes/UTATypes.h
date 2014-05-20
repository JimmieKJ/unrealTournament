// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTATypes.generated.h"

UENUM()
namespace EGameStage
{
	enum Type
	{
		/** Playing montage in usual way. */
		Initializing,
		PreGame, 
		GameInProgress,
		GameOver,
		MAX,
	};
}

UENUM()
namespace ETextHorzPos
{
	enum Type
	{
		/** Playing montage in usual way. */
		Left,
		Center, 
		Right,
		MAX,
	};
}
