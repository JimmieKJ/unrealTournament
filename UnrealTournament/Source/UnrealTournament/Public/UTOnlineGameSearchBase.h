// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

class FUTOnlineGameSearchBase : public FOnlineSessionSearch
{
public:
	FUTOnlineGameSearchBase(bool bSearchForLANGames);
	virtual ~FUTOnlineGameSearchBase(){}
};