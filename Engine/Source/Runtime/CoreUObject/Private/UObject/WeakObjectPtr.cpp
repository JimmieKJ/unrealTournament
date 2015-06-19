// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	WeakObjectPtr.cpp: Weak pointer to UObject
=============================================================================*/

#include "CoreUObjectPrivate.h"

DEFINE_LOG_CATEGORY_STATIC(LogWeakObjectPtr, Log, All);

/*-----------------------------------------------------------------------------------------------------------
	Base serial number management. This is complicated by the need for thread safety and lock-free implementation
-------------------------------------------------------------------------------------------------------------*/

int32** GSerialNumberBlocksForDebugVisualizersRoot = 0;

/** Helper struct for a block of serial numbers **/
struct FSerialNumberBlock
{
	enum
	{
		START_SERIAL_NUMBER = 1000,				//Starting serial number...leave some room to catch corruption
		SERIAL_NUMBER_BLOCK_SIZE = 16384,		// number of UObjects oer block of serial numbers...if you change this, you need to change the visualizer
		MAX_SERIAL_NUMBER_BLOCKS = 4096,		// enough for about 7M UObjects,
		MAX_UOBJECTS = SERIAL_NUMBER_BLOCK_SIZE * MAX_SERIAL_NUMBER_BLOCKS,
	};

	int32 SerialNumbers[SERIAL_NUMBER_BLOCK_SIZE];
	FSerialNumberBlock()
	{
		for (int32 Index = 0; Index < SERIAL_NUMBER_BLOCK_SIZE; Index++)
		{
			SerialNumbers[Index] = 0;
		}
	}
};

/** Helper class to manage the serial numbers in a way that is thread-safe (deletion of UObjects is assummed to be non-concurrent, but getting serial numbers is thread safe. **/
class FSerialNumberManager : public FUObjectArray::FUObjectDeleteListener
{
	/** static list of blocks of serial numbers, these are allocated on demand (and leaked at shutdown) **/
	struct FSerialNumberBlock* Blocks[FSerialNumberBlock::MAX_SERIAL_NUMBER_BLOCKS];
	/** Current master serial number **/
	FThreadSafeCounter	MasterSerialNumber;

public:

	/** Constructor, intializes the blocks to unallocated and intializes the master serial number **/
	FSerialNumberManager()
		: MasterSerialNumber(FSerialNumberBlock::START_SERIAL_NUMBER)
	{
		for (int32 Index = 0; Index < FSerialNumberBlock::MAX_SERIAL_NUMBER_BLOCKS; Index++)
		{
			Blocks[Index] = NULL;
		}
		GSerialNumberBlocksForDebugVisualizersRoot = (int32**)&Blocks;

		FCoreDelegates::GetSerialNumberBlocksForDebugVisualizersDelegate().BindStatic(&GetSerialNumberBlocksForDebugVisualizersPtr);
	}
	/** Destructor, does nothing, leaks the memory **/
	~FSerialNumberManager()
	{
	}

	/** 
	 * Given a UObject index fetch the block and subindex for the serial number
	 * @param Index - UObject Index
	 * @param OutBlock - Serial Number Block
	 * @param OutSubIndex - Serial Number SubIndex
	 */
	void GetBlock(const int32& InIndex, int32& OutBlock, int32& OutSubIndex) const
	{
		OutBlock = InIndex / FSerialNumberBlock::SERIAL_NUMBER_BLOCK_SIZE;
		OutSubIndex = InIndex - OutBlock * FSerialNumberBlock::SERIAL_NUMBER_BLOCK_SIZE;
		checkfSlow(InIndex >= 0 && OutSubIndex >= 0 && OutSubIndex < FSerialNumberBlock::SERIAL_NUMBER_BLOCK_SIZE, TEXT("[Index:%d] [SubIndex:%d] invalid index or subindex"), InIndex, OutSubIndex);
		if (OutBlock >= FSerialNumberBlock::MAX_SERIAL_NUMBER_BLOCKS)
		{
			UE_LOG(LogWeakObjectPtr, Fatal, TEXT("[Index:%d] exceeds maximum number of UObjects, [Block:%d] increase MAX_SERIAL_NUMBER_BLOCKS"), InIndex, OutBlock);
		}
	}

	/** 
	 * Given a UObject index return the serial number. If it doesn't have a serial number, give it one. Threadsafe.
	 * @param Index - UObject Index
	 * @return - the serial number for this UObject
	 */
	int32 GetAndAllocateSerialNumber(int32 Index)
	{
		int32 Block, SubIndex;
		GetBlock( Index, Block, SubIndex );
		volatile FSerialNumberBlock** BlockPtr = (volatile FSerialNumberBlock**)&Blocks[Block];
		FSerialNumberBlock* BlockToUse = (FSerialNumberBlock*)*BlockPtr;
		if (!BlockToUse)
		{
			BlockToUse = new FSerialNumberBlock();
			FSerialNumberBlock* ValueWas = (FSerialNumberBlock*)FPlatformAtomics::InterlockedCompareExchangePointer((void**)BlockPtr,BlockToUse,NULL);
			if (ValueWas != NULL)
			{
				// some other thread already added this block
				delete BlockToUse;
				BlockToUse = ValueWas;
			}
		}
		checkSlow(BlockToUse);

		volatile int32 *SerialNumberPtr = &BlockToUse->SerialNumbers[SubIndex];
		int32 SerialNumber = *SerialNumberPtr;
		if (!SerialNumber)
		{
			SerialNumber = MasterSerialNumber.Increment();
			if (SerialNumber <= FSerialNumberBlock::START_SERIAL_NUMBER)
			{
				UE_LOG(LogWeakObjectPtr, Fatal,TEXT("UObject serial numbers overflowed."));
			}
			int32 ValueWas = FPlatformAtomics::InterlockedCompareExchange((int32 *)SerialNumberPtr,SerialNumber,0);
			if (ValueWas != 0)
			{
				// someone else go it first, use their value
				SerialNumber = ValueWas;
			}
		}
		checkSlow(SerialNumber > FSerialNumberBlock::START_SERIAL_NUMBER);
		return SerialNumber;
	}

	/** 
	 * Given a UObject index return the serial number. If it doesn't have a serial number, return 0. Threadsafe.
	 * @param Index - UObject Index
	 * @return - the serial number for this UObject
	 */
	int32 GetSerialNumber(int32 Index)
	{
		int32 Block, SubIndex;
		GetBlock( Index, Block, SubIndex );
		volatile FSerialNumberBlock** BlockPtr = (volatile FSerialNumberBlock**)&Blocks[Block];
		FSerialNumberBlock* BlockToUse = (FSerialNumberBlock*)*BlockPtr;
		int32 SerialNumber = 0;
		if (BlockToUse)
		{
			volatile int32 *SerialNumberPtr = &BlockToUse->SerialNumbers[SubIndex];
			SerialNumber = *SerialNumberPtr;
		}
		return SerialNumber;
	}
	/**
	 * Interface for FUObjectAllocator::FUObjectDeleteListener, resets the serial number for this index so that all weak pointers to this UObject are invalidated
	 *
	 * @param Object object that has been destroyed
	 * @param Index	index of object that is being deleted
	 */
	virtual void NotifyUObjectDeleted(const UObjectBase *Object, int32 Index)
	{		
		int32 Block, SubIndex;
		GetBlock( Index, Block, SubIndex );
		volatile FSerialNumberBlock** BlockPtr = (volatile FSerialNumberBlock**)&Blocks[Block];
		FSerialNumberBlock* BlockToUse = (FSerialNumberBlock*)*BlockPtr;
		if (BlockToUse)
		{			
			volatile int32 *SerialNumberPtr = &BlockToUse->SerialNumbers[SubIndex];
			int32 SerialNumber = *SerialNumberPtr;
			if (SerialNumber)
			{
				checkSlow(IsInGameThread());
				*SerialNumberPtr = 0;
				FPlatformMisc::MemoryBarrier();
				checkSlow(!*SerialNumberPtr); // nobody should be creating these while we are zeroing them!
			}
		}
	}

	static int32*** GetSerialNumberBlocksForDebugVisualizersPtr()
	{
		return &GSerialNumberBlocksForDebugVisualizersRoot;
	}
};

static FSerialNumberManager GSerialNumberManager;


/*-----------------------------------------------------------------------------------------------------------
	FWeakObjectPtr
-------------------------------------------------------------------------------------------------------------*/

/**  
 * Startup the weak object system
**/
void FWeakObjectPtr::Init()
{
	GetUObjectArray().AddUObjectDeleteListener(&GSerialNumberManager);
}

/**  
 * Copy from an object pointer
 * @param Object object to create a weak pointer to
**/
void FWeakObjectPtr::operator=(const class UObject *Object)
{
	if (Object // && UObjectInitialized() we might need this at some point, but it is a speed hit we would prefer to avoid
		)
	{
		ObjectIndex = GetUObjectArray().ObjectToIndex((UObjectBase*)Object);
		ObjectSerialNumber = GSerialNumberManager.GetAndAllocateSerialNumber(ObjectIndex);
		checkSlow(SerialNumbersMatch());
	}
	else
	{
		Reset();
	}
}

/**  
 * internal function to test for serial number matches
 * @return true if the serial number in this matches the central table
**/
bool FWeakObjectPtr::SerialNumbersMatch() const
{
	checkSlow(ObjectSerialNumber > FSerialNumberBlock::START_SERIAL_NUMBER && ObjectIndex >= 0 && ObjectIndex < FSerialNumberBlock::MAX_UOBJECTS); // otherwise this is a corrupted weak pointer
	int32 ActualSerialNumber = GSerialNumberManager.GetSerialNumber(ObjectIndex);
	checkSlow(!ActualSerialNumber || ActualSerialNumber >= ObjectSerialNumber); // serial numbers should never shrink
	return ActualSerialNumber == ObjectSerialNumber;
}


bool FWeakObjectPtr::IsValid(bool bEvenIfPendingKill, bool bThreadsafeTest) const
{
	if (ObjectSerialNumber == 0)
	{
		checkSlow(ObjectIndex == 0 || ObjectIndex == -1); // otherwise this is a corrupted weak pointer
		return false;
	}
	if (ObjectIndex < 0)
	{
		return false;
	}
	if (!SerialNumbersMatch())
	{
		return false;
	}
	if (bThreadsafeTest)
	{
		return true;
	}
	return GetUObjectArray().IsValid(ObjectIndex, bEvenIfPendingKill);
}

bool FWeakObjectPtr::IsStale(bool bEvenIfPendingKill, bool bThreadsafeTest) const
{
	if (ObjectSerialNumber == 0)
	{
		checkSlow(ObjectIndex == 0 || ObjectIndex == -1); // otherwise this is a corrupted weak pointer
		return false;
	}
	if (ObjectIndex < 0)
	{
		return true;
	}
	if (!SerialNumbersMatch())
	{
		return true;
	}
	if (bThreadsafeTest)
	{
		return false;
	}
	return GetUObjectArray().IsStale(ObjectIndex, bEvenIfPendingKill);
}

UObject* FWeakObjectPtr::Get(bool bEvenIfPendingKill) const
{
	UObject* Result = nullptr;
	if (IsValid(true))
	{
		Result = (UObject*)(GetUObjectArray().IndexToObject(GetObjectIndex(), bEvenIfPendingKill));
	}
	return Result;
}

UObject* FWeakObjectPtr::GetEvenIfUnreachable() const
{
	UObject* Result = nullptr;
	if (IsValid(true, true))
	{
		Result = static_cast<UObject*>(GetUObjectArray().IndexToObject(GetObjectIndex(), true));
	}
	return Result;
}


/**
 * Weak object pointer serialization.  Weak object pointers only have weak references to objects and
 * won't serialize the object when gathering references for garbage collection.  So in many cases, you
 * don't need to bother serializing weak object pointers.  However, serialization is required if you
 * want to load and save your object.
 */
FArchive& operator<<( FArchive& Ar, FWeakObjectPtr& WeakObjectPtr )
{
	// We never serialize our reference while the garbage collector is harvesting references
	// to objects, because we don't want weak object pointers to keep objects from being garbage
	// collected.  That would defeat the whole purpose of a weak object pointer!
	// However, when modifying both kinds of references we want to serialize and writeback the updated value.
	if( !Ar.IsObjectReferenceCollector() || Ar.IsModifyingWeakAndStrongReferences() )
	{
		// Downcast from UObjectBase to UObject
		UObject* Object = static_cast< UObject* >( WeakObjectPtr.Get(true) );

		Ar << Object;

		if( Ar.IsLoading() || Ar.IsModifyingWeakAndStrongReferences() )
		{
			WeakObjectPtr = Object;
		}
	}

	return Ar;
}

