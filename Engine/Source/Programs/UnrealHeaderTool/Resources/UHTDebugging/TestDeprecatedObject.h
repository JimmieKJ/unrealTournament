// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TestDeprecatedObject.generated.h"

namespace EFoo
{
	enum Bar
	{
		Example,
	};
}

UCLASS(deprecated)
class UDEPRECATED_TestDeprecatedObject : public UObject
{
	GENERATED_BODY()
public:
	UDEPRECATED_TestDeprecatedObject(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
