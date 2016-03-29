// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "StreamableManager.generated.h"

/** Defines FStreamableDelegate delegate interface */
DECLARE_DELEGATE( FStreamableDelegate );

USTRUCT()
struct ENGINE_API FStreamableManager
{
	GENERATED_USTRUCT_BODY();

	// Default priority for all async loads
	static const TAsyncLoadPriority DefaultAsyncLoadPriority = 0;

	FStreamableManager();
	~FStreamableManager();

	/** 
	 * Synchronously load the referred asset and return the loaded object, or NULL if it can't be found.
	 * A hard reference is made to the object (use Unload to release it), and this can be very slow.
	 */
	UObject* SynchronousLoad(FStringAssetReference const& Target);

	template< typename T >
	T* SynchronousLoadType( FStringAssetReference const& Target )
	{
		return Cast< T >( SynchronousLoad( Target ) );
	}
	
	/** 
	 * Perform a simple asynchronous load of a single object.
	 * Object will be strongly referenced until Unload is manually called on it
	 */
	void SimpleAsyncLoad(FStringAssetReference const& Target, TAsyncLoadPriority Priority = DefaultAsyncLoadPriority);

	/** 
	 * Releases a reference to an asynchonously loaded object, that was loaded by SimpleAsyncLoad
	 */
	void Unload(FStringAssetReference const& Target);

	/** 
	 * Returns true if an object has been fully loaded that was previously asynchronously loaded
	 */
	bool IsAsyncLoadComplete(FStringAssetReference const& Target);

	/** 
	 * Request streaming of one or more target objects, and call a delegate on completion. 
	 * Objects will be strongly referenced until the delegate is called, then Unload is called automatically 
	 */
	void RequestAsyncLoad(const TArray<FStringAssetReference>& TargetsToStream, FStreamableDelegate DelegateToCall, TAsyncLoadPriority Priority = DefaultAsyncLoadPriority);
	void RequestAsyncLoad(const FStringAssetReference& TargetToStream, FStreamableDelegate DelegateToCall, TAsyncLoadPriority Priority = DefaultAsyncLoadPriority);

	void RequestAsyncLoad( const TArray<FStringAssetReference>& TargetsToStream, TFunction<void()>&& Callback, TAsyncLoadPriority Priority = DefaultAsyncLoadPriority );
	void RequestAsyncLoad( const FStringAssetReference& TargetToStream, TFunction<void()>&& Callback, TAsyncLoadPriority Priority = DefaultAsyncLoadPriority );

	/** Exposes references to GC system */
	void AddStructReferencedObjects(class FReferenceCollector& Collector) const;

private:
	FStringAssetReference ResolveRedirects(FStringAssetReference const& Target) const;
	void FindInMemory(FStringAssetReference& InOutTarget, struct FStreamable* Existing);
	struct FStreamable* StreamInternal(FStringAssetReference const& Target, TAsyncLoadPriority Priority);
	UObject* GetStreamed(FStringAssetReference const& Target);
	void CheckCompletedRequests(FStringAssetReference const& Target, struct FStreamable* Existing);

	void OnPreGarbageCollect();
	void OnPostGarbageCollect();
	void AsyncLoadCallback(FStringAssetReference Request);

	struct FCallback
	{
		const FStringAssetReference Request;
		FStreamableManager*	Manager;

		FCallback(FStringAssetReference const& InRequest, FStreamableManager* InManager)
			: Request(InRequest)
			, Manager(InManager)
		{
		}
	};

	static void AsyncLoadCallbackWrapper(const FName& PackageName, UPackage* LevelPackage, EAsyncLoadingResult::Type Result, FCallback* Handler)
	{
		FCallback* Callback = Handler;
		Callback->Manager->AsyncLoadCallback(Callback->Request);
		delete Callback;
	}

	typedef TMap<FStringAssetReference, struct FStreamable*> TStreamableMap;
	TStreamableMap StreamableItems;

	typedef TMap<FStringAssetReference, FStringAssetReference> TStreamableRedirects;
	TStreamableRedirects StreamableRedirects;

};

template<>
struct TStructOpsTypeTraits<FStreamableManager> : public TStructOpsTypeTraitsBase
{
	enum 
	{
		WithAddStructReferencedObjects = true,
	};
};


