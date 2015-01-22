// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	IOSPlatformString.mm: Additional NSString methods
=============================================================================*/
#include "CorePrivatePCH.h"

@implementation NSString (FString_Extensions)

+ (NSString*) stringWithTCHARString:(const TCHAR*)MyTCHARString
{
	return [NSString stringWithCString:TCHAR_TO_UTF8(MyTCHARString) encoding:NSUTF8StringEncoding];
}

+ (NSString*) stringWithFString:(const FString&)InFString
{
	return [NSString stringWithTCHARString:*InFString];
}


@end
