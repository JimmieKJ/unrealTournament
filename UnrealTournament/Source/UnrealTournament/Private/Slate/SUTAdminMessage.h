// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "SUWToast.h"
#include "SlateBasics.h"

#if !UE_SERVER

class UNREALTOURNAMENT_API SUTAdminMessage : public SUWToast
{
private:
	virtual TSharedRef<SWidget> BuildToast(const FArguments& InArgs);
};

#endif