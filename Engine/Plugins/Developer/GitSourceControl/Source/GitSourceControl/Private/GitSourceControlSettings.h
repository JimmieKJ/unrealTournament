// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FGitSourceControlSettings
{
public:
	/** Get the Git Binary Path */
	const FString& GetBinaryPath() const;

	/** Set the Git Binary Path */
	void SetBinaryPath(const FString& InString);

	/** Load settings from ini file */
	void LoadSettings();

	/** Save settings to ini file */
	void SaveSettings() const;

private:
	/** A critical section for settings access */
	mutable FCriticalSection CriticalSection;

	/** Git binary path */
	FString BinaryPath;
};