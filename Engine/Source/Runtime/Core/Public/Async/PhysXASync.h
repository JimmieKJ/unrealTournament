// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#if 0

/** Implements basic multi-reader and single-writer synchronization primitive. */
class FMultiReaderSingleWriter
{
private:
	FCriticalSection CriticalSectionWrite;
	FCriticalSection CriticalSectionReaderCount;
	FEvent* ReadersCleared;

	int32 Readers;	

public:
	FMultiReaderSingleWriter()
		: Readers( 0 ) 
	{
		ReadersCleared = FPlatformProcess::CreateSynchEvent( true );
		ReadersCleared->Trigger();
	}

	~FMultiReaderSingleWriter()
	{
		ReadersCleared->Wait();
		delete ReadersCleared;
	}

	void EnterReader(void)
	{
		FScopeLock ScopeLockWrite( &CriticalSectionWrite );
		FScopeLock ScopeLockReaderCount( &CriticalSectionReaderCount );
		if (++Readers == 1)
		{
			ReadersCleared->Reset();
		}
	}

	void LeaveReader(void)
	{
		FScopeLock ScopeLockReaderCount( &CriticalSectionReaderCount );
		if (--Readers == 0)
		{
			ReadersCleared->Trigger();
		}
	}

	void EnterWriter(void)
	{
		CriticalSectionWrite.Lock();
		ReadersCleared->Wait();
	}

	void LeaveWriter(void)
	{
		CriticalSectionWrite.Unlock();
	}
};

FORCEINLINE FMultiReaderSingleWriter& GetPhysXMultiReaderSingleWriter()
{
	static FMultiReaderSingleWriter PhysXSync;
	return PhysXSync;
}

class FScopeLockPhysXReader
{
public:
	/**
	 * Constructor that enters the reader.
	 *
	 */
	FScopeLockPhysXReader()
	{
		GetPhysXMultiReaderSingleWriter().EnterReader();
	}
	/**
	 * Destructor that performs leaves the reader.
	 */
	~FScopeLockPhysXReader ()
	{
		GetPhysXMultiReaderSingleWriter().LeaveReader();
	}
};

class FScopeLockPhysXWriter
{
public:
	/**
	 * Constructor that enters the writer.
	 *
	 */
	FScopeLockPhysXWriter()
	{
		GetPhysXMultiReaderSingleWriter().EnterWriter();
	}
	/**
	 * Destructor that leaves the writer.
	 */
	~FScopeLockPhysXWriter ()
	{
		GetPhysXMultiReaderSingleWriter().LeaveWriter();
	}
};

#else

class FScopeLockPhysXReader
{
public:
	FScopeLockPhysXReader()
	{}

	~FScopeLockPhysXReader ()
	{}
};

class FScopeLockPhysXWriter
{
public:
	FScopeLockPhysXWriter()
	{}

	~FScopeLockPhysXWriter ()
	{}
};

#endif