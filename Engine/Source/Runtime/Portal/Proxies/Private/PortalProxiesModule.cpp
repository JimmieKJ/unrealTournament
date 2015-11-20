// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PortalProxiesPrivatePCH.h"
#include "IPortalServiceProvider.h"
#include "IPortalApplicationWindow.h"


/**
 * Implements the PortalProxies module.
 */
class FPortalProxiesModule
	: public IPortalServiceProvider
{
public:

	// IModuleInterface interface

	virtual void StartupModule() override 
	{
		ApplicationWindow_SupportedServiceNames.Add(TNameOf<IPortalApplicationWindow>::GetName());
	}
	virtual void ShutdownModule() override { }

	virtual bool SupportsDynamicReloading() override
	{
		return false;
	}

public:

	// IPortalServiceProvider interface

	virtual TSharedPtr<IPortalService> GetService(const FString& ServiceName, const TSharedRef<FTypeContainer>& Dependencies) override
	{
		TSharedPtr<IMessageRpcClient> RpcClient = Dependencies->GetInstance<IMessageRpcClient>();

		if (!RpcClient.IsValid())
		{
			return nullptr;
		}

		if (ApplicationWindow_SupportedServiceNames.Contains(ServiceName))
		{
			return FPortalApplicationWindowProxyFactory::Create(RpcClient.ToSharedRef());
		}
	
		//Add additional supported proxy services here

		return nullptr;
	}

private:

	TSet<FString> ApplicationWindow_SupportedServiceNames;

};


IMPLEMENT_MODULE(FPortalProxiesModule, PortalProxies);
