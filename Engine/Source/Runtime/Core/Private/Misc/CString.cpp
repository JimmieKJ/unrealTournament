// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"

static struct FForceInitAtBootTCStringSpcHelperAnsi : public TForceInitAtBoot<TCStringSpcHelper<ANSICHAR>>
{} FForceInitAtBootTCStringSpcHelperAnsi;

static struct FForceInitAtBootTCStringSpcHelperWide : public TForceInitAtBoot<TCStringSpcHelper<WIDECHAR>>
{} FForceInitAtBootTCStringSpcHelperWide;

bool FToBoolHelper::FromCStringAnsi( const ANSICHAR* String )
{
	return FToBoolHelper::FromCStringWide( ANSI_TO_TCHAR(String) );
}

bool FToBoolHelper::FromCStringWide( const WIDECHAR* String )
{
	if (
			FCStringWide::Stricmp(String, (L"True") )==0 
		||	FCStringWide::Stricmp(String, (L"Yes"))==0
		||	FCStringWide::Stricmp(String, (L"On"))==0
		||	FCStringWide::Stricmp(String, *(GTrue.ToString()))==0
		||	FCStringWide::Stricmp(String, *(GYes.ToString()))==0)
	{
		return true;
	}
	else if(
			FCStringWide::Stricmp(String, (L"False"))==0
		||	FCStringWide::Stricmp(String, (L"No"))==0
		||	FCStringWide::Stricmp(String, (L"Off"))==0
		||	FCStringWide::Stricmp(String, *(GFalse.ToString()))==0
		||	FCStringWide::Stricmp(String, *(GNo.ToString()))==0)
	{
		return false;
	}
	else
	{
		return FCStringWide::Atoi(String) ? true : false;
	}
}
