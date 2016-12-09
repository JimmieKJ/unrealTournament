// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MatineeTransBuffer.h"
#include "MatineeTransaction.h"

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
