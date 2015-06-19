// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Engine/StreamableManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogStreamableManager, Log, All);

struct FStreamableRequest
{
	/** Delegate to call when streaming is complete */
	FStreamableDelegate CompletionDelegate;
	/** How many FStreamables are referenced by this request */
	int32 StreamablesReferenced;

	FStreamableRequest()
		: StreamablesReferenced(0)
	{
	}
};

struct FStreamable
{
	UObject* Target;
	FWeakObjectPtr WeakTarget;
	bool	bAsyncLoadRequestOutstanding;
	bool	bAsyncUnloadRequestOutstanding;
	bool	bLoadFailed;
	TArray< TSharedRef< FStreamableRequest> > RelatedRequests;

	FStreamable()
		: Target(NULL)
		, bAsyncLoadRequestOutstanding(false)
		, bAsyncUnloadRequestOutstanding(false)
		, bLoadFailed(false)
	{
	}

	~FStreamable()
	{
		// Decrement any referenced requests
		for (int32 i = 0; i < RelatedRequests.Num(); i++)
		{
			RelatedRequests[i]->StreamablesReferenced--;
		}
		RelatedRequests.Empty();
	}

	void AddRelatedRequest(TSharedRef<FStreamableRequest> NewRequest)
	{
		if (RelatedRequests.Contains(NewRequest))
		{
			UE_LOG(LogStreamableManager, Verbose, TEXT("Duplicate item added to StreamableRequest"));
			return;
		}
		RelatedRequests.Add(NewRequest);
		NewRequest->StreamablesReferenced++;
	}
};


FStreamableManager::FStreamableManager()
{
	FCoreUObjectDelegates::PreGarbageCollect.AddRaw(this, &FStreamableManager::OnPreGarbageCollect);
	FCoreUObjectDelegates::PostGarbageCollect.AddRaw(this, &FStreamableManager::OnPostGarbageCollect);
}

FStreamableManager::~FStreamableManager()
{
	FCoreUObjectDelegates::PreGarbageCollect.RemoveAll(this);
	FCoreUObjectDelegates::PostGarbageCollect.RemoveAll(this);
	for (TStreamableMap::TIterator It(StreamableItems); It; ++It)
	{
		delete It.Value();
	}
	StreamableItems.Empty();
}

void FStreamableManager::OnPreGarbageCollect()
{
	// now lets release our reference, would have only caused issues if we released it sooner
	for (TStreamableMap::TIterator It(StreamableItems); It; ++It)
	{
		FStreamable& Item = *It.Value();
		if (Item.bAsyncUnloadRequestOutstanding && Item.RelatedRequests.Num() == 0)
		{
			// Only release if unload requested, and there are no request callbacks pending
			Item.WeakTarget = Item.Target;
			Item.Target = NULL;
		}
	}
}

void FStreamableManager::OnPostGarbageCollect()
{
	// now lets make sure it actually worked
	for (TStreamableMap::TIterator It(StreamableItems); It; ++It)
	{
		FStreamable* Existing = It.Value();
		if (Existing->bAsyncUnloadRequestOutstanding && Existing->RelatedRequests.Num() == 0)
		{
			if (Existing->WeakTarget.Get())
			{
				// This is reasonable if someone else has made a hard reference to it
				UE_LOG(LogStreamableManager, Verbose, TEXT("Failed to unload %s."), *It.Key().ToString());
			}
			delete Existing;
			It.RemoveCurrent();
		}
	}
}

UObject* FStreamableManager::SynchronousLoad(FStringAssetReference const& InTargetName)
{
	UE_LOG(LogStreamableManager, Verbose, TEXT("Synchronous load %s"), *InTargetName.ToString());

	if (FPackageName::IsShortPackageName(InTargetName.ToString()))
	{
		UE_LOG(LogStreamableManager, Error, TEXT("     Can't load invalid package name %s"), *InTargetName.ToString());
		return NULL;
	}

	FStringAssetReference TargetName = ResolveRedirects(InTargetName);
	FStreamable* Existing = StreamableItems.FindRef(TargetName);
	while (Existing && Existing->bAsyncLoadRequestOutstanding)
	{
		UE_LOG(LogStreamableManager, Verbose, TEXT("     Flushing async load for %s"), *TargetName.ToString());
		check(!Existing->bAsyncUnloadRequestOutstanding); // we should not be both loading and unloading
		FlushAsyncLoading(); 
		// the async request might have found a redirect and retried
		TargetName = ResolveRedirects(TargetName);
		Existing = StreamableItems.FindRef(TargetName);
	}
	if (!Existing)
	{
		Existing = StreamableItems.Add(TargetName, new FStreamable());
	}
	check(!Existing->bAsyncLoadRequestOutstanding); // should have already dealt with this

	if (Existing->bAsyncUnloadRequestOutstanding)
	{
		check(!Existing->bAsyncLoadRequestOutstanding); // we should not be both loading and unloading
		UE_LOG(LogStreamableManager, Verbose, TEXT("     Aborted unload for %s"), *TargetName.ToString());
		Existing->bAsyncUnloadRequestOutstanding = false;
	}
	check(!Existing->bAsyncUnloadRequestOutstanding); // should have already dealt with this
	check(!Existing->WeakTarget.Get()); // weak target is only valid during GC
	if (!Existing->Target)
	{
		UE_LOG(LogStreamableManager, Verbose, TEXT("     Static loading %s"), *TargetName.ToString());
		Existing->Target = StaticLoadObject(UObject::StaticClass(), NULL, *TargetName.ToString());
		// need to manually detect redirectors because the above call only expects to load a UObject::StaticClass() type
		while (UObjectRedirector* Redirector = Cast<UObjectRedirector>(Existing->Target))
		{
				Existing->Target = Redirector->DestinationObject;
		}
		if (Existing->Target)
		{
			UE_LOG(LogStreamableManager, Verbose, TEXT("     Static loaded %s"), *Existing->Target->GetFullName());
			FStringAssetReference PossiblyNewName(Existing->Target->GetPathName());
			if (PossiblyNewName != TargetName)
			{
				UE_LOG(LogStreamableManager, Verbose, TEXT("     Which redirected to %s"), *PossiblyNewName.ToString());
				StreamableRedirects.Add(TargetName, PossiblyNewName);
				StreamableItems.Add(PossiblyNewName, Existing);
				StreamableItems.Remove(TargetName);
				TargetName = PossiblyNewName; // we are done with the old name
			}
		}
		else
		{
			Existing->bLoadFailed = true;
			UE_LOG(LogStreamableManager, Log, TEXT("Failed attempt to load %s"), *TargetName.ToString());
		}
	}
	else
	{
		Existing->bLoadFailed = false;
	}
	return Existing->Target;
}

struct FStreamable* FStreamableManager::StreamInternal(FStringAssetReference const& InTargetName)
{
	check(IsInGameThread());
	UE_LOG(LogStreamableManager, Verbose, TEXT("Asynchronous load %s"), *InTargetName.ToString());

	if (FPackageName::IsShortPackageName(InTargetName.ToString()))
	{
		UE_LOG(LogStreamableManager, Error, TEXT("     Can't load invalid package name %s"), *InTargetName.ToString());
		return NULL;
	}

	FStringAssetReference TargetName = ResolveRedirects(InTargetName);
	FStreamable* Existing = StreamableItems.FindRef(TargetName);
	if (Existing)
	{
		if (Existing->bAsyncUnloadRequestOutstanding)
		{
			// It's valid to have a live pointer if an async loaded object was hard referenced later
			check(!Existing->bAsyncLoadRequestOutstanding); // we should not be both loading and unloading
			UE_LOG(LogStreamableManager, Verbose, TEXT("     Aborted unload for %s"), *TargetName.ToString());
			Existing->bAsyncUnloadRequestOutstanding = false;
			check(Existing->Target || Existing->bLoadFailed); // should not be an unload request unless the target is valid
			return Existing;
		}
		if (Existing->bAsyncLoadRequestOutstanding)
		{
			UE_LOG(LogStreamableManager, Verbose, TEXT("     Already in progress %s"), *TargetName.ToString());
			check(!Existing->bAsyncUnloadRequestOutstanding); // we should not be both loading and unloading
			check(!Existing->Target); // should not be an load request unless the target is invalid
			return Existing; // already have one in process
		}
		if (Existing->Target)
		{
			UE_LOG(LogStreamableManager, Verbose, TEXT("     Already Loaded %s"), *TargetName.ToString());
			return Existing; 
		}
	}
	else
	{
		Existing = StreamableItems.Add(TargetName, new FStreamable());
	}

	FindInMemory(TargetName, Existing);

	if (!Existing->Target)
	{
		FString Package = TargetName.ToString();
		int32 FirstDot = Package.Find(TEXT("."));
		if (FirstDot != INDEX_NONE)
		{
			Package = Package.Left(FirstDot);
		}

		Existing->bAsyncLoadRequestOutstanding = true;
		LoadPackageAsync(Package, FLoadPackageAsyncDelegate::CreateStatic(&AsyncLoadCallbackWrapper, new FCallback(TargetName, this)));
	}
	return Existing;
}

void FStreamableManager::SimpleAsyncLoad(FStringAssetReference const& InTargetName)
{
	StreamInternal(InTargetName);
}

void FStreamableManager::RequestAsyncLoad(const TArray<FStringAssetReference>& TargetsToStream, FStreamableDelegate DelegateToCall)
{
	// Schedule a new callback, this will get called when all related async loads are completed
	TSharedRef<FStreamableRequest> NewRequest = MakeShareable(new FStreamableRequest());
	NewRequest->CompletionDelegate = DelegateToCall;

	TArray<FStreamable *> ExistingStreamables;
	ExistingStreamables.SetNum(TargetsToStream.Num());

	for (int32 i = 0; i < TargetsToStream.Num(); i++)
	{
		FStreamable* Existing = StreamInternal(TargetsToStream[i]);
		ExistingStreamables[i] = Existing;

		if (Existing)
		{
			Existing->AddRelatedRequest(NewRequest);
		}
	}

	// Go through and complete loading anything that's already in memory, this may call the callback right away
	for (int32 i = 0; i < TargetsToStream.Num(); i++)
	{
		FStreamable* Existing = ExistingStreamables[i];

		if (Existing && Existing->Target)
		{
			CheckCompletedRequests(TargetsToStream[i], Existing);
		}
	}
}

void FStreamableManager::RequestAsyncLoad( const FStringAssetReference& TargetToStream, FStreamableDelegate DelegateToCall )
{
	TSharedRef< FStreamableRequest > NewRequest = MakeShareable( new FStreamableRequest() );
	NewRequest->CompletionDelegate = DelegateToCall;

	if ( FStreamable* Streamable = StreamInternal( TargetToStream ) )
	{
		Streamable->AddRelatedRequest( NewRequest );

		if ( Streamable->Target )
		{
			CheckCompletedRequests( TargetToStream, Streamable );
		}
	}
}


void FStreamableManager::FindInMemory(FStringAssetReference& InOutTargetName, struct FStreamable* Existing)
{
	check(Existing);
	check(!Existing->bAsyncUnloadRequestOutstanding);
	check(!Existing->bAsyncLoadRequestOutstanding);
	UE_LOG(LogStreamableManager, Verbose, TEXT("     Searching in memory for %s"), *InOutTargetName.ToString());
	Existing->Target = StaticFindObject(UObject::StaticClass(), NULL, *InOutTargetName.ToString());

	UObjectRedirector* Redir = Cast<UObjectRedirector>(Existing->Target);
	if (Redir)
	{
		Existing->Target = Redir->DestinationObject;
		UE_LOG(LogStreamableManager, Verbose, TEXT("     Found redirect %s"), *Redir->GetFullName());
		if (!Existing->Target)
		{
			Existing->bLoadFailed = true;
			UE_LOG(LogStreamableManager, Warning, TEXT("Destination of redirector was not found %s -> %s."), *InOutTargetName.ToString(), *Redir->GetFullName());
		}
		else
		{
			UE_LOG(LogStreamableManager, Verbose, TEXT("     Redirect to %s"), *Redir->DestinationObject->GetFullName());
		}
	}
	if (Existing->Target)
	{
		FStringAssetReference PossiblyNewName(Existing->Target->GetPathName());
		if (InOutTargetName != PossiblyNewName)
		{
			UE_LOG(LogStreamableManager, Verbose, TEXT("     Name changed to %s"), *PossiblyNewName.ToString());
			StreamableRedirects.Add(InOutTargetName, PossiblyNewName);
			StreamableItems.Add(PossiblyNewName, Existing);
			StreamableItems.Remove(InOutTargetName);
			InOutTargetName = PossiblyNewName; // we are done with the old name
		}
		UE_LOG(LogStreamableManager, Verbose, TEXT("     Found in memory %s"), *Existing->Target->GetFullName());
		Existing->bLoadFailed = false;
	}
}


void FStreamableManager::AsyncLoadCallback(FStringAssetReference Request)
{
	FStringAssetReference TargetName = Request;
	FStreamable* Existing = StreamableItems.FindRef(TargetName);

	UE_LOG(LogStreamableManager, Verbose, TEXT("Stream Complete callback %s"), *TargetName.ToString());
	if (!Existing)
	{
		// hmm, maybe it was redirected by a consolidate
		TargetName = ResolveRedirects(TargetName);
		Existing = StreamableItems.FindRef(TargetName);
	}
	if (Existing && Existing->bAsyncLoadRequestOutstanding)
	{
		Existing->bAsyncLoadRequestOutstanding = false;
		if (!Existing->Target)
		{
			FindInMemory(TargetName, Existing);
		}

		CheckCompletedRequests(Request, Existing);
	}
	if (Existing->Target)
	{
		UE_LOG(LogStreamableManager, Verbose, TEXT("    Found target %s"), *Existing->Target->GetFullName());
	}
	else
	{
		// Async load failed to find the object
		Existing->bLoadFailed = true;
		UE_LOG(LogStreamableManager, Verbose, TEXT("    Failed async load."), *TargetName.ToString());
	}
}

void FStreamableManager::CheckCompletedRequests(FStringAssetReference const& Target, struct FStreamable* Existing)
{
	// If we're related to any requests we should start an unload after this
	bool bShouldStartUnload = false;

	for (int32 i = 0; i < Existing->RelatedRequests.Num(); i++)
	{
		bShouldStartUnload = true;
		// Decrement related requests, and call delegate if all are done
		Existing->RelatedRequests[i]->StreamablesReferenced--;
		if (Existing->RelatedRequests[i]->StreamablesReferenced == 0)
		{
			Existing->RelatedRequests[i]->CompletionDelegate.ExecuteIfBound();
		}
	}
	// This may result in request objects being deleted due to no references
	Existing->RelatedRequests.Empty();

	if (bShouldStartUnload)
	{
		// This will end up getting the references dropped by GC
		Unload(Target);
	}
}

bool FStreamableManager::IsAsyncLoadComplete(FStringAssetReference const& InTargetName)
{
	check(IsInGameThread());
	FStringAssetReference TargetName = ResolveRedirects(InTargetName);
	FStreamable* Existing = StreamableItems.FindRef(TargetName);
	UE_LOG(LogStreamableManager, Verbose, TEXT("IsStreamComplete %s  -> %d"), *TargetName.ToString(), !Existing || !Existing->bAsyncLoadRequestOutstanding);
	return !Existing || !Existing->bAsyncLoadRequestOutstanding;
}

UObject* FStreamableManager::GetStreamed(FStringAssetReference const& InTargetName)
{
	check(IsInGameThread());
	FStringAssetReference TargetName = ResolveRedirects(InTargetName);
	FStreamable* Existing = StreamableItems.FindRef(TargetName);
	if (Existing && Existing->Target)
	{
		UE_LOG(LogStreamableManager, Verbose, TEXT("GetStreamed %s  -> %s"), *TargetName.ToString(), *Existing->Target->GetFullName());
		return Existing->Target;
	}
	UE_LOG(LogStreamableManager, Verbose, TEXT("GetStreamed %s  -> NULL"), *TargetName.ToString());
	return NULL;
}

void FStreamableManager::Unload(FStringAssetReference const& InTargetName)
{
	check(IsInGameThread());
	FStringAssetReference TargetName = ResolveRedirects(InTargetName);
	FStreamable* Existing = StreamableItems.FindRef(TargetName);
	if (Existing)
	{
		UE_LOG(LogStreamableManager, Verbose, TEXT("Unload %s"), *TargetName.ToString());
		Existing->bAsyncLoadRequestOutstanding = false;
		Existing->bAsyncUnloadRequestOutstanding = true;
	}
	else
	{
		UE_LOG(LogStreamableManager, Verbose, TEXT("Attempt to unload %s, but it isn't loaded"), *TargetName.ToString());
	}
}

void FStreamableManager::AddStructReferencedObjects(class FReferenceCollector& Collector) const
{
	for (TStreamableMap::TConstIterator It(StreamableItems); It; ++It)
	{
		FStreamable* Existing = It.Value();
		if (Existing->Target)
		{
			Collector.AddReferencedObject(Existing->Target);
		}
	}
}

FStringAssetReference FStreamableManager::ResolveRedirects(FStringAssetReference const& Target) const
{
	FStringAssetReference const* Redir = StreamableRedirects.Find(Target);
	if (Redir)
	{
		check(Target != *Redir);
		UE_LOG(LogStreamableManager, Verbose, TEXT("Redirected %s -> %s"), *Target.ToString(), *Redir->ToString());
		return *Redir;
	}
	return Target;
}


