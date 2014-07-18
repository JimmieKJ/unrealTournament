// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTHUD_CTF.generated.h"

UCLASS()
class AUTHUD_CTF : public AUTHUD
{
	GENERATED_UCLASS_BODY()

	virtual FLinearColor GetBaseHUDColor();
	virtual void PostRender();
};