// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

/** Tests whether two typenames refer to the same type. */
template<typename A,typename B>
struct TAreTypesEqual;

template<typename,typename>
struct TAreTypesEqual
{
	enum { Value = false };
};

template<typename A>
struct TAreTypesEqual<A,A>
{
	enum { Value = true };
};

#define ARE_TYPES_EQUAL(A,B) TAreTypesEqual<A,B>::Value
