// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "DirectoryWatcherPrivatePCH.h"

FDirectoryWatchRequestLinux::FDirectoryWatchRequestLinux()
:	bRunning(false)
,	bEndWatchRequestInvoked(false)
{
	NotifyFilter = IN_CREATE | IN_MOVE | IN_MODIFY | IN_DELETE;
}

FDirectoryWatchRequestLinux::~FDirectoryWatchRequestLinux()
{
	Shutdown();
}

void FDirectoryWatchRequestLinux::Shutdown()
{
	FMemory::Free(WatchDescriptor);
	close(FileDescriptor);
}

bool FDirectoryWatchRequestLinux::Init(const FString& InDirectory)
{
	if (InDirectory.Len() == 0)
	{
		// Verify input
		return false;
	}

	Directory = InDirectory;

	if (bRunning)
	{
		Shutdown();
	}

	bEndWatchRequestInvoked = false;

	// Make sure the path is absolute
	const FString FullPath = FPaths::ConvertRelativePathToFull(InDirectory);

	FileDescriptor = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);

	if (FileDescriptor == -1)
	{
		// Failed to init inotify
		UE_LOG(LogDirectoryWatcher, Error, TEXT("Failed to init inotify"));
		return false;
	}

	IFileManager::Get().FindFilesRecursive(AllFiles, *FullPath, TEXT("*"), false, true);

	// Allocate memory for watch descriptors
	SIZE_T AllocSize = AllFiles.Num()+1 * sizeof(int);
	WatchDescriptor = reinterpret_cast<int*>(FMemory::Malloc(AllocSize));
	if (WatchDescriptor == nullptr) 
	{
		UE_LOG(LogDirectoryWatcher, Error, TEXT("Failed to allocate memory for WatchDescriptor"));
		return false;
	}
	FMemory::Memzero(WatchDescriptor, AllocSize);

	for (int32 FileIdx = 0; FileIdx < AllFiles.Num(); ++FileIdx)
	{
			const FString& FolderName = AllFiles[FileIdx];
			WatchDescriptor[FileIdx] = inotify_add_watch(FileDescriptor, TCHAR_TO_UTF8(*FolderName), NotifyFilter); //FileIdx+1
			if (WatchDescriptor[FileIdx] == -1) 
			{
				UE_LOG(LogDirectoryWatcher, Error, TEXT("inotify_add_watch cannot watch folder %s"), *FolderName);
				return false;
			}
	}

	bRunning = true;

	return true;
}

FDelegateHandle FDirectoryWatchRequestLinux::AddDelegate(const IDirectoryWatcher::FDirectoryChanged& InDelegate)
{
	Delegates.Add(InDelegate);
	return Delegates.Last().GetHandle();
}

bool FDirectoryWatchRequestLinux::RemoveDelegate(const IDirectoryWatcher::FDirectoryChanged& InDelegate)
{
	return DEPRECATED_RemoveDelegate(InDelegate);
}

bool FDirectoryWatchRequestLinux::DEPRECATED_RemoveDelegate(const IDirectoryWatcher::FDirectoryChanged& InDelegate)
{
	return Delegates.RemoveAll([&](const IDirectoryWatcher::FDirectoryChanged& Delegate) {
		return Delegate.DEPRECATED_Compare(InDelegate);
	}) != 0;
}

bool FDirectoryWatchRequestLinux::RemoveDelegate(FDelegateHandle InHandle)
{
	return Delegates.RemoveAll([=](const IDirectoryWatcher::FDirectoryChanged& Delegate) {
		return Delegate.GetHandle() == InHandle;
	}) != 0;
}

bool FDirectoryWatchRequestLinux::HasDelegates() const
{
	return Delegates.Num() > 0;
}

void FDirectoryWatchRequestLinux::EndWatchRequest()
{
	bEndWatchRequestInvoked = true;
}


void FDirectoryWatchRequestLinux::ProcessPendingNotifications()
{
	ProcessChanges();

	// Trigger all listening delegates with the files that have changed
	if (FileChanges.Num() > 0)
	{
		for (int32 DelegateIdx = 0; DelegateIdx < Delegates.Num(); ++DelegateIdx)
		{
			Delegates[DelegateIdx].Execute(FileChanges);
		}

		FileChanges.Empty();
	}
}

void FDirectoryWatchRequestLinux::ProcessChanges()
{
	uint8_t Buf[BUF_LEN] __attribute__ ((aligned(__alignof__(struct inotify_event))));
	const struct inotify_event* Event;
	ssize_t Len = 0;
	uint8_t* Ptr = nullptr;

	// Loop while events can be read from inotify file descriptor
	for (;;)
	{
		FFileChangeData::EFileChangeAction Action = FFileChangeData::FCA_Unknown;
		// Read some events
		Len = read(FileDescriptor, Buf, sizeof Buf);
		if (Len == -1 && errno != EAGAIN)
		{
			UE_LOG(LogDirectoryWatcher, Fatal, TEXT("DirectoryWatchRequest ProcessChanges read error"));
			// unreachable
			break;
		}

		// If the non-blocking read() found no events to read, then it returns -1 with errno set to EAGAIN. 
		if (Len <= 0)
		{
			break;
		}

		// Loop over all events in the buffer
		for (Ptr = Buf; Ptr < Buf + Len;)
		{
			Event = reinterpret_cast<const struct inotify_event *>(Ptr);

			if ((Event->mask & IN_CREATE) || (Event->mask & IN_MOVED_TO))
			{
				Action = FFileChangeData::FCA_Added;
			}

			if (Event->mask & IN_MODIFY)
			{
				Action = FFileChangeData::FCA_Modified;
			}

			if ((Event->mask & IN_DELETE) || (Event->mask & IN_MOVED_FROM))
			{
				Action = FFileChangeData::FCA_Removed;
			}

			Ptr += sizeof(struct inotify_event) + Event->len;

			const FString Filename = Directory / UTF8_TO_TCHAR(Event->name);
			new (FileChanges) FFileChangeData(Filename, Action);
		}
	}
}
