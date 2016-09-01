// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_PROFILE
#include "Social.h"
#else
//Fake class sitting in GithubStubs
#include "SocialStyle.h"
#endif

#include "UTSocialStyle.generated.h"

UCLASS(hidecategories = Object, MinimalAPI, BlueprintType)
class USocialStyleAsset : public UDataAsset
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, Category = Properties)
	FSocialStyle Style;
};