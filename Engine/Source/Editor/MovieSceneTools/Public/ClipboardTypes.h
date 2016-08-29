// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "MovieSceneClipboard.h"

namespace MovieSceneClipboard
{
	template<> inline FName GetKeyTypeName<uint8>()
	{
		static FName Name("Byte");
		return Name;
	}
	template<> inline FName GetKeyTypeName<int32>()
	{
		static FName Name("Int");
		return Name;
	}
	template<> inline FName GetKeyTypeName<FRichCurveKey>()
	{
		static FName Name("RichKey");
		return Name;
	}
	template<> inline FName GetKeyTypeName<FName>()
	{
		static FName Name("Name");
		return Name;
	}
	template<> inline FName GetKeyTypeName<bool>()
	{
		static FName Name("Bool");
		return Name;
	}
	template<> inline FName GetKeyTypeName<FString>()
	{
		static FName Name("String");
		return Name;
	}
}

#endif