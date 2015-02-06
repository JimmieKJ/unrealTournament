// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MatineeModule.h"
#include "MatineeTransaction.h"
#include "MatineeTransBuffer.h"

/*-----------------------------------------------------------------------------
	UMatineeTransBuffer / FMatineeTransaction
-----------------------------------------------------------------------------*/

void UMatineeTransBuffer::BeginSpecial(const FText& Description)
{
	BeginInternal<FMatineeTransaction>(TEXT("Matinee"), Description);
}

void UMatineeTransBuffer::EndSpecial()
{
	UTransBuffer::End();
}
