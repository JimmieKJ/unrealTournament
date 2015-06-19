// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "BreakIterator.h"

#if UE_ENABLE_ICU
#include "ICUBreakIterator.h"

TSharedRef<IBreakIterator> FBreakIterator::CreateCharacterBoundaryIterator()
{
	return MakeShareable(new FICUBreakIterator(FICUBreakIteratorManager::Get().CreateCharacterBoundaryIterator()));
}

#endif
