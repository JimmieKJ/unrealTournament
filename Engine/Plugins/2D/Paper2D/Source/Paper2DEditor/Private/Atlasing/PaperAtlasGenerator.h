// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

//////////////////////////////////////////////////////////////////////////
// FPaperAtlasGenerator

class UPaperSpriteAtlas;

struct FPaperAtlasGenerator
{
public:
	static void HandleAssetChangedEvent(UPaperSpriteAtlas* Atlas);
};
