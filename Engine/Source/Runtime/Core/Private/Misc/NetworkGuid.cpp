// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"


/* FNetworkGUID
 *****************************************************************************/

bool FNetworkGUID::NetSerialize( FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess )
{
	Ar << Value;
	return true;
}
