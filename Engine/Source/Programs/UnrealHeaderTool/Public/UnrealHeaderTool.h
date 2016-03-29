// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#ifndef __UnrealHeaderTool_h__
#define __UnrealHeaderTool_h__

#include "Core.h"
#include "CoreUObject.h"
#include "CompilationResult.h"
#include "UHTMakefile/MakefileHelpers.h"

DECLARE_LOG_CATEGORY_EXTERN(LogCompile, Log, All);

extern bool GUHTWarningLogged;

#define UE_LOG_WARNING_UHT(Format, ...) { GUHTWarningLogged = true; UE_LOG(LogCompile, Warning, Format, ##__VA_ARGS__); }

#endif		// __UnrealHeaderTool_h__