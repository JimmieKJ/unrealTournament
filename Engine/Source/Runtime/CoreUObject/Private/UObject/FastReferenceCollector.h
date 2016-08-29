// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/GarbageCollection.h"

/*=============================================================================
	FastReferenceCollector.h: Unreal realtime garbage collection helpers
=============================================================================*/

/**
 * Helper class that looks for UObject references by traversing UClass token stream and calls AddReferencedObjects.
 * Provides a generic way of processing references that is used by Unreal Engine garbage collection.
 * Can be used for fast (does not use serialization) reference collection purposes.
 * 
 * IT IS CRITICAL THIS CLASS DOES NOT CHANGE WITHOUT CONSIDERING PERFORMANCE IMPACT OF SAID CHANGES
 *
 * This class depends on three components: ReferenceProcessor, ReferenceCollector and ArrayPool.
 * The assumptions for each of those components are as follows:
 *
   class FSampleReferenceProcessor
   {
   public:
     int32 GetMinDesiredObjectsPerSubTask() const;
     volatile bool IsRunningMultithreaded() const;
		 void SetIsRunningMultithreaded(bool bIsParallel);
		 void HandleTokenStreamObjectReference(TArray<UObject*>& ObjectsToSerialize, UObject* ReferencingObject, UObject*& Object, const int32 TokenIndex, bool bAllowReferenceElimination);
		 void UpdateDetailedStats(UObject* CurrentObject, uint32 DeltaCycles);
		 void LogDetailedStatsSummary();
	 };

	 class FSampleCollctor : public FReferenceCollector 
	 {
	   // Needs to implement FReferenceCollector pure virtual functions
	 };
   
	 class FSampleArrayPool
	 {
	   static FSampleArrayPool& Get();
		 TArray<UObject*>* GetArrayFromPool();
		 void ReturnToPool(TArray<UObject*>* Array);
	 };
 */
template <typename ReferenceProcessorType, typename CollectorType, typename ArrayPoolType, bool bAutoGenerateTokenStream = false>
class TFastReferenceCollector
{
private:

	/** Task graph task responsible for processing UObject array */
	class FCollectorTask
	{
		TFastReferenceCollector*	Owner;
		TArray<UObject*>*	ObjectsToSerialize;
		ArrayPoolType& ArrayPool;

	public:
		FCollectorTask(TFastReferenceCollector* InOwner, const TArray<UObject*>* InObjectsToSerialize, int32 StartIndex, int32 NumObjects, ArrayPoolType& InArrayPool)
			: Owner(InOwner)
			, ObjectsToSerialize(InArrayPool.GetArrayFromPool())
			, ArrayPool(InArrayPool)
		{
			ObjectsToSerialize->AddUninitialized(NumObjects);
			FMemory::Memcpy(ObjectsToSerialize->GetData(), InObjectsToSerialize->GetData() + StartIndex, NumObjects * sizeof(UObject*));
		}
		~FCollectorTask()
		{
			ArrayPool.ReturnToPool(ObjectsToSerialize);
		}
		FORCEINLINE TStatId GetStatId() const
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(FCollectorTask, STATGROUP_TaskGraphTasks);
		}
		static ENamedThreads::Type GetDesiredThread()
		{
			return ENamedThreads::AnyThread;
		}
		static ESubsequentsMode::Type GetSubsequentsMode()
		{
			return ESubsequentsMode::TrackSubsequents;
		}
		void DoTask(ENamedThreads::Type CurrentThread, FGraphEventRef& MyCompletionGraphEvent)
		{
			Owner->ProcessObjectArray(*ObjectsToSerialize, MyCompletionGraphEvent);
		}
	};

	/** Object that handles all UObject references */
	ReferenceProcessorType& ReferenceProcessor;
	/** Custom TArray allocator */
	ArrayPoolType& ArrayPool;

	/** Helper struct for stack based approach */
	struct FStackEntry
	{
		/** Current data pointer, incremented by stride */
		uint8*	Data;
		/** Current stride */
		int32		Stride;
		/** Current loop count, decremented each iteration */
		int32		Count;
		/** First token index in loop */
		int32		LoopStartIndex;
	};



public:
	/** Default constructor, initializing all members. */
	TFastReferenceCollector(ReferenceProcessorType& InReferenceProcessor, ArrayPoolType& InArrayPool)
		: ReferenceProcessor(InReferenceProcessor)
		, ArrayPool(InArrayPool)
	{}

	/**
	* Performs reachability analysis.
	*
	* @param ObjectsToCollectReferencesFor List of objects which references should be collected
	* @param bForceSingleThreaded Collect references on a single thread
	*/
	void CollectReferences(TArray<UObject*>& ObjectsToCollectReferencesFor, bool bForceSingleThreaded = false)
	{
		if (ObjectsToCollectReferencesFor.Num())
		{
			check(!ReferenceProcessor.IsRunningMultithreaded());

			if (bForceSingleThreaded)
			{
				FGraphEventRef InvalidRef;
				ReferenceProcessor.SetIsRunningMultithreaded(false);
				ProcessObjectArray(ObjectsToCollectReferencesFor, InvalidRef);
			}
			else
			{
				ReferenceProcessor.SetIsRunningMultithreaded(true);

				int32 NumChunks = FMath::Min<int32>(FTaskGraphInterface::Get().GetNumWorkerThreads(), ObjectsToCollectReferencesFor.Num());
				int32 NumPerChunk = ObjectsToCollectReferencesFor.Num() / NumChunks;
				check(NumPerChunk > 0);
				FGraphEventArray ChunkTasks;
				ChunkTasks.Empty(NumChunks);
				int32 StartIndex = 0;
				for (int32 Chunk = 0; Chunk < NumChunks; Chunk++)
				{
					if (Chunk + 1 == NumChunks)
					{
						NumPerChunk = ObjectsToCollectReferencesFor.Num() - StartIndex; // last chunk takes all remaining items
					}
					ChunkTasks.Add(TGraphTask< FCollectorTask >::CreateTask().ConstructAndDispatchWhenReady(this, &ObjectsToCollectReferencesFor, StartIndex, NumPerChunk, ArrayPool));
					StartIndex += NumPerChunk;
				}
				QUICK_SCOPE_CYCLE_COUNTER(STAT_GC_Subtask_Wait);
				FTaskGraphInterface::Get().WaitUntilTasksComplete(ChunkTasks, ENamedThreads::GameThread_Local);
				ReferenceProcessor.SetIsRunningMultithreaded(false);
			}
		}
	}

private:

	/**
	 * Traverses UObject token stream to find existing references
	 *
	 * @param InObjectsToSerializeArray Objects to process
	 * @param MyCompletionGraphEvent Task graph event
	 */
	void ProcessObjectArray(TArray<UObject*>& InObjectsToSerializeArray, FGraphEventRef& MyCompletionGraphEvent)
	{
		DECLARE_SCOPE_CYCLE_COUNTER(TEXT("TFastReferenceCollector::ProcessObjectArray"), STAT_FFastReferenceCollector_ProcessObjectArray, STATGROUP_GC);

		UObject* CurrentObject = NULL;

		const int32 MinDesiredObjectsPerSubTask = ReferenceProcessor.GetMinDesiredObjectsPerSubTask(); // sometimes there will be less, a lot less

		/** Growing array of objects that require serialization */
		TArray<UObject*>&	NewObjectsToSerializeArray = *ArrayPool.GetArrayFromPool();

		// Ping-pong between these two arrays if there's not enough objects to spawn a new task
		TArray<UObject*>& ObjectsToSerialize = InObjectsToSerializeArray;
		TArray<UObject*>& NewObjectsToSerialize = NewObjectsToSerializeArray;

		// Presized "recursion" stack for handling arrays and structs.
		TArray<FStackEntry> Stack;
		Stack.AddUninitialized(128); //@todo rtgc: need to add code handling more than 128 layers of recursion or at least assert

		// it is necessary to have at least one extra item in the array memory block for the iffy prefetch code, below
		ObjectsToSerialize.Reserve(ObjectsToSerialize.Num() + 1);

		// Keep serializing objects till we reach the end of the growing array at which point
		// we are done.
		int32 CurrentIndex = 0;
		do
		{
			CollectorType ReferenceCollector(ReferenceProcessor, NewObjectsToSerialize);
			while (CurrentIndex < ObjectsToSerialize.Num())
			{
#if PERF_DETAILED_PER_CLASS_GC_STATS
				uint32 StartCycles = FPlatformTime::Cycles();
#endif
				CurrentObject = ObjectsToSerialize[CurrentIndex++];

				// GetData() used to avoiding bounds checking (min and max)
				// FMath::Min used to avoid out of bounds (without branching) on last iteration. Though anything can be passed into PrefetchBlock, 
				// reading ObjectsToSerialize out of bounds is not safe since ObjectsToSerialize[Num()] may be an unallocated/unsafe address.
				const UObject * const NextObject = ObjectsToSerialize.GetData()[FMath::Min<int32>(CurrentIndex, ObjectsToSerialize.Num() - 1)];

				// Prefetch the next object assuming that the property size of the next object is the same as the current one.
				// This allows us to avoid a branch here.
				FPlatformMisc::PrefetchBlock(NextObject, CurrentObject->GetClass()->GetPropertiesSize());

				//@todo rtgc: we need to handle object references in struct defaults

				// Make sure that token stream has been assembled at this point as the below code relies on it.
				if (bAutoGenerateTokenStream && !ReferenceProcessor.IsRunningMultithreaded())
				{
					UClass* ObjectClass = CurrentObject->GetClass();
					if (!ObjectClass->HasAnyClassFlags(CLASS_TokenStreamAssembled))
					{
						ObjectClass->AssembleReferenceTokenStream();
					}
				}
				check(CurrentObject->GetClass()->HasAnyClassFlags(CLASS_TokenStreamAssembled));

				// Get pointer to token stream and jump to the start.
				FGCReferenceTokenStream* RESTRICT TokenStream = &CurrentObject->GetClass()->ReferenceTokenStream;
				uint32 TokenStreamIndex = 0;
				// Keep track of index to reference info. Used to avoid LHSs.
				uint32 ReferenceTokenStreamIndex = 0;

				// Create stack entry and initialize sane values.
				FStackEntry* RESTRICT StackEntry = Stack.GetData();
				uint8* StackEntryData = (uint8*)CurrentObject;
				StackEntry->Data = StackEntryData;
				StackEntry->Stride = 0;
				StackEntry->Count = -1;
				StackEntry->LoopStartIndex = -1;

				// Keep track of token return count in separate integer as arrays need to fiddle with it.
				int32 TokenReturnCount = 0;

				// Parse the token stream.
				while (true)
				{
					// Cache current token index as it is the one pointing to the reference info.
					ReferenceTokenStreamIndex = TokenStreamIndex;

					// Handle returning from an array of structs, array of structs of arrays of ... (yadda yadda)
					for (int32 ReturnCount = 0; ReturnCount<TokenReturnCount; ReturnCount++)
					{
						// Make sure there's no stack underflow.
						check(StackEntry->Count != -1);

						// We pre-decrement as we're already through the loop once at this point.
						if (--StackEntry->Count > 0)
						{
							// Point data to next entry.
							StackEntryData = StackEntry->Data + StackEntry->Stride;
							StackEntry->Data = StackEntryData;

							// Jump back to the beginning of the loop.
							TokenStreamIndex = StackEntry->LoopStartIndex;
							ReferenceTokenStreamIndex = StackEntry->LoopStartIndex;
							// We're not done with this token loop so we need to early out instead of backing out further.
							break;
						}
						else
						{
							StackEntry--;
							StackEntryData = StackEntry->Data;
						}
					}

					TokenStreamIndex++;
					FGCReferenceInfo ReferenceInfo = TokenStream->AccessReferenceInfo(ReferenceTokenStreamIndex);

					if (ReferenceInfo.Type == GCRT_Object)
					{
						// We're dealing with an object reference.
						UObject**	ObjectPtr = (UObject**)(StackEntryData + ReferenceInfo.Offset);
						UObject*&	Object = *ObjectPtr;
						TokenReturnCount = ReferenceInfo.ReturnCount;
						ReferenceProcessor.HandleTokenStreamObjectReference(NewObjectsToSerialize, CurrentObject, Object, ReferenceTokenStreamIndex, true);
					}
					else if (ReferenceInfo.Type == GCRT_ArrayObject)
					{
						// We're dealing with an array of object references.
						TArray<UObject*>& ObjectArray = *((TArray<UObject*>*)(StackEntryData + ReferenceInfo.Offset));
						TokenReturnCount = ReferenceInfo.ReturnCount;
						for (int32 ObjectIndex = 0, ObjectNum = ObjectArray.Num(); ObjectIndex < ObjectNum; ++ObjectIndex)
						{
							ReferenceProcessor.HandleTokenStreamObjectReference(NewObjectsToSerialize, CurrentObject, ObjectArray[ObjectIndex], ReferenceTokenStreamIndex, true);
						}
					}
					else if (ReferenceInfo.Type == GCRT_ArrayStruct)
					{
						// We're dealing with a dynamic array of structs.
						const FScriptArray& Array = *((FScriptArray*)(StackEntryData + ReferenceInfo.Offset));
						StackEntry++;
						StackEntryData = (uint8*)Array.GetData();
						StackEntry->Data = StackEntryData;
						StackEntry->Stride = TokenStream->ReadStride(TokenStreamIndex);
						StackEntry->Count = Array.Num();

						const FGCSkipInfo SkipInfo = TokenStream->ReadSkipInfo(TokenStreamIndex);
						StackEntry->LoopStartIndex = TokenStreamIndex;

						if (StackEntry->Count == 0)
						{
							// Skip empty array by jumping to skip index and set return count to the one about to be read in.
							TokenStreamIndex = SkipInfo.SkipIndex;
							TokenReturnCount = TokenStream->GetSkipReturnCount(SkipInfo);
						}
						else
						{
							// Loop again.
							check(StackEntry->Data);
							TokenReturnCount = 0;
						}
					}
					else if (ReferenceInfo.Type == GCRT_PersistentObject)
					{
						// We're dealing with an object reference.
						UObject**	ObjectPtr = (UObject**)(StackEntryData + ReferenceInfo.Offset);
						UObject*&	Object = *ObjectPtr;
						TokenReturnCount = ReferenceInfo.ReturnCount;
						ReferenceProcessor.HandleTokenStreamObjectReference(NewObjectsToSerialize, CurrentObject, Object, ReferenceTokenStreamIndex, false);
					}
					else if (ReferenceInfo.Type == GCRT_FixedArray)
					{
						// We're dealing with a fixed size array
						uint8* PreviousData = StackEntryData;
						StackEntry++;
						StackEntryData = PreviousData;
						StackEntry->Data = PreviousData;
						StackEntry->Stride = TokenStream->ReadStride(TokenStreamIndex);
						StackEntry->Count = TokenStream->ReadCount(TokenStreamIndex);
						StackEntry->LoopStartIndex = TokenStreamIndex;
						TokenReturnCount = 0;
					}
					else if (ReferenceInfo.Type == GCRT_AddStructReferencedObjects)
					{
						// We're dealing with a function call
						void const*	StructPtr = (void*)(StackEntryData + ReferenceInfo.Offset);
						TokenReturnCount = ReferenceInfo.ReturnCount;
						UScriptStruct::ICppStructOps::TPointerToAddStructReferencedObjects Func = (UScriptStruct::ICppStructOps::TPointerToAddStructReferencedObjects) TokenStream->ReadPointer(TokenStreamIndex);
						Func(StructPtr, ReferenceCollector);
					}
					else if (ReferenceInfo.Type == GCRT_AddReferencedObjects)
					{
						// Static AddReferencedObjects function call.
						void(*AddReferencedObjects)(UObject*, FReferenceCollector&) = (void(*)(UObject*, FReferenceCollector&))TokenStream->ReadPointer(TokenStreamIndex);
						TokenReturnCount = ReferenceInfo.ReturnCount;
						AddReferencedObjects(CurrentObject, ReferenceCollector);
					}
					else if (ReferenceInfo.Type == GCRT_AddTMapReferencedObjects)
					{
						void*         Map = StackEntryData + ReferenceInfo.Offset;
						UMapProperty* MapProperty = (UMapProperty*)TokenStream->ReadPointer(TokenStreamIndex);
						TokenReturnCount = ReferenceInfo.ReturnCount;
						FSimpleObjectReferenceCollectorArchive CollectorArchive(CurrentObject, ReferenceCollector);
						MapProperty->SerializeItem(CollectorArchive, Map, nullptr);
					}
					else if (ReferenceInfo.Type == GCRT_AddTSetReferencedObjects)
					{
						void*         Set = StackEntryData + ReferenceInfo.Offset;
						USetProperty* SetProperty = (USetProperty*)TokenStream->ReadPointer(TokenStreamIndex);
						TokenReturnCount = ReferenceInfo.ReturnCount;
						FSimpleObjectReferenceCollectorArchive CollectorArchive(CurrentObject, ReferenceCollector);
						SetProperty->SerializeItem(CollectorArchive, Set, nullptr);
					}
					else if (ReferenceInfo.Type == GCRT_EndOfPointer)
					{
						TokenReturnCount = ReferenceInfo.ReturnCount;
					}
					else if (ReferenceInfo.Type == GCRT_EndOfStream)
					{
						// Break out of loop.
						break;
					}
					else
					{
						UE_LOG(LogGarbage, Fatal, TEXT("Unknown token"));
					}
				}
				check(StackEntry == Stack.GetData());

				if (ReferenceProcessor.IsRunningMultithreaded() && NewObjectsToSerialize.Num() >= MinDesiredObjectsPerSubTask)
				{
					// This will start queueing task with objects from the end of array until there's less objects than worth to queue
					const int32 ObjectsPerSubTask = FMath::Max<int32>(MinDesiredObjectsPerSubTask, NewObjectsToSerialize.Num() / FTaskGraphInterface::Get().GetNumWorkerThreads());
					while (NewObjectsToSerialize.Num() >= MinDesiredObjectsPerSubTask)
					{
						const int32 StartIndex = FMath::Max(0, NewObjectsToSerialize.Num() - ObjectsPerSubTask);
						const int32 NumThisTask = NewObjectsToSerialize.Num() - StartIndex;
						MyCompletionGraphEvent->DontCompleteUntil(TGraphTask< FCollectorTask >::CreateTask().ConstructAndDispatchWhenReady(this, &NewObjectsToSerialize, StartIndex, NumThisTask, ArrayPool));
						NewObjectsToSerialize.SetNumUnsafeInternal(StartIndex);
					}
				}
#if PERF_DETAILED_PER_CLASS_GC_STATS
				// Detailed per class stats should not be performed when parallel GC is running
				check(!ReferenceProcessor.IsRunningMultithreaded());
				ReferenceProcessor.UpdateDetailedStats(CurrentObject, FPlatformTime::Cycles() - StartCycles);
			}
			// Log summary stats.
			ReferenceProcessor.LogDetailedStatsSummary();
#else
			}
#endif
			if (ReferenceProcessor.IsRunningMultithreaded() && NewObjectsToSerialize.Num() >= MinDesiredObjectsPerSubTask)
			{
				const int32 ObjectsPerSubTask = FMath::Max<int32>(MinDesiredObjectsPerSubTask, NewObjectsToSerialize.Num() / FTaskGraphInterface::Get().GetNumWorkerThreads());
				int32 StartIndex = 0;
				while (StartIndex < NewObjectsToSerialize.Num())
				{
					const int32 NumThisTask = FMath::Min<int32>(ObjectsPerSubTask, NewObjectsToSerialize.Num() - StartIndex);
					MyCompletionGraphEvent->DontCompleteUntil(TGraphTask< FCollectorTask >::CreateTask().ConstructAndDispatchWhenReady(this, &NewObjectsToSerialize, StartIndex, NumThisTask, ArrayPool));
					StartIndex += NumThisTask;
				}
				NewObjectsToSerialize.SetNumUnsafeInternal(0);
			}
			else if (NewObjectsToSerialize.Num())
			{
				// Don't spawn a new task, continue in the current one
				// To avoid allocating and moving memory around swap ObjectsToSerialize and NewObjectsToSerialize arrays
				Exchange(ObjectsToSerialize, NewObjectsToSerialize);
				// Empty but don't free allocated memory
				NewObjectsToSerialize.SetNumUnsafeInternal(0);

				CurrentIndex = 0;
			}
		}
		while (CurrentIndex < ObjectsToSerialize.Num());

		ArrayPool.ReturnToPool(&NewObjectsToSerializeArray);
	}
};