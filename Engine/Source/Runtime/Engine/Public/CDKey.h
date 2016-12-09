// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	CDKey.h: CD Key validation
=============================================================================*/

#pragma once

#include "Containers/UnrealString.h"

/*-----------------------------------------------------------------------------
	Global CD Key functions
-----------------------------------------------------------------------------*/

FString GetCDKeyHash();
FString GetCDKeyResponse( const TCHAR* Challenge );

