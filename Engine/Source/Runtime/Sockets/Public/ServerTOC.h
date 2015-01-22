// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Helper struct to hold and quickly access server TOC.
 */
struct SOCKETS_API FServerTOC
{
	/** List of files in a directory. */
	typedef TMap<FString, FDateTime> FDirectory;

	/** This is the "TOC" of the server */
	TMap<FString, FDirectory*> Directories;

	/** Destructor. Destroys directories. */
	~FServerTOC();

	/**
	 * Adds a file or directory to TOC.
	 *
	 * @param Filename File name or directory name to add.
	 * @param Timestamp File timestamp. Directories should have this set to 0.
	 */
	void AddFileOrDirectory(const FString& Filename, const FDateTime& Timestamp);

	/**
	 * Finds a file in TOC.
	 *
	 * @param Filename File name to find.
	 * @return Pointer to a timestamp if the file was found, NULL otherwise.
	 */
	FDateTime* FindFile(const FString& Filename);

	/**
	 * Finds a directory in TOC.
	 *
	 * @param Directory Directory to find.
	 * @return Pointer to a timestamp if the directory was found, NULL otherwise.
	 */
	FDirectory* FindDirectory(const FString& Directory);
};
