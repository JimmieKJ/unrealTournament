// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "BlueprintContext.h"

#include "BlueprintContextManager.h"

#if WITH_EDITOR
/*static*/ TMap<TWeakObjectPtr<UGameInstance>, FBlueprintContextManager*> FBlueprintContextManager::Instances;
#else
/*static*/ FBlueprintContextManager* FBlueprintContextManager::Instance = nullptr;
#endif

FBlueprintContextManager::FBlueprintContextManager( UGameInstance* InGameInstance )
    : GameInstance( InGameInstance )
{
	PostWorldInitHandle  = FWorldDelegates::OnPostWorldInitialization.AddRaw( this, &FBlueprintContextManager::OnPostWorldInit );
	OnWorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddRaw( this, &FBlueprintContextManager::OnWorldCleanup );
}

FBlueprintContextManager::~FBlueprintContextManager()
{
	FWorldDelegates::OnPostWorldInitialization.Remove( PostWorldInitHandle );
	FWorldDelegates::OnWorldCleanup.Remove( OnWorldCleanupHandle );
}

/*static*/ FBlueprintContextManager& FBlueprintContextManager::Get( UGameInstance* GameInstance )
{
#if WITH_EDITOR
	FBlueprintContextManager*& Result = Instances.FindOrAdd( GameInstance );
	if ( !Result )
	{
		Result = new FBlueprintContextManager( GameInstance );
	}

	return *Result;
#else
	if ( !FBlueprintContextManager::Instance )
	{
		FBlueprintContextManager::Instance = new FBlueprintContextManager( GameInstance );
	}

	return *FBlueprintContextManager::Instance;
#endif
}

void FBlueprintContextManager::ConstructContextsForPlayer( ULocalPlayer* LocalPlayer )
{
	checkf( LocalPlayer, TEXT( "You can't call this method without a valid player!" ) );

	const int32 PlayerIndex = LocalPlayer->GetControllerId();
	auto& PlayerContexts = ContextMapping.FindOrAdd( PlayerIndex );
	for ( const auto& ClassType : FContextRegistrar::GetTypes() )
	{
		UBlueprintContextBase*& Context = PlayerContexts.Add( ClassType );
		Context                         = NewBlueprintContext( LocalPlayer, ClassType );
	}

#if WITH_EDITOR
	if ( LocalPlayer->GetWorld() )
	{
		OnPostWorldInit( LocalPlayer->GetWorld(), UWorld::InitializationValues() );
	}
#endif // WITH_EDITOR
}

void FBlueprintContextManager::DestructContextsForPlayer( ULocalPlayer* LocalPlayer )
{
	checkf( LocalPlayer, TEXT( "You can't call this method without a valid player!" ) );

	UWorld* World = LocalPlayer->GetWorld();
	auto PlayerContexts = ContextMapping.Find( LocalPlayer->GetControllerId() );
	if ( PlayerContexts )
	{
		for ( auto Iter = PlayerContexts->CreateIterator(); Iter; ++Iter )
		{
			Iter.Value()->PreDestructContext( World );
			Iter.Value()->MarkPendingKill();
		}
	}

	ContextMapping.Remove( LocalPlayer->GetControllerId() );
}

UBlueprintContextBase* FBlueprintContextManager::GetContextFor( int32 ControllerId, TSubclassOf<UBlueprintContextBase> ClassType ) const
{
	const auto* PlayerContexts = ContextMapping.Find( ControllerId );
	if ( PlayerContexts )
	{
		UBlueprintContextBase* const* Context = PlayerContexts->Find( ClassType );
		if ( Context )
		{
			return *Context;
		}
	}

	return nullptr;
}

void FBlueprintContextManager::AddReferencedObjects( FReferenceCollector& Collector )
{
	for ( auto Iter = ContextMapping.CreateIterator(); Iter; ++Iter )
	{
		Collector.AddReferencedObjects<TSubclassOf<UBlueprintContextBase>, UBlueprintContextBase*>( Iter.Value() );
	}
}

void FBlueprintContextManager::OnPostWorldInit( UWorld* World, const UWorld::InitializationValues )
{
	TWeakObjectPtr<UWorld>& PendingWorld = PendingWorlds.FindOrAdd( World->GetFName() );
	PendingWorld                         = World;

	if ( GameInstance.IsValid() )
	{
		GameInstance->GetTimerManager().SetTimerForNextTick(
		    FTimerDelegate::CreateRaw( this, &FBlueprintContextManager::HandleWorldPostInit, World->GetFName() ) );
	}
}

void FBlueprintContextManager::HandleWorldPostInit( const FName WorldName )
{
	TWeakObjectPtr<UWorld>* PendingWorld = PendingWorlds.Find( WorldName );
	if ( PendingWorld )
	{
		UWorld* World = PendingWorld->IsValid() ? PendingWorld->Get() : nullptr;
		PendingWorlds.Remove( WorldName );

		if ( const FWorldContext* WorldContext = GEngine->GetWorldContextFromWorld( World ) )
		{
			if ( WorldContext->OwningGameInstance == GameInstance.Get() )
			{
				ForEachContext( [World]( UBlueprintContextBase* InContext )
				                {
					                InContext->PostInitWorld( World );
					            } );
			}
		}
	}
}

void FBlueprintContextManager::OnWorldCleanup( UWorld* World, bool bSessionEnded, bool bCleanupResources )
{
	if ( const FWorldContext* WorldContext = GEngine->GetWorldContextFromWorld( World ) )
	{
		if ( WorldContext->OwningGameInstance == GameInstance.Get() )
		{
			ForEachContext( [World]( UBlueprintContextBase* InContext )
			                {
				                InContext->OnWorldCleanup( World );
				            } );
		}
	}
}

void FBlueprintContextManager::ForEachContext( TFunctionRef<void( UBlueprintContextBase* )> Function )
{
	for ( auto Context : ContextMapping )
	{
		for ( auto ContextIter : Context.Value )
		{
			Function( ContextIter.Value );
		}
	}
}
