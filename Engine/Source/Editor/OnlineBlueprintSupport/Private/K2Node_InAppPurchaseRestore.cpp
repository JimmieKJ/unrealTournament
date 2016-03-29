// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "OnlineBlueprintSupportPrivatePCH.h"
#include "InAppPurchaseRestoreCallbackProxy.h"
#include "K2Node_InAppPurchaseRestore.h"

#define LOCTEXT_NAMESPACE "K2Node"

UK2Node_InAppPurchaseRestore::UK2Node_InAppPurchaseRestore(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ProxyFactoryFunctionName = GET_FUNCTION_NAME_CHECKED(UInAppPurchaseRestoreCallbackProxy, CreateProxyObjectForInAppPurchaseRestore);
	ProxyFactoryClass = UInAppPurchaseRestoreCallbackProxy::StaticClass();

	ProxyClass = UInAppPurchaseRestoreCallbackProxy::StaticClass();
}

#undef LOCTEXT_NAMESPACE
