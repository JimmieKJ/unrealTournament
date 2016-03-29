// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayDebuggerTypes.h"
#include "GameplayDebuggerCategoryReplicator.h"

class GAMEPLAYDEBUGGER_API FGameplayDebuggerAddonBase : public TSharedFromThis<FGameplayDebuggerAddonBase>
{
public:

	virtual ~FGameplayDebuggerAddonBase() {}

	int32 GetNumInputHandlers() const { return InputHandlers.Num(); }
	FGameplayDebuggerInputHandler& GetInputHandler(int32 HandlerId) { return InputHandlers[HandlerId]; }
	FString GetInputHandlerDescription(int32 HandlerId) const;

	/** check if simulate in editor mode is active */
	static bool IsSimulateInEditor();

protected:

	/** tries to find selected actor in local world */
	AActor* FindLocalDebugActor() const;

	/** returns replicator actor */
	AGameplayDebuggerCategoryReplicator* GetReplicator() const;

	/** creates new key binding handler */
	template<class UserClass>
	bool BindKeyPress(FName KeyName, UserClass* KeyHandlerObject, typename FGameplayDebuggerInputHandler::FHandler::TRawMethodDelegate< UserClass >::FMethodPtr KeyHandlerFunc)
	{
		FGameplayDebuggerInputHandler NewHandler;
		NewHandler.KeyName = KeyName;
		NewHandler.Delegate.BindRaw(KeyHandlerObject, KeyHandlerFunc);

		if (NewHandler.IsValid())
		{
			InputHandlers.Add(NewHandler);
			return true;
		}

		return false;
	}

	/** creates new key binding handler */
	template<class UserClass>
	bool BindKeyPress(FName KeyName, FGameplayDebuggerInputModifier KeyModifer, UserClass* KeyHandlerObject, typename FGameplayDebuggerInputHandler::FHandler::TRawMethodDelegate< UserClass >::FMethodPtr KeyHandlerFunc)
	{
		FGameplayDebuggerInputHandler NewHandler;
		NewHandler.KeyName = KeyName;
		NewHandler.Modifier = KeyModifer;
		NewHandler.Delegate.BindRaw(KeyHandlerObject, KeyHandlerFunc);

		if (NewHandler.IsValid())
		{
			InputHandlers.Add(NewHandler);
			return true;
		}

		return false;
	}

private:

	friend class FGameplayDebuggerAddonManager;

	/** list of input handlers */
	TArray<FGameplayDebuggerInputHandler> InputHandlers;

	/** replicator actor */
	TWeakObjectPtr<AGameplayDebuggerCategoryReplicator> RepOwner;
};
