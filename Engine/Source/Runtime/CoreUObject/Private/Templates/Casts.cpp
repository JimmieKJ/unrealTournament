// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CoreUObjectPrivate.h"
#include "Casts.h"

DECLARE_LOG_CATEGORY_EXTERN(LogCasts, Log, All);
DEFINE_LOG_CATEGORY(LogCasts);

void CastLogError(const TCHAR* FromType, const TCHAR* ToType)
{
	UE_LOG(LogCasts, Fatal, TEXT("Cast of %s to %s failed"), FromType, ToType);
}
