// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "../Base/SUTToastBase.h"
#include "SlateBasics.h"

#if !UE_SERVER

class UNREALTOURNAMENT_API SUTAdminMessageToast : public SUTToastBase
{
private:
	virtual TSharedRef<SWidget> BuildToast(const FArguments& InArgs);
};

#endif