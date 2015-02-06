// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/inotify.h>

#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define BUF_LEN     ( 1024 * ( EVENT_SIZE + 16 ) )

class FDirectoryWatchRequestLinux
{
public:
	FDirectoryWatchRequestLinux();
	virtual ~FDirectoryWatchRequestLinux();

	/** Sets up the directory handle and request information */
	bool Init(const FString& InDirectory);

	/** Adds a delegate to get fired when the directory changes */
	FDelegateHandle AddDelegate( const IDirectoryWatcher::FDirectoryChanged& InDelegate );
	/** Removes a delegate to get fired when the directory changes */
	DELEGATE_DEPRECATED("This overload of RemoveDelegate is deprecated, instead pass the result of AddDelegate.")
	bool RemoveDelegate( const IDirectoryWatcher::FDirectoryChanged& InDelegate );
	/** Same as above, but for use within other deprecated calls to prevent multiple deprecation warnings */
	bool DEPRECATED_RemoveDelegate( const IDirectoryWatcher::FDirectoryChanged& InDelegate );
	/** Removes a delegate to get fired when the directory changes */
	bool RemoveDelegate( FDelegateHandle InHandle );
	/** Returns true if this request has any delegates listening to directory changes */
	bool HasDelegates() const;
	/** Prepares the request for deletion */
	void EndWatchRequest();
	/** Triggers all pending file change notifications */
	void ProcessPendingNotifications();

private:

	FString Directory;

	bool bRunning;
	bool bEndWatchRequestInvoked;

	int FileDescriptor;
	int * WatchDescriptor;
	int NotifyFilter;

	TArray<FString> AllFiles;
	TArray<IDirectoryWatcher::FDirectoryChanged> Delegates;
	TArray<FFileChangeData> FileChanges;

	void Shutdown();
	void ProcessChanges();
};
