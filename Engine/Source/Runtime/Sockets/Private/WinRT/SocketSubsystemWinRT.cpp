// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SocketsPrivatePCH.h"
#include "SocketSubsystemWinRT.h"
#include "ModuleManager.h"
#include "IPAddress.h"

using namespace Windows::Foundation;
using namespace Windows::Networking::Sockets;

// DEFINE_LOG_CATEGORY_STATIC(LogLaunchWinRT, Log, All);

// class FWinRTSocket_UDP
// {
// public:
// 	DatagramSocket^ Socket;
// };

/**
 * Represents an internet address. All data is in network byte order
 */
class FInternetAddrWinRT : public FInternetAddr
{
public:

	FInternetAddrWinRT()
	{

	}

	virtual ~FInternetAddrWinRT() 
	{
	}

	/**
	 * Sets the ip address from a host byte order uint32
	 *
	 * @param InAddr the new address to use (must convert to network byte order)
	 */
	virtual void SetIp(uint32 InAddr) override
	{

	}
	
	/**
	 * Sets the ip address from a string ("A.B.C.D")
	 *
	 * @param InAddr the string containing the new ip address to use
	 */
	virtual void SetIp(const TCHAR* InAddr, bool& bIsValid) override
	{

	}

	/**
	 * Copies the network byte order ip address to a host byte order dword
	 *
	 * @param OutAddr the out param receiving the ip address
	 */
	virtual void GetIp(uint32& OutAddr) const override
	{

	}

	/**
	 * Sets the port number from a host byte order int
	 *
	 * @param InPort the new port to use (must convert to network byte order)
	 */
	virtual void SetPort(int32 InPort) override
	{

	}

	/**
	 * Copies the port number from this address and places it into a host byte order int
	 *
	 * @param OutPort the host byte order int that receives the port
	 */
	virtual void GetPort(int32& OutPort) const override
	{

	}

	/**
	 * Returns the port number from this address in host byte order
	 */
	virtual int32 GetPort() const override
	{
		return 0;
	}

	/** Sets the address to be any address */
	virtual void SetAnyAddress() override
	{

	}

	/** Sets the address to broadcast */
	virtual void SetBroadcastAddress() override
	{

	}

	/**
	 * Converts this internet ip address to string form
	 *
	 * @param bAppendPort whether to append the port information or not
	 */
	virtual FString ToString(bool bAppendPort) const override
	{
		return TEXT("");
	}

	/**
	 * Compares two internet ip addresses for equality
	 *
	 * @param Other the address to compare against
	 */
	virtual bool operator==(const FInternetAddr& Other) const
	{
		uint32 ThisIP, OtherIP;
		GetIp(ThisIP);
		Other.GetIp(OtherIP);
		return ThisIP == OtherIP && GetPort() == Other.GetPort();
	}

	/**
	 * Is this a well formed internet address
	 *
	 * @return true if a valid IP, false otherwise
	 */
	virtual bool IsValid() const override
	{
		return true;
	}
};

////
FSocketSubsystemWinRT* FSocketSubsystemWinRT::SocketSingleton = NULL;

FName CreateSocketSubsystem( FSocketSubsystemModule& SocketSubsystemModule )
{
	FName SubsystemName(TEXT("WINRT"));
	// Create and register our singleton factory with the main online subsystem for easy access
	FSocketSubsystemWinRT* SocketSubsystem = FSocketSubsystemWinRT::Create();
	FString Error;
	if (SocketSubsystem->Init(Error))
	{
		SocketSubsystemModule.RegisterSocketSubsystem(SubsystemName, SocketSubsystem);
		return SubsystemName;
	}
	else
	{
		FSocketSubsystemWinRT::Destroy();
		return NAME_None;
	}
}

void DestroySocketSubsystem( FSocketSubsystemModule& SocketSubsystemModule )
{
	SocketSubsystemModule.UnregisterSocketSubsystem(FName(TEXT("WINRT")));
	FSocketSubsystemWinRT::Destroy();
}

FSocketSubsystemWinRT* FSocketSubsystemWinRT::Create()
{
	if (SocketSingleton == NULL)
	{
		SocketSingleton = new FSocketSubsystemWinRT();
	}

	return SocketSingleton;
}

void FSocketSubsystemWinRT::Destroy()
{
	if (SocketSingleton != NULL)
	{
		SocketSingleton->Shutdown();
		delete SocketSingleton;
		SocketSingleton = NULL;
	}
}

bool FSocketSubsystemWinRT::Init(FString& Error)
{
	bool bSuccess = false;
	if (bTriedToInit == false)
	{
		bSuccess = true;
		bTriedToInit = true;
	}
	return bSuccess;
}

void FSocketSubsystemWinRT::Shutdown(void)
{
}

FSocket* FSocketSubsystemWinRT::CreateSocket(const FName& SocketType, const FString& SocketDescription, bool bForceUDP/* = false*/)
{
	FSocket* NewSocket = NULL;
	switch (SocketType.GetIndex())
	{
	case NAME_DGram:
		// Creates a data gram (UDP) socket
		break;
	case NAME_Stream:
		// Creates a stream (TCP) socket
		break;
	}

	return NewSocket;
}

void FSocketSubsystemWinRT::DestroySocket(class FSocket* Socket)
{
}

ESocketErrors FSocketSubsystemWinRT::GetHostByName(const ANSICHAR* HostName, FInternetAddr& OutAddr)
{
	return SE_NO_ERROR;
}

FResolveInfo* FSocketSubsystemWinRT::GetHostByName(const ANSICHAR* HostName)
{
	return NULL;
}

bool FSocketSubsystemWinRT::GetHostName(FString& HostName)
{
	return true;
}

TSharedRef<FInternetAddr> FSocketSubsystemWinRT::CreateInternetAddr(uint32 Address/*=0*/, uint32 Port/*=0*/)
{
	TSharedRef<FInternetAddr> Result = MakeShareable(new FInternetAddrWinRT);
	Result->SetIp(Address);
	Result->SetPort(Port);
	return Result;
}

bool FSocketSubsystemWinRT::GetLocalAdapterAddresses( TArray<TSharedPtr<FInternetAddr> >& OutAdresses ) override
{
	bool bCanBindAll;
	OutAdresses.Add(GetLocalHostAddr(*GLog, bCanBindAll));
	return true;
}

ESocketErrors FSocketSubsystemWinRT::GetLastErrorCode()
{
	return SE_NO_ERROR;
}

ESocketErrors FSocketSubsystemWinRT::TranslateErrorCode(int32 Code)
{
	// handle the generic -1 error
	return GetLastErrorCode();
}


bool FSocketSubsystemWinRT::HasNetworkDevice()
{
	return true;
}

const TCHAR* FSocketSubsystemWinRT::GetSocketAPIName() const
{
	return TEXT("WinSock");
}

