// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

// We disable the deprecation warnings here because otherwise it'll complain about us
// implementing RegisterDirectoryChangedCallback and UnregisterDirectoryChangedCallback.  We know
// that, but we only want it to complain if *others* implement or call these functions.
//
// These macros should be removed when those functions are removed.
PRAGMA_DISABLE_DEPRECATION_WARNINGS

class FDirectoryWatcherWindows : public IDirectoryWatcher
{
public:
	FDirectoryWatcherWindows();
	virtual ~FDirectoryWatcherWindows();

	virtual bool RegisterDirectoryChangedCallback (const FString& Directory, const FDirectoryChanged& InDelegate) override;
	virtual bool UnregisterDirectoryChangedCallback (const FString& Directory, const FDirectoryChanged& InDelegate) override;
	virtual bool RegisterDirectoryChangedCallback_Handle (const FString& Directory, const FDirectoryChanged& InDelegate, FDelegateHandle& OutHandle, bool bIncludeDirectoryEvents) override;
	virtual bool UnregisterDirectoryChangedCallback_Handle (const FString& Directory, FDelegateHandle InHandle) override;
	virtual void Tick (float DeltaSeconds) override;

	/** Map of directory paths to requests */
	TMap<FString, FDirectoryWatchRequestWindows*> RequestMap;
	TArray<FDirectoryWatchRequestWindows*> RequestsPendingDelete;

	/** A count of FDirectoryWatchRequestWindows created to ensure they are cleaned up on shutdown */
	int32 NumRequests;
};

PRAGMA_ENABLE_DEPRECATION_WARNINGS

typedef FDirectoryWatcherWindows FDirectoryWatcher;
