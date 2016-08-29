// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSubsystemOculusPrivatePCH.h"

#include "IPAddressOculus.h"
#include "OculusNetConnection.h"

void UOculusNetConnection::InitBase(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, EConnectionState InState, int32 InMaxPacket, int32 InPacketOverhead)
{
	// Pass the call up the chain
	Super::InitBase(InDriver, InSocket, InURL, InState,
		// Use the default packet size/overhead unless overridden by a child class
		InMaxPacket == 0 ? MAX_PACKET_SIZE : InMaxPacket,
		/* PacketOverhead */ 1);

	// We handle our own overhead
	PacketOverhead = 0;

	// Initalize the send buffer
	InitSendBuffer();

	if (Driver->InitialConnectTimeout == 0.0)
	{
		UE_LOG(LogNet, Warning, TEXT("InitalConnectTimeout was set to %f"), Driver->InitialConnectTimeout);
		Driver->InitialConnectTimeout = 120.0;
	}

	if (Driver->ConnectionTimeout == 0.0)
	{
		UE_LOG(LogNet, Warning, TEXT("ConnectionTimeout was set to %f"), Driver->ConnectionTimeout);
		Driver->ConnectionTimeout = 120.0;
	}

	if (Driver->KeepAliveTime == 0.0)
	{
		UE_LOG(LogNet, Warning, TEXT("KeepAliveTime was set to %f"), Driver->KeepAliveTime);
		Driver->KeepAliveTime = 0.2;
	}

	if (Driver->SpawnPrioritySeconds == 0.0)
	{
		UE_LOG(LogNet, Warning, TEXT("SpawnPrioritySeconds was set to %f"), Driver->SpawnPrioritySeconds);
		Driver->SpawnPrioritySeconds = 1.0;
	}

	if (Driver->RelevantTimeout == 0.0)
	{
		UE_LOG(LogNet, Warning, TEXT("RelevantTimeout was set to %f"), Driver->RelevantTimeout);
		Driver->RelevantTimeout = 5.0;
	}
}

void UOculusNetConnection::InitLocalConnection(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, EConnectionState InState, int32 InMaxPacket, int32 InPacketOverhead)
{
	InitBase(InDriver, InSocket, InURL, InState,
		// Use the default packet size/overhead unless overridden by a child class
		InMaxPacket == 0 ? MAX_PACKET_SIZE : InMaxPacket,
		0);
}

void UOculusNetConnection::InitRemoteConnection(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, const class FInternetAddr& InRemoteAddr, EConnectionState InState, int32 InMaxPacket, int32 InPacketOverhead)
{
	InitBase(InDriver, InSocket, InURL, InState,
		// Use the default packet size/overhead unless overridden by a child class
		InMaxPacket == 0 ? MAX_PACKET_SIZE : InMaxPacket,
		0);

	auto OculusAddr = static_cast<const FInternetAddrOculus&>(InRemoteAddr);
	PeerID = OculusAddr.GetID();
}

void UOculusNetConnection::LowLevelSend(void* Data, int32 CountBytes, int32 CountBits)
{
	check(PeerID);
	const uint8* DataToSend = reinterpret_cast<uint8*>(Data);

	UE_LOG(LogNetTraffic, VeryVerbose, TEXT("Low level send to: %llu Count: %d"), PeerID, CountBytes);

	// Process any packet modifiers
	if (Handler.IsValid() && !Handler->GetRawSend())
	{
		const ProcessedPacket ProcessedData = Handler->Outgoing(reinterpret_cast<uint8*>(Data), CountBits);

		DataToSend = ProcessedData.Data;
		CountBytes = FMath::DivideAndRoundUp(ProcessedData.CountBits, 8);
		CountBits = ProcessedData.CountBits;
	}

	ovr_Net_SendPacket(PeerID, (size_t)CountBytes, DataToSend, (InternalAck) ? ovrSend_Reliable : ovrSend_Unreliable);
}

FString UOculusNetConnection::LowLevelGetRemoteAddress(bool bAppendPort)
{
	return FString::Printf(TEXT("%llu.oculus"), PeerID);
}

FString UOculusNetConnection::LowLevelDescribe()
{
	return FString::Printf(TEXT("PeerId=%llu"), PeerID);
}

void UOculusNetConnection::FinishDestroy()
{
	Super::FinishDestroy();
	if (PeerID != 0 && State != EConnectionState::USOCK_Closed)
	{
		ovr_Net_Close(PeerID);
	}
}

FString UOculusNetConnection::RemoteAddressToString()
{
	return LowLevelGetRemoteAddress(/* bAppendPort */ false);
}
