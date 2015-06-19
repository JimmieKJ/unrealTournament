// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#ifndef _INC_DIRECTORYWATCHERMAC
#define _INC_DIRECTORYWATCHERMAC

class FDirectoryWatcherMac : public IDirectoryWatcher
{
public:
	FDirectoryWatcherMac();
	virtual ~FDirectoryWatcherMac();

	virtual bool RegisterDirectoryChangedCallback (const FString& Directory, const FDirectoryChanged& InDelegate) override;
	virtual bool UnregisterDirectoryChangedCallback (const FString& Directory, const FDirectoryChanged& InDelegate) override;
	virtual bool RegisterDirectoryChangedCallback_Handle (const FString& Directory, const FDirectoryChanged& InDelegate, FDelegateHandle& OutHandle, bool bIncludeDirectoryChanges) override;
	virtual bool UnregisterDirectoryChangedCallback_Handle (const FString& Directory, FDelegateHandle InHandle) override;
	virtual void Tick (float DeltaSeconds) override;

	/** Map of directory paths to requests */
	TMap<FString, FDirectoryWatchRequestMac*> RequestMap;
	TArray<FDirectoryWatchRequestMac*> RequestsPendingDelete;

	/** A count of FDirectoryWatchRequestMac created to ensure they are cleaned up on shutdown */
	int32 NumRequests;
};

typedef FDirectoryWatcherMac FDirectoryWatcher;

#endif // _INC_DIRECTORYWATCHERMAC
