// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "HAL/ThreadSafeCounter.h"

/**
 * Object synchronizing read access (from any thread) and write access (only from game thread).
 * Multiple simultaneous reads from different threads are allowed. Allows multiple writes from
 * reentrant calls on main thread. Locking write doesn't prevent reads from main thread.
 **/
class FMultiReaderSingleWriterGT
{
public:
	/** Protect data from modification while reading. If issued on game thread, doesn't wait for write to finish. */
	void LockRead();

	/** Ends protecting data from modification while reading. */
	void UnlockRead();

	/** Protect data from reading while modifying. Can be called only on game thread. Reentrant. */
	void LockWrite();

	/** Ends protecting data from reading while modifying. */
	void UnlockWrite();

private:
	struct FMRSWCriticalSection
	{
		FMRSWCriticalSection()
			: Action(0)
			, ReadCounter(0)
			, WriteCounter(0)
		{ }

		int32 Action;
		FThreadSafeCounter ReadCounter;
		FThreadSafeCounter WriteCounter;
	};

	FMRSWCriticalSection CriticalSection;

	static const int32 WritingAction = -1;
	static const int32 NoAction = 0;
	static const int32 ReadingAction = 1;

	bool CanRead()
	{
		return FPlatformAtomics::InterlockedCompareExchange(&CriticalSection.Action, ReadingAction, NoAction) == ReadingAction;
	}
	
	bool CanWrite()
	{
		return FPlatformAtomics::InterlockedCompareExchange(&CriticalSection.Action, WritingAction, NoAction) == WritingAction;
	}
};

class FReadScopeLock
{
public:

	/**
	* Constructor that performs a lock on the synchronization object
	*
	* @param InSynchObject The synchronization object to manage
	*/
	FReadScopeLock(FMultiReaderSingleWriterGT* InSynchObject)
		: SynchObject(InSynchObject)
	{
		check(SynchObject);
		SynchObject->LockRead();
	}

	/** Destructor that performs a release on the synchronization object. */
	~FReadScopeLock()
	{
		check(SynchObject);
		SynchObject->UnlockRead();
	}
private:

	/** Default constructor (hidden on purpose). */
	FReadScopeLock();

	/** Copy constructor( hidden on purpose). */
	FReadScopeLock(FReadScopeLock* InScopeLock);

	/** Assignment operator (hidden on purpose). */
	FReadScopeLock& operator=(FReadScopeLock& InScopeLock)
	{
		return *this;
	}

private:

	// Holds the synchronization object to aggregate and scope manage.
	FMultiReaderSingleWriterGT* SynchObject;
};

class FWriteScopeLock
{
public:

	/**
	* Constructor that performs a lock on the synchronization object
	*
	* @param InSynchObject The synchronization object to manage
	*/
	FWriteScopeLock(FMultiReaderSingleWriterGT* InSynchObject)
		: SynchObject(InSynchObject)
	{
		check(SynchObject);
		SynchObject->LockWrite();
	}

	/** Destructor that performs a release on the synchronization object. */
	~FWriteScopeLock()
	{
		check(SynchObject);
		SynchObject->UnlockWrite();
	}
private:

	/** Default constructor (hidden on purpose). */
	FWriteScopeLock();

	/** Copy constructor( hidden on purpose). */
	FWriteScopeLock(FWriteScopeLock* InScopeLock);

	/** Assignment operator (hidden on purpose). */
	FWriteScopeLock& operator=(FWriteScopeLock& InScopeLock)
	{
		return *this;
	}

private:

	// Holds the synchronization object to aggregate and scope manage.
	FMultiReaderSingleWriterGT* SynchObject;
};
