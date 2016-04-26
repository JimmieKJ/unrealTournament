// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Serialization/CustomVersion.h"
#include "BlueprintsObjectVersion.h"
#include "BuildObjectVersion.h"
#include "CoreObjectVersion.h"
#include "EditorObjectVersion.h"
#include "FrameworkObjectVersion.h"
#include "MobileObjectVersion.h"
#include "NetworkingObjectVersion.h"
#include "OnlineObjectVersion.h"
#include "PhysicsObjectVersion.h"
#include "PlatformObjectVersion.h"
#include "RenderingObjectVersion.h"
#include "SequencerObjectVersion.h"
#include "VRObjectVersion.h"

class CORE_API FDevVersionRegistration :  public FCustomVersionRegistration
{
public:
	FDevVersionRegistration(FGuid InKey, int32 Version, FName InFriendlyName);

	/** Dumps all registered versions to log */
	static void DumpVersionsToLog();
};