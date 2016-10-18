// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MediaAssetsPCH.h"
#include "MediaPlaylist.h"


/* UMediaPlaylist interface
 *****************************************************************************/

UMediaSource* UMediaPlaylist::Get(int32 Index)
{
	if (!Items.IsValidIndex(Index))
	{
		return nullptr;
	}

	return Items[Index];
}


UMediaSource* UMediaPlaylist::GetNext(int32& InOutIndex)
{
	if (Items.Num() == 0)
	{
		InOutIndex = INDEX_NONE;

		return nullptr;
	}

	InOutIndex = (InOutIndex != INDEX_NONE) ? (InOutIndex + 1) % Items.Num() : 0;

	return Items[InOutIndex];
}


UMediaSource* UMediaPlaylist::GetPrevious(int32& InOutIndex)
{
	if (Items.Num() == 0)
	{
		InOutIndex = INDEX_NONE;

		return nullptr;
	}

	InOutIndex = (InOutIndex != INDEX_NONE) ? (InOutIndex + Items.Num() - 1) % Items.Num() : 0;

	return Items[InOutIndex];
}


UMediaSource* UMediaPlaylist::GetRandom(int32& InOutIndex)
{
	if (Items.Num() == 0)
	{
		InOutIndex = INDEX_NONE;

		return nullptr;
	}

	InOutIndex = FMath::RandHelper(Items.Num() - 1);

	return Items[InOutIndex];
}
