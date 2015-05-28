// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSubsystemUtilsClasses.h"

#include "UnitTestNetDriver.generated.h"


/**
 * A net driver for supporting unit test net connections
 */
UCLASS(transient)
class UUnitTestNetDriver : public UIpNetDriver
{
	GENERATED_UCLASS_BODY()

	virtual bool InitConnectionClass() override;

	virtual bool InitConnect(FNetworkNotify* InNotify, const FURL& ConnectURL, FString& Error) override;

	virtual void ProcessRemoteFunction(AActor* Actor, UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack,
										UObject* SubObject) override;


	/**
	 * Delegate for hooking ProcessRemoteFunction
	 *
	 * @param Actor			The actor the RPC will be called in
	 * @param Function		The RPC to call
	 * @param Parameters	The parameters data blob
	 * @param OutParms		Out parameter information (irrelevant for RPC's)
	 * @param Stack			The script stack
	 * @param SubObject		The sub-object the RPC is being called in (if applicable)
	 * @return				Whether or not to allow sending of the RPC
	 */
	DECLARE_DELEGATE_RetVal_SixParams(bool, FOnSendRPCDel, AActor* /*Actor*/, UFunction* /*Function*/, void* /*Parameters*/,
										FOutParmRec* /*OutParms*/, FFrame* /*Stack*/, UObject* /*SubObject*/);


	/** Delegate for hooking ProcessRemoteFunction */
	FOnSendRPCDel	SendRPCDel;
};


