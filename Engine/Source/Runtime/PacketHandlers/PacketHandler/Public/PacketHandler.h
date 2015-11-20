// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleManager.h"
#include "Core.h"

DECLARE_LOG_CATEGORY_EXTERN(PacketHandlerLog, Log, All);

class HandlerComponent;
class ReliabilityHandlerComponent;

/* Enums related to the Packet Handler */
namespace Handler
{
	/* State of Packet Handler */
	enum State
	{
		Uninitialized,
		InitializingProcessors,
		Initialized
	};

	/* Mode of Packet Handler */
	enum Mode
	{
		Client,
		Server
	};

	namespace Component
	{
		/* Packet Handler State */
		enum State
		{
			UnInitialized,
			InitializedOnLocal, // Initialized on local instance
			InitializeOnRemote, // Initialized on remote instance, not on local instance
			Initialized         // Initialized on both local and remote instances
		};
	}
}

/* The result of calling Incoming and Outgoing in the PacketHandler */
struct PACKETHANDLER_API ProcessedPacket
{
	ProcessedPacket(uint8* Data, int32 Count);
	uint8* Data;
	int32 Count;
};

/*
* Packet Handler will buffer packets, this struct is used to buffer such packets
* while handler components are initialized
*/
struct PACKETHANDLER_API BufferedPacket
{
	BufferedPacket();
	~BufferedPacket();

	uint8* Data;
	uint32 BytesCount;
	float SendTime;
	uint32 Id;
};

/*
* This class maintains an array of all Packet Handler Components and
* forwards incoming and outgoing packets the each component
*/
class PACKETHANDLER_API PacketHandler
{
public:
	/* Default initialization of data */
	PacketHandler();

	/* Destructor */
	virtual ~PacketHandler() { };

	/* Handles initialization of manager */
	virtual void Initialize(Handler::Mode Mode);

	/* Tick per frame */
	virtual void Tick(float DeltaTime);

	/* For adding handler components */
	virtual void Add(TSharedPtr<HandlerComponent> NewHandler);

	/* Handles any incoming packets */
	virtual const ProcessedPacket Incoming(uint8* Packet, int32 Count);

	/* Handles any outgoing packets */
	virtual const ProcessedPacket Outgoing(uint8* Packet, int32 Count);

	/* Indicates that a handler was initialized */
	void HandlerComponentInitialized();

	/* Queue's a packet to be sent when the handler is ticked */
	void QueuePacketForSending(BufferedPacket* PacketToQueue);

	/* Gets a packet from the the buffered packet queue for sending */
	BufferedPacket* GetQueuedPacket();


	/** Whether or not the PacketHandler system is enabled */
	bool bEnabled;

	/* Mode of the handler, Client or Server */
	Handler::Mode Mode;

	/* Time, updated by Tick */
	float Time;

private:
	/* Set state of handler */
	void SetState(Handler::State InState);

	/* Called when handler is initialized */
	void HandlerInitialized();

	/* Used for packing outgoing packets */
	FBitWriter OutgoingPacket;

	/* Used for unpacking incoming packets */
	FBitReader IncomingPacket;

	/* Array of Handler components */
	TArray<TSharedPtr<HandlerComponent>> HandlerComponents;

	/* State of the handler */
	Handler::State State;
	
	/*
	* Packets that are buffered while handler components
	* are being initialized
	*/
	TArray<BufferedPacket*> BufferedPackets;

	/*
	* Packets that are queued to be sent when handler is ticked
	*/
	TQueue<BufferedPacket*> QueuedPackets;

	/* Reliability Handler Component */
	TSharedPtr<ReliabilityHandlerComponent> ReliabilityComponent;
};

/*
* This class appends or modifies incoming and outgoing packets
* on a connection
*/
class PACKETHANDLER_API HandlerComponent
{
public:
	/* Default initialization of data */
	HandlerComponent();

	/** Base destructor */
	virtual ~HandlerComponent() {}

	/* Returns whether this handler is currently active */
	virtual bool IsActive() const;

	/* Return whether this handler is valid */
	virtual bool IsValid() const PURE_VIRTUAL(Handler::Component::IsValid, return false;);

	/* Returns whether this handler is initialized */
	bool IsInitialized() const;

	/* Returns whether this handler requires a tick every frame */
	bool DoesTick() const;

	/* Handles any incoming packets */
	virtual void Incoming(FBitReader& Packet) = 0;

	/* Handles any outgoing packets */
	virtual void Outgoing(FBitWriter& Packet) = 0;

	/* Initialization functionality should be placed here */
	virtual void Initialize() = 0;

	/* Tick functionality should be placed here */
	virtual void Tick(float DeltaTime){};

	/* The manager of the handler, set in initialization */
	PacketHandler* Handler; 

	/* Whether this handler is active */
	virtual void SetActive(bool Active);

protected:

	/* Sets the state of the handler */
	void SetState(Handler::Component::State State);

	/* Should be called when the handler is fully initialized on both remote and local */
	void Initialized();

	/* The state of this handler */
	Handler::Component::State State;

private:
	/* 
	* Whether this handler is active, which dictates whether it will receive
	* incoming and outgoing packets.
	*/
	bool bActive;

	/* Whether this handler is fully initialized on both remote and local */
	bool bInitialized;

	/* Allows manager access to setting the Handler variable */
	friend PacketHandler;
};

/** Packet Handler Module Interface */
class FPacketHandlerComponentModuleInterface : public IModuleInterface
{
public:
	/* Creates an instance of this component */
	virtual TSharedPtr<HandlerComponent> CreateComponentInstance(FString& Options)
		PURE_VIRTUAL(FPacketHandlerModuleInterface::CreateComponentInstance, return TSharedPtr<HandlerComponent>(NULL););
};
