// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AndOr.h"
#include "IsArithmetic.h"
#include "IsPointer.h"

/**
 * Traits class which tests if a type is POD.
 */

#if _MSC_VER == 1900
	// __is_pod changed in VS2015, however the results are still correct for all usages I've been able to locate.
	#pragma warning(push)
	#pragma warning(disable:4647)
#endif // _MSC_VER == 1900

template <typename T>
struct TIsPODType 
{ 
	enum { Value = TOrValue<__is_pod(T) || __is_enum(T), TIsArithmetic<T>, TIsPointer<T>>::Value };
};

#if _MSC_VER == 1900
	#pragma warning(pop)
#endif // _MSC_VER == 1900
