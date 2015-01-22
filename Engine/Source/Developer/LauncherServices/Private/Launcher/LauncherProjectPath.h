// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Implements the launcher project path utilities.
 */
class FLauncherProjectPath
{
public:

	/**
	 * Gets the name of the Unreal project to use.
	 */
	static FString GetProjectName(const FString& ProjectPath);

	/**
	 * Gets the base project path for the project (e.g. Samples/Showcases/MyShowcase)
	 */
	static FString GetProjectBasePath(const FString& ProjectPath);

};
