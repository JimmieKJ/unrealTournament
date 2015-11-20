// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UnrealString.h"

/**
 * Simplified parsing information about UClasses.
 */
class FSimplifiedParsingClassInfo
{
public:
	// Constructor.
	FSimplifiedParsingClassInfo(FString InClassName, FString InBaseClassName, int32 InClassDefLine, bool bInClassIsAnInterface)
		: ClassName          (MoveTemp(InClassName))
		, BaseClassName      (MoveTemp(InBaseClassName))
		, ClassDefLine       (InClassDefLine)
		, bClassIsAnInterface(bInClassIsAnInterface)
	{}

	/**
	 * Gets class name.
	 */
	const FString& GetClassName() const
	{
		return ClassName;
	}

	/**
	 * Gets base class name.
	 */
	const FString& GetBaseClassName() const
	{
		return BaseClassName;
	}

	/**
	 * Gets class definition line number.
	 */
	int32 GetClassDefLine() const
	{
		return ClassDefLine;
	}

	/**
	 * Tells if this class is an interface.
	 */
	bool IsInterface() const
	{
		return bClassIsAnInterface;
	}

private:
	// Name.
	FString ClassName;

	// Super-class name.
	FString BaseClassName;

	// Class definition line.
	int32 ClassDefLine;

	// Is this an interface class?
	bool bClassIsAnInterface;
};