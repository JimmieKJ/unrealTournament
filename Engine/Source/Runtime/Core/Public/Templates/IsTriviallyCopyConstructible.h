// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AndOr.h"
#include "IsPODType.h"

/**
 * Traits class which tests if a type has a trivial copy constructor.
 */
template <typename T>
struct TIsTriviallyCopyConstructible
{
	enum { Value = TOrValue<__has_trivial_copy(T), TIsPODType<T>>::Value };
};
