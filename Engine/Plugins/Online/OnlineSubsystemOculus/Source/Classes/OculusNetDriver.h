// Copyright 2016 Oculus VR, LLC All Rights reserved.

#pragma once

#include "Engine/NetDriver.h"
#include "OculusNetDriver.generated.h"

/**
 *
 */
UCLASS(transient, config = Engine)
class UOculusNetDriver : public UNetDriver
{
	GENERATED_BODY()

private:

	FDelegateHandle PeerConnectRequestDelegateHandle;
	FDelegateHandle NetworkingConnectionStateChangeDelegateHandle;

public:
	TMap<ovrID, UOculusNetConnection*> Connections;

	// Begin UNetDriver interface.
	virtual bool IsAvailable() const override;
	virtual bool InitBase(bool bInitAsClient, FNetworkNotify* InNotify, const FURL& URL, bool bReuseAddressAndPort, FString& Error) override;
	virtual bool InitConnect(FNetworkNotify* InNotify, const FURL& ConnectURL, FString& Error) override;
	virtual bool InitListen(FNetworkNotify* InNotify, FURL& LocalURL, bool bReuseAddressAndPort, FString& Error) override;
	virtual void TickDispatch(float DeltaTime) override;
	virtual void ProcessRemoteFunction(class AActor* Actor, UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack, class UObject* SubObject);
	virtual void Shutdown() override;
	virtual bool IsNetResourceValid() override;

	virtual class ISocketSubsystem* GetSocketSubsystem() override;
	
	virtual FSocket * CreateSocket();

	void OnNewNetworkingPeerRequest(ovrMessageHandle Message, bool bIsError);

	void OnNetworkingConnectionStateChange(ovrMessageHandle Message, bool bIsError);
};
