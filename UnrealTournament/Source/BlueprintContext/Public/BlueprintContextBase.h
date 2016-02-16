// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BlueprintContextTypes.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "ModuleManager.h"

#include "BlueprintContextBase.generated.h"

UCLASS( Within = LocalPlayer )
class BLUEPRINTCONTEXT_API UBlueprintContextBase : public UObject
{
	GENERATED_BODY()
public:
	UBlueprintContextBase();

	virtual void PostInitProperties() override;
	virtual void BeginDestroy() override;

	int32 GetPlayerIndex() const { return LocalPlayerIndex; }

	template <class TContext>
	TWeakObjectPtr<TContext> AsWeak()
	{
		return TWeakObjectPtr<TContext>( Cast<TContext>( this ) );
	}

	/**
	 * Get the owning player and do something on it
	 *
	 * @return const ULocalPlayer*
	 */
	template <class TLocalPlayer = const ULocalPlayer>
	TTEnableIf<TIsDerivedFrom<TLocalPlayer, ULocalPlayer>::IsDerived, TLocalPlayer*> GetOwningPlayer( bool bChecked = false ) const
	{
		return !bChecked ? Cast<TLocalPlayer>( GetOuterULocalPlayer() ) :
		                   CastChecked<TLocalPlayer>( GetOuterULocalPlayer(), ECastCheckedType::NullAllowed );
	}

	// Implement this for initialization without being CDO
	virtual void Initialize();

	// Implement this for shutdown when not CDO
	virtual void Finalize();

	/**
	 * Get the context owner's player controller
	 *
	 * @return const APlayerController*
	 */
	template <class TPlayerController = const APlayerController>
	TTEnableIf<TIsDerivedFrom<TPlayerController, APlayerController>::IsDerived, TPlayerController*> GetPlayerController( bool bChecked = false ) const
	{
		if ( auto Player = GetOwningPlayer() )
		{
			return !bChecked ? Cast<TPlayerController>( Player->GetPlayerController( GetWorld() ) ) :
			                   CastChecked<TPlayerController>( Player->GetPlayerController( GetWorld() ), ECastCheckedType::NullAllowed );
		}

		return nullptr;
	}

	/**
	 * Get the context owner's player state
	 *
	 * @return const APlayerState*
	 */
	template <class TPlayerState = const APlayerState>
	TTEnableIf<TIsDerivedFrom<TPlayerState, APlayerState>::IsDerived, TPlayerState*> GetPlayerState( bool bChecked = false ) const
	{
		if ( auto Controller = GetPlayerController() )
		{
			return !bChecked ? Cast<TPlayerState>( Controller->PlayerState ) :
			                   CastChecked<TPlayerState>( Controller->PlayerState, ECastCheckedType::NullAllowed );
		}

		return nullptr;
	}

	/**
	 * Get the context owner's Pawn
	 *
	 * @return const APawn*
	 */
	template <class TPawn = const APawn>
	TTEnableIf<TIsDerivedFrom<TPawn, APawn>::IsDerived, TPawn*> GetPawn( bool bChecked = false ) const
	{
		if ( auto Controller = GetPlayerController() )
		{
			return !bChecked ? Cast<TPawn>( Controller->GetPawn() ) : CastChecked<TPawn>( Controller->GetPawn(), ECastCheckedType::NullAllowed );
		}

		return nullptr;
	}

	/**
	 * Helper function to get the game state
	 *
	 * @return const AGameState*
	 */
	template <class TGameState = const AGameState>
	TTEnableIf<TIsDerivedFrom<TGameState, AGameState>::IsDerived, TGameState*> GetGameState( bool bChecked = false ) const
	{
		if ( auto World = GetWorld() )
		{
			return !bChecked ? Cast<TGameState>( World->GetGameState() ) :
			                   CastChecked<TGameState>( World->GetGameState(), ECastCheckedType::NullAllowed );
		}

		return nullptr;
	}

protected:
	/**
	 * Called after the world is created
	 * a chance to initialize the context with a valid world reference
	 *
	 * @param World the world associated with this context
	 */
	virtual void PostInitWorld( UWorld* World );

	/**
	 * Called after the world is cleaned up
	 * a chance to cleanup the context before its world reference is removed
	 *
	 * @param World the world associated with this context
	 */
	virtual void OnWorldCleanup( UWorld* World );

	/**
	 * Called by DestructContextsForPlayer prior to MarkPendingKill
	 * a chance to clean up the context
	 *
	 * @param World the world associated with this context
	 */
	virtual void PreDestructContext( UWorld* World );

	/**
	 * Get the game instance.. self explanatory
	 *
	 * @return const UGameInstance*
	 */
	template <class TGameInstance = const UGameInstance>
	TTEnableIf<TIsDerivedFrom<TGameInstance, UGameInstance>::IsDerived, TGameInstance*> GetGameInstance( bool bChecked = false ) const
	{
		if ( auto Player = GetOwningPlayer() )
		{
			return !bChecked ? Cast<TGameInstance>( Player->GetGameInstance() ) :
			                   CastChecked<TGameInstance>( Player->GetGameInstance(), ECastCheckedType::NullAllowed );
		}

		return nullptr;
	}

	/**
	 * Get the world from the game instance
	 *
	 * @return UWorld*
	 */
	virtual UWorld* GetWorld() const override;

	/**
	 * Get a context of type T
	 *
	 * @return a pointer to the context for the local player we're operating on or null
	 */
	template <class TContext>
	TContext* GetContext() const
	{
		return Cast<TContext>( GetContextPrivate( TContext::StaticClass() ) );
	}

	// Friends
	friend class FBlueprintContextManager;

private:
	/**
	 * Get the context!
	 *
	 * @param Class the context we wanna get
	 * @return a pointer to it
	 */
	UBlueprintContextBase* GetContextPrivate( TSubclassOf<UBlueprintContextBase> Class ) const;

	UPROPERTY( Transient )
	int32 LocalPlayerIndex;
};

/**
 * Construct a new context given a game instance and a local player
 *
 * @param OwningPlayer owning local player
 * @param Class type to construct (defaults to T::StaticClass())
 * @return T* a pointer to the new object
 */
static UBlueprintContextBase* NewBlueprintContext( ULocalPlayer* OwningPlayer, TSubclassOf<UBlueprintContextBase> Class )
{
	UBlueprintContextBase* Result = NewObject<UBlueprintContextBase>( OwningPlayer, Class );
#if UE_BUILD_DEBUG
	checkf( Result, TEXT( "Couldn't allocate a blueprint context of type = %s" ), *Class->GetName() );
#endif

	return Result;
}