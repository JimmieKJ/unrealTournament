// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


class FLaunchCommand
{
public:

	/**
	 * Executes the command.
	 *
	 * @return true on success, false otherwise.
	 */
	static bool Run( const FString& Params );
};
