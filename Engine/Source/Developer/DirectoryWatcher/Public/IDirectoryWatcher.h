// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

struct FFileChangeData
{
	enum EFileChangeAction
	{
		FCA_Unknown,
		FCA_Added,
		FCA_Modified,
		FCA_Removed
	};

	FFileChangeData(const FString& InFilename, EFileChangeAction InAction)
		: Filename(InFilename)
		, Action(InAction)
	{
		FPaths::MakeStandardFilename(Filename);
	}

	FString Filename;
	EFileChangeAction Action;
};


/** The public interface for the directory watcher singleton. */
class IDirectoryWatcher
{
public:
	/** A delegate to report directory changes */
	DECLARE_DELEGATE_OneParam(FDirectoryChanged, const TArray<struct FFileChangeData>& /*FileChanges*/);

	/** Virtual destructor */
	virtual ~IDirectoryWatcher() {};

	/**
	 * Register a callback to fire when directories are changed
	 *
	 * @param	Directory to watch
	 * @param	Delegate to add to our callback list
	 */
	virtual bool RegisterDirectoryChangedCallback (const FString& Directory, const FDirectoryChanged& InDelegate) = 0;

	/**
	 * Unregisters a callback to fire when directories are changed
	 *
	 * @param	Directory to stop watching
	 * @param	Delegate to remove from our callback list
	 */
	virtual bool UnregisterDirectoryChangedCallback (const FString& Directory, const FDirectoryChanged& InDelegate) = 0;

	/**
	 * Allows for subclasses to be ticked (by editor or other programs that need to tick the singleton)
	 */
	virtual void Tick(float DeltaSeconds)
	{
	}
};

