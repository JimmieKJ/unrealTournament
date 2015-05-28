// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "VisualLogger/VisualLogger.h"
#include "VisualLogger/VisualLogger.h"
#include "VisualLogger/VisualLoggerKismetLibrary.h"

UVisualLoggerKismetLibrary::UVisualLoggerKismetLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UVisualLoggerKismetLibrary::LogText(UObject* WorldContextObject, FString Text, FName CategoryName)
{
#if ENABLE_VISUAL_LOG
	const ELogVerbosity::Type DefaultVerbosity = ELogVerbosity::Log;
	FVisualLogger::CategorizedLogf(WorldContextObject, FLogCategoryBase(*CategoryName.ToString(), DefaultVerbosity, DefaultVerbosity), DefaultVerbosity, INDEX_NONE, *Text);
#endif
}

void UVisualLoggerKismetLibrary::LogLocation(UObject* WorldContextObject, FVector Location, FString Text, FLinearColor Color, float Radius, FName CategoryName)
{
#if ENABLE_VISUAL_LOG
	const ELogVerbosity::Type DefaultVerbosity = ELogVerbosity::Log;
	FVisualLogger::GeometryShapeLogf(WorldContextObject, FLogCategoryBase(*CategoryName.ToString(), DefaultVerbosity, DefaultVerbosity), DefaultVerbosity, INDEX_NONE, Location, Radius, Color, *Text);
#endif
}

void UVisualLoggerKismetLibrary::LogBox(UObject* WorldContextObject, FBox Box, FString Text, FLinearColor ObjectColor, FName CategoryName)
{
#if ENABLE_VISUAL_LOG
	const ELogVerbosity::Type DefaultVerbosity = ELogVerbosity::Log;
	FVisualLogger::GeometryShapeLogf(WorldContextObject, FLogCategoryBase(*CategoryName.ToString(), DefaultVerbosity, DefaultVerbosity), DefaultVerbosity, INDEX_NONE, Box, FMatrix::Identity, ObjectColor, *Text);
#endif
}
