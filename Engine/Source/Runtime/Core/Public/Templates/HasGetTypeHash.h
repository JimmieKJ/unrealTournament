// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

namespace UE4GetTypeHashExists_Private
{
	struct FNotSpecified {};

	template <typename T>
	struct FReturnValueCheck
	{
		static char (&Func())[2];
	};

	template <>
	struct FReturnValueCheck<FNotSpecified>
	{
		static char (&Func())[1];
	};

	template <typename T>
	FNotSpecified GetTypeHash(const T&);

	template <typename T>
	const T& Make();

	template <typename T>
	struct GetTypeHashQuery
	{
		enum { Value = sizeof(FReturnValueCheck<decltype(GetTypeHash(Make<T>()))>::Func()) == sizeof(char[2]) };
	};
}

/**
* Traits class which tests if a type has a GetTypeHash overload.
*/
template <typename T>
struct THasGetTypeHash
{
	enum { Value = UE4GetTypeHashExists_Private::GetTypeHashQuery<T>::Value };
};
