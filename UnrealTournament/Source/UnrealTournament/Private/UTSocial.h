// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "FriendsAndChatStyle.h"

#include "UTSocial.generated.h"

UCLASS(hidecategories = Object, MinimalAPI, BlueprintType)
class USocialAsset : public UDataAsset
{
	GENERATED_BODY()
public:

	UPROPERTY(EditAnywhere, Category = Properties)
		FFriendsAndChatStyle Style;
};