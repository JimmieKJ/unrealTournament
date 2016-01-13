// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

class UNREALTOURNAMENT_API FUTOnlineGameSearchBase : public FOnlineSessionSearch
{
public:
	FUTOnlineGameSearchBase(bool bSearchForLANGames);
	virtual ~FUTOnlineGameSearchBase(){}
};