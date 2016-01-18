// this is included in the definitions of the various UEngine subclasses as we want it in all of them but don't want to have to potentially change several places
// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#ifndef UT_LOADING_SCREEN_HOOK
#define UT_LOADING_SCREEN_HOOK
#endif

#define UT_LOADMAP_DEFINITION() \
virtual bool LoadMap(FWorldContext& WorldContext, FURL URL, class UPendingNetGame* Pending, FString& Error) \
{ \
	UT_LOADING_SCREEN_HOOK \
	bool bResult = Super::LoadMap(WorldContext, URL, Pending, Error); \
	if (bResult && WorldContext.World() != NULL) \
	{ \
		/* always call BeginPlay() at startup for consistent behavior across server and client */ \
		WorldContext.World()->GetWorldSettings()->NotifyBeginPlay(); \
	} \
	return bResult; \
}