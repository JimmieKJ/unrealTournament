// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphRuntimePrivatePCH.h"
#include "AnimationCustomVersion.h"

const FGuid FAnimationCustomVersion::GUID(0x2EB5FDBD, 0x01AC4D10, 0x8136F38F, 0x3393A5DA);

// Register the custom version with core
FCustomVersionRegistration GRegisterAnimationCustomVersion(FAnimationCustomVersion::GUID, FAnimationCustomVersion::LatestVersion, TEXT("AnimGraphVer"));
