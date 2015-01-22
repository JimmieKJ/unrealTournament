// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

namespace ECollectionShareType
{
	enum Type
	{
		CST_System,
		CST_Local,
		CST_Private,
		CST_Shared,

		CST_All
	};
}

/** A name/type pair to uniquely identify a collection */
struct FCollectionNameType
{
	FCollectionNameType(FName InName, ECollectionShareType::Type InType)
		: Name(InName)
		, Type(InType)
	{}

	bool operator==(const FCollectionNameType& Other) const
	{
		return Name == Other.Name && Type == Other.Type;
	}

	FName Name;
	ECollectionShareType::Type Type;
};