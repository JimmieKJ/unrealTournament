// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FTestClassWithStaticField
{
public:
	static int StaticField;
};

int FTestClassWithStaticField::StaticField = 5;
