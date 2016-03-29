// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Template for triple buffers.
 *
 * This template implements a lock-free triple buffer that can be used to exchange data
 * between two threads that are producing and consuming at different rates. Instead of
 * atomically exchanging pointers to the buffers, we atomically update a Flags register
 * that holds the indices into a 3-element buffer array.
 *
 * The three buffers are named as follows:
 * - Read buffer: This is where Read() will read the latest value from
 * - Write buffer: This is where Write() will write a new value to
 * - Temp buffer: This is the second back-buffer currently not used for reading or writing
 *
 * Please note that reading and writing to the buffer does not automatically swap the
 * back-buffers. Instead, two separate methods, SwapReadBuffers() and SwapWriteBuffers()
 * are provided. For convenience, we also provide SwapAndRead() and WriteAndSwap() to
 * update and swap the buffers using a single method call.
 *
 * A dirty flag indicates whether a new value has been written and swapped into the second
 * back-buffer and is available for reading. It can be checked using the IsDirtyMethod().
 * As an optimization, SwapReadBuffers() and SwapAndRead() will not perform a back-buffer
 * swap if no new data is available.
 *
 * This class is thread-safe in single-producer, single-consumer scenarios.
 *
 * Based on ideas and C code in "Triple Buffering as a Concurrency Mechanism" (Reddit.com)
 */
template<typename ItemType>
class TTripleBuffer
{
	/** Enumerates human readable bit values for accessing the Flags field. */
	enum EBufferFlag
	{
		/** Indicates whether a new buffer is available for reading. */
		Dirty = 0x40,

		/** Initial flags value (0dttwwrr; dirty = false, temp index = 0, write index = 1, read index = 2) */
		Initial = 0x06,

		/** Bit mask for accessing the read buffer index (bit 0-1). */
		ReaderMask = 0x03,

		/** Bit mask for the index of the unused/clean/empty buffer (bit 4-5). */
		TempMask = 0x30,

		/** Bit shift for accessing the temp buffer index. */
		TempShift = 4,

		/** Bit mask for accessing the write buffer index (bit 2-3). */
		WriterMask = 0x0c,

		/** Bit shift for accessing the write buffer index. */
		WriterShift = 2,
	};

public:

	/** Default constructor. */
	TTripleBuffer()
	{
		Buffers[0] = Buffers[1] = Buffers[2] = ItemType();
		Flags = EBufferFlag::Initial;
	}

	/** Default constructor (no initialization). */
	explicit TTripleBuffer(ENoInit)
	{
		Flags = EBufferFlag::Initial;
	}

	/**
	 * Create and initialize a new instance.
	 *
	 * @param InValue The initial value of all three buffers.
	 */
	explicit TTripleBuffer(const ItemType& InValue)
	{
		Buffers[0] = Buffers[1] = Buffers[2] = InValue;
		Flags = EBufferFlag::Initial;
	}

	/**
	 * Create and initialize a new instance.
	 *
	 * @param ReadValue The read buffer's initial value.
	 * @param WriteValue The write buffer's initial value.
	 * @param TempValue The temp buffer's initial value.
	 */
	TTripleBuffer(const ItemType& ReadValue, const ItemType& WriteValue, const ItemType& TempValue)
	{
		Buffers[0] = TempValue;
		Buffers[1] = WriteValue;
		Buffers[2] = ReadValue;
		Flags = EBufferFlag::Initial | EBufferFlag::Dirty;
	}

public:

	/**
	 * Check whether a new value is available for reading.
	 *
	 * @return true if a new value is available, false otherwise.
	 */
	bool IsDirty() const
	{
		return ((Flags & EBufferFlag::Dirty) != 0);
	}

	/**
	 * Read a value from the current read buffer.
	 *
	 * @return Reference to the read buffer's value.
	 * @see SwapRead, Write
	 */
	ItemType& Read()
	{
		return Buffers[Flags & EBufferFlag::ReaderMask];
	}

	/**
	 * Swap the latest read buffer, if available.
	 *
	 * Will not perform a back-buffer swap if no new data is available (dirty flag = false).
	 *
	 * @see SwapWrite, Read
	 */
	void SwapReadBuffers()
	{
		if (!IsDirty())
		{
			return;
		}

		int32 OldFlags = 0;

		do
		{
			OldFlags = Flags;
		}
		while (FPlatformAtomics::InterlockedCompareExchange(&Flags, SwapReadWithTempFlags(OldFlags), OldFlags) != OldFlags);
	}

public:

	/**
	 * Get the current write buffer.
	 *
	 * @return Reference to write buffer.
	 */
	ItemType& GetWriteBuffer()
	{
		return Buffers[(Flags & EBufferFlag::WriterMask) >> EBufferFlag::WriterShift];
	}

	/**
	 * Swap a new write buffer (makes current write buffer available for reading).
	 *
	 * @see SwapRead, Write
	 */
	void SwapWriteBuffers()
	{
		int32 OldFlags = 0;

		do
		{
			OldFlags = Flags;
		}
		while (FPlatformAtomics::InterlockedCompareExchange(&Flags, SwapWriteWithTempFlags(OldFlags), OldFlags) != OldFlags);
	}

	/**
	 * Write a value to the current write buffer.
	 *
	 * @param Value The value to write.
	 * @see SwapWrite, Read
	 */
	void Write(const ItemType Value)
	{
		Buffers[(Flags & EBufferFlag::WriterMask) >> EBufferFlag::WriterShift] = Value;
	}

public:

	/**
	 * Get read-only access to the underlying buffers.
	 *
	 * @return Pointer to the first element of the buffer array,.
	 */
	ItemType* GetBuffers()
	{
		return Buffers;
	}

	/**
	 * Convenience method for fetching and reading the latest buffer.
	 *
	 * @return Reference to the buffer.
	 * @see SwapRead, Read, WriteAndSwap
	 */
	const ItemType& SwapAndRead()
	{
		SwapReadBuffers();
		return Read();
	}

	/**
	 * Convenience method for writing the latest buffer and fetching a new one.
	 *
	 * @param Value The value to write into the buffer.
	 * @see SwapAndRead, SwapWrite, Write
	 */
	void WriteAndSwap(const ItemType Value)
	{
		Write(Value);
		SwapWriteBuffers();
	}

private:

	/** Swaps the read and temp buffer indices in the Flags field. */
	static FORCEINLINE int32 SwapReadWithTempFlags(int32 Flags)
	{
		return ((Flags & EBufferFlag::ReaderMask) << 4) | ((Flags & EBufferFlag::TempMask) >> 4) | (Flags & EBufferFlag::WriterMask);
	}

	/** Swaps the write and temp buffer indices in the Flags field, and sets the dirty bit. */
	static FORCEINLINE int32 SwapWriteWithTempFlags(int32 Flags)
	{
		return ((Flags & EBufferFlag::TempMask) >> 2) | ((Flags & EBufferFlag::WriterMask) << 2) | (Flags & EBufferFlag::ReaderMask) | EBufferFlag::Dirty;
	}

private:

	/** Hidden copy constructor (triple buffers cannot be copied). */
	TTripleBuffer(const TTripleBuffer&);

	/** Hidden copy assignment (triple buffers cannot be copied). */
	TTripleBuffer& operator=(const TTripleBuffer&);

private:

	/** The three buffers. */
	ItemType Buffers[3];

	/** Buffer access flags. */
	MS_ALIGN(16) int32 volatile Flags GCC_ALIGN(16);
};
