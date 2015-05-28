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
	DELEGATE_DEPRECATED("The RegisterDirectoryChangedCallback function is deprecated - please use the RegisterDirectoryChangedCallback_Handle function instead which returns a handle.") \
	virtual bool RegisterDirectoryChangedCallback (const FString& Directory, const FDirectoryChanged& InDelegate) = 0;

	/**
	 * Unregisters a callback to fire when directories are changed
	 *
	 * @param	Directory to stop watching
	 * @param	Delegate to remove from our callback list
	 */
	DELEGATE_DEPRECATED("The UnregisterDirectoryChangedCallback function is deprecated - please use the RegisterDirectoryChangedCallback_Handle function and pass its out parameter to UnregisterDirectoryChangedCallback_Handle instead.") \
	virtual bool UnregisterDirectoryChangedCallback (const FString& Directory, const FDirectoryChanged& InDelegate) = 0;

	/**
	 * Register a callback to fire when directories are changed
	 *
	 * @param	Directory to watch
	 * @param	Delegate to add to our callback list
	 * @param	The handle to the registered delegate, if the registration was successful.
	 * @param   Whether to include notifications for changes to actual directories (such as directories being created or removed)
	 */
	virtual bool RegisterDirectoryChangedCallback_Handle (const FString& Directory, const FDirectoryChanged& InDelegate, FDelegateHandle& OutHandle, bool bIncludeDirectoryChanges = false) = 0;

	/**
	 * Unregisters a callback to fire when directories are changed
	 *
	 * @param	Directory to stop watching
	 * @param	Handle to the delegate to remove from our callback list
	 */
	virtual bool UnregisterDirectoryChangedCallback_Handle (const FString& Directory, FDelegateHandle InHandle) = 0;

	/**
	 * Allows for subclasses to be ticked (by editor or other programs that need to tick the singleton)
	 */
	virtual void Tick(float DeltaSeconds)
	{
	}
};

