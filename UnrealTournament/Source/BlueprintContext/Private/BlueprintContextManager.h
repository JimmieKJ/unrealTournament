// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FBlueprintContextManager : public FGCObject
{
public:
	FBlueprintContextManager( UGameInstance* InGameInstance );
	~FBlueprintContextManager();

	/**
	 * Singleton for this class
	 *
	 * @return FBlueprintContextManager&
	 */
	static FBlueprintContextManager& Get( class UGameInstance* GameInstance );

	/**
	 * Construct contexts for a new local player
	 *
	 * @param LocalPlayer the new player!
	 * @return void
	 */
	void ConstructContextsForPlayer( class ULocalPlayer* LocalPlayer );

	/**
	 * Destroy contexts for a dying local player
	 *
	 * @param LocalPlayer the player going away
	 * @return void
	 */
	void DestructContextsForPlayer( class ULocalPlayer* LocalPlayer );

	/**
	 * Get a context for a specific local player
	 *
	 * @param ControllerId the local player
	 * @param ClassType the class type of the context
	 * @return UBlueprintContextBase*
	 */
	class UBlueprintContextBase* GetContextFor( int32 ControllerId, TSubclassOf<UBlueprintContextBase> ClassType ) const;

	/**
	 * Get a context without having to cast!
	 *
	 * @param ControllerId the local player
	 * @return T*
	 */
	template <typename T>
	T* GetTypedContext( int32 ControllerId ) const
	{
		return static_cast<T*>( GetContextFor( ControllerId, T::StaticClass() ) );
	}

	// FGCObject
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;
	// FGCObject

private:
	void OnPostWorldInit( UWorld* World, const UWorld::InitializationValues );
	void HandleWorldPostInit( const FName WorldName );
	void OnWorldCleanup( UWorld* World, bool bSessionEnded, bool bCleanupResources );

	void ForEachContext( TFunctionRef<void( UBlueprintContextBase* )> Function );

	/** Maintains a mapping of PlayerControllerIndex -> [Contexts] */
	TMap<int32, TMap<TSubclassOf<UBlueprintContextBase>, UBlueprintContextBase*>> ContextMapping;

	TWeakObjectPtr<UGameInstance> GameInstance;
	TMap<FName, TWeakObjectPtr<UWorld>> PendingWorlds;

	FDelegateHandle PostWorldInitHandle;
	FDelegateHandle OnWorldCleanupHandle;

#if WITH_EDITOR
	static TMap<TWeakObjectPtr<UGameInstance>, FBlueprintContextManager*> Instances;
#else
	static FBlueprintContextManager* Instance;
#endif
};

/**
 * Utility structure to register contexts
 */
struct FContextRegistrar
{
	static TArray<TSubclassOf<UBlueprintContextBase>>& GetTypes()
	{
		static TArray<TSubclassOf<UBlueprintContextBase>> Types;
		return Types;
	}

	FContextRegistrar( TSubclassOf<UBlueprintContextBase> ClassType )
	{
		GetTypes().Add( ClassType );
	}
};

/**
 * Register a Blueprint Context to be constructed each time a local player is added
 *
 * @param ContextType a subclass of UBlueprintContextBase
 */
#define REGISTER_CONTEXT( ContextType ) static const FContextRegistrar ContextRegistrar_##ContextType( ContextType::StaticClass() );
