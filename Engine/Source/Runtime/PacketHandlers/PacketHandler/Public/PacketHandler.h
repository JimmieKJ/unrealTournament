// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(PacketHandlerLog, Log, All);

class HandlerComponent;
class ReliabilityHandlerComponent;


/**
 * Enums related to the PacketHandler
 */
namespace Handler
{
	/**
	 * State of PacketHandler
	 */
	enum State
	{
		Uninitialized,			// PacketHandler is uninitialized
		InitializingComponents,	// PacketHandler is initializing HandlerComponents
		Initialized				// PacketHandler and all HandlerComponents (if any) are initialized
	};

	/**
	 * Mode of Packet Handler
	 */
	enum Mode
	{
		Client,					// Clientside PacketHandler
		Server					// Serverside PacketHandler
	};

	namespace Component
	{
		/**
		 * HandlerComponent State
		 */
		enum State
		{
			UnInitialized,		// HandlerComponent not yet initialized
			InitializedOnLocal, // Initialized on local instance
			InitializeOnRemote, // Initialized on remote instance, not on local instance
			Initialized         // Initialized on both local and remote instances
		};
	}
}

/**
 * The result of calling Incoming and Outgoing in the PacketHandler
 */
struct PACKETHANDLER_API ProcessedPacket
{
	/** Pointer to the returned packet data */
	uint8* Data;

	/** Size of the returned packet data in bits */
	int32 CountBits;

public:
	ProcessedPacket()
		: Data()
		, CountBits(0)
	{
	}

	/**
	 * Base constructor
	 */
	ProcessedPacket(uint8* InData, int32 InCountBits)
		: Data(InData)
		, CountBits(InCountBits)
	{
	}
};

/**
 * PacketHandler will buffer packets, this struct is used to buffer such packets while handler components are initialized
 */
struct PACKETHANDLER_API BufferedPacket
{
	/** Buffered packet data */
	uint8* Data;

	/** Size of buffered packet in bits */
	uint32 CountBits;

	/** Used by ReliabilityHandlerComponent, to mark a packet for resending */
	float ResendTime;

	/** Used by ReliabilityHandlerComponent, to track packet id's */
	uint32 Id;

	/** For connectionless packets, the address to send to (format is abstract, determined by active net driver) */
	FString Address;


public:
	/**
	 * Base constructor
	 */
	BufferedPacket()
		: Data(nullptr)
		, CountBits(0)
		, ResendTime(0.f)
		, Id(0)
		, Address()
	{
	}

	BufferedPacket(uint8* InCopyData, uint32 InCountBits, float InResendTime=0.f, uint32 InId=0)
		: CountBits(InCountBits)
		, ResendTime(InResendTime)
		, Id(InId)
		, Address()
	{
		check(InCopyData != nullptr);

		Data = new uint8[FMath::DivideAndRoundUp(InCountBits, 8u)];
		FMemory::Memcpy(Data, InCopyData, FMath::DivideAndRoundUp(InCountBits, 8u));
	}

	BufferedPacket(FString InAddress, uint8* InCopyData, uint32 InCountBits, float InResendTime=0.f, uint32 InId=0)
		: BufferedPacket(InCopyData, InCountBits, InResendTime, InId)
	{
		Address = InAddress;
	}

	/**
	 * Base destructor
	 */
	~BufferedPacket()
	{
		if (Data != nullptr)
		{
			delete Data;
		}
	}
};

/**
 * This class maintains an array of all PacketHandler Components and forwards incoming and outgoing packets the each component
 */
class PACKETHANDLER_API PacketHandler
{
public:
	/**
	 * Base constructor
	 */
	PacketHandler();

	/**
	 * Base Destructor
	 */
	virtual ~PacketHandler()
	{
	};

	/**
	 * Handles initialization of manager
	 *
	 * @param Mode					The mode the manager should be initialized in
	 * @param bConnectionlessOnly	Whether or not this is a connectionless-only manager (ignores .ini components)
	 */
	virtual void Initialize(Handler::Mode Mode, bool bConnectionlessOnly=false);

	/**
	 * Triggers initialization of HandlerComponents.
	 */
	virtual void InitializeComponents();

	virtual void Tick(float DeltaTime);

	/**
	 * Adds a HandlerComponent to the pipeline, prior to initialization (none can be added after initialization)
	 *
	 * @param NewHandler		The HandlerComponent to add
	 * @param bDeferInitialize	Whether or not to defer triggering Initialize (for batch-adds - code calling this, triggers it instead)
	 */
	virtual void AddHandler(TSharedPtr<HandlerComponent> NewHandler, bool bDeferInitialize=false);

	/**
	 * As above, but initializes from a string specifying the component module, and (optionally) additional options
	 *
	 * @param ComponentStr		The handler component to load
	 * @param bDeferInitialize	Whether or not to defer triggering Initialize (for batch-adds - code calling this, triggers it instead)
	 */
	virtual TSharedPtr<HandlerComponent> AddHandler(FString ComponentStr, bool bDeferInitialize=false);


	/**
	 * Processes incoming UNetConnection packets, through all HandlerComponents, and returns the final packet for local handling
	 *
	 * @param Packet		The packet data to be processed
	 * @param CountBytes	The size of the packet data in bytes
	 * @return				Returns the final packet
	 */
	virtual const ProcessedPacket Incoming(uint8* Packet, int32 CountBytes);

	/**
	 * Processes outgoing UNetConnection packets, through all HandlerComponents, and returns the final packet for sending
	 *
	 * @param Packet		The packet data to be processed
	 * @param CountBits		The size of the packet data in bits
	 * @return				Returns the final packet
	 */
	virtual const ProcessedPacket Outgoing(uint8* Packet, int32 CountBits);

	/**
	 * Processes incoming packets without a UNetConnection, in the same manner as 'Incoming' above
	 * IMPORTANT: Net drivers triggering this, should call 'UNetDriver::FlushHandler' shortly afterwards, to minimize packet buffering
	 * NOTE: Connectionless packets are unreliable.
	 *
	 * @param Address		The address the packet was received from (format is abstract, determined by active net driver)
	 * @param Packet		The packet data to be processed
	 * @param CountBytes	The size of the packet data in bytes
	 * @return				Returns the final packet
	 */
	virtual const ProcessedPacket IncomingConnectionless(FString Address, uint8* Packet, int32 CountBytes);

	/**
	 * Processes outgoing packets without a UNetConnection, in the same manner as 'Outgoing' above
	 * NOTE: Connectionless packets are unreliable.
	 *
	 * @param Address		The address the packet is being sent to (format is abstract, determined by active net driver)
	 * @param Packet		The packet data to be processed
	 * @param CountBits		The size of the packet data in bits
	 * @return				Returns the final packet
	 */
	virtual const ProcessedPacket OutgoingConnectionless(FString Address, uint8* Packet, int32 CountBits);


	/**
	 * Triggered when a child HandlerComponen has been initialized
	 */
	void HandlerComponentInitialized();

	/**
	 * Queue's a packet to be sent when the handler is ticked
	 * NOTE: Unimplemented.
	 *
	 * @param PacketToQueue		The packet to be queued
	 */
	void QueuePacketForSending(BufferedPacket* PacketToQueue);

	/**
	 * Gets a packet from the buffered packet queue for sending
	 *
	 * @return		The packet to be sent, or nullptr if none are to be sent
	 */
	BufferedPacket* GetQueuedPacket();

	/**
	 * Gets a packet from the buffered connectionless packet queue for sending
	 *
	 * @return		The packet to be sent, or nullptr if none are to be sent
	 */
	BufferedPacket* GetQueuedConnectionlessPacket();

	/**
	 * Gets the combined packet/protocol overhead from all handlers, for reserving space in the parent connections packets
	 *
	 * @return	The combined packet/protocol overhead
	 */
	int32 GetTotalPacketOverheadBits();


	/**
	 * Sets whether or not outgoing packets should bypass this handler - used when raw packet sends are necessary
	 * (such as for the stateless handshake)
	 *
	 * @param bInEnabled	Whether or not raw sends are enabled
	 */
	FORCEINLINE void SetRawSend(bool bInEnabled)
	{
		bRawSend = bInEnabled;
	}

	/**
	 * Whether or not raw packet sends are enabled
	 */
	FORCEINLINE bool GetRawSend()
	{
		return bRawSend;
	}

	FORCEINLINE uint8 GetPacketBitAlignment()
	{
		return PacketBitAlignment;
	}


private:
	/**
	 * Set state of handler
	 *
	 * @param InState	The new state for the handler
	 */
	void SetState(Handler::State InState);

	/**
	 * Called when net send/receive functions are triggered, when the handler is still uninitialized - to set a valid initial state
	 */
	void UpdateInitialState();

	/**
	 * Called when handler is finished initializing
	 */
	void HandlerInitialized();

	/**
	 * Replaces IncomingPacket with all unread data from ReplacementPacket
	 *
	 * @param ReplacementPacket		The packet whose unread data should replace IncomingPacket
	 */
	void ReplaceIncomingPacket(FBitReader& ReplacementPacket);

	/**
	 * Takes a Packet whose position is not at bit 0, and shifts/aligns all packet data to place the current bit at position 0
	 *
	 * @param Packet	The packet to realign
	 */
	void RealignPacket(FBitReader& Packet);


public:
	/** Mode of the handler, Client or Server */
	Handler::Mode Mode;

	/** Time, updated by Tick */
	float Time;


private:
	/** Used for packing outgoing packets */
	FBitWriter OutgoingPacket;

	/** Used for unpacking incoming packets */
	FBitReader IncomingPacket;

	/** The HandlerComponent pipeline, for processing incoming/outgoing packets */
	TArray<TSharedPtr<HandlerComponent>> HandlerComponents;

	/** The overall packet bit alignment (0-7), for all HandlerComponents - used to determine the bit-size of incoming packets/ */
	uint8 PacketBitAlignment;

	/** State of the handler */
	Handler::State State;
	
	/** Packets that are buffered while HandlerComponents are being initialized */
	TArray<BufferedPacket*> BufferedPackets;

	/** Packets that are queued to be sent when handler is ticked */
	TQueue<BufferedPacket*> QueuedPackets;

	/** Packets that are buffered while HandlerComponents are being initialized */
	TArray<BufferedPacket*> BufferedConnectionlessPackets;

	/** Packets that are queued to be sent when handler is ticked */
	TQueue<BufferedPacket*> QueuedConnectionlessPackets;

	/** Reliability Handler Component */
	// @todo #JohnB: Deprecate?
	TSharedPtr<ReliabilityHandlerComponent> ReliabilityComponent;

	/** Whether or not outgoing packets bypass the handler */
	bool bRawSend;
};

/**
 * This class appends or modifies incoming and outgoing packets on a connection
 */
class PACKETHANDLER_API HandlerComponent
{
public:
	/**
	 * Base constructor
	 */
	HandlerComponent();

	/**
	 * Base destructor
	 */
	virtual ~HandlerComponent()
	{
	}

	/**
	 * Returns whether this handler is currently active
	 */
	virtual bool IsActive() const;

	/**
	 * Return whether this handler is valid
	 */
	virtual bool IsValid() const PURE_VIRTUAL(Handler::Component::IsValid, return false;);

	/**
	 * Returns whether this handler is initialized
	 */
	bool IsInitialized() const;

	/**
	 * Returns whether this handler requires a tick every frame
	 */
	bool DoesTick() const;


	/**
	 * Handles incoming packets
	 *
	 * @param Packet	The packet to be handled
	 */
	virtual void Incoming(FBitReader& Packet) PURE_VIRTUAL(HandlerComponent::Incoming,);

	/**
	 * Handles any outgoing packets
	 *
	 * @param Packet	The packet to be handled
	 */
	virtual void Outgoing(FBitWriter& Packet) PURE_VIRTUAL(HandlerComponent::Outgoing,);

	/**
	 * Handles incoming packets not associated with a UNetConnection
	 *
	 * @param Address	The address the packet was received from (format is abstract, determined by active net driver)
	 * @param Packet	The packet to be handled
	 */
	virtual void IncomingConnectionless(FString Address, FBitReader& Packet) PURE_VIRTUAL(HandlerComponent::IncomingConnectionless,);

	/**
	 * Handles any outgoing packets not associated with a UNetConnection
	 *
	 * @param Address	The address the packet is being sent to (format is abstract, determined by active net driver)
	 * @param Packet	The packet to be handled
	 */
	virtual void OutgoingConnectionless(FString Address, FBitWriter& Packet) PURE_VIRTUAL(HandlerComponent::OutgoingConnectionless,);


	/**
	 * Whether or not the Incoming/IncomingConnectionless implementations, support reading Packets that aren't aligned at bit position 0
	 * (i.e. whether or not this handler supports bit-level, rather than byte-level, reads)
	 *
	 * @return	Whether or not the above is supported
	 */
	virtual bool CanReadUnaligned()
	{
		return false;
	}


	/**
	 * Initialization functionality should be placed here
	 */
	virtual void Initialize() PURE_VIRTUAL(HandlerComponent::Initialize,);

	/**
	 * Tick functionality should be placed here
	 */
	virtual void Tick(float DeltaTime)
	{
	}

	/**
	 * Sets whether this handler is currently active
	 *
	 * @param Active	Whether or not the handled should be active
	 */
	virtual void SetActive(bool Active);

	/**
	 * Returns the amount of packet/protocol overhead expected from this component.
	 *
	 * IMPORTANT: This MUST be accurate, and should represent the worst-case overhead expected from the component.
	 *				If this is inaccurate, packets will randomly fail to send, in rare cases which are extremely hard to trace.
	 *
	 * @return	The worst-case packet overhead for the component
	 */
	virtual int32 GetPacketOverheadBits() PURE_VIRTUAL(Handler::Component::GetPacketOverheadBits, return -1;);

	/**
	 * Determines the bit alignment (0-7) of this component, so the PacketHandler can determine the bit-size of incoming packets,
	 * when they are not byte-aligned.
	 *
	 * NOTE: The return value must be static, determined at compile time. It must not change at runtime.
	 *
	 * @return	Returns the bit alignment of this component
	 */
	virtual uint8 GetBitAlignment() PURE_VIRTUAL(Handler::Component::GetBitAlignment, return 0;);

	/**
	 * Whether or not the changes this component makes to the packet, resets all bit alignment changes to the packet,
	 * from previous components in the chain (with the new packet alignment, matching this components bit alignment).
	 *
	 * NOTE: The return value must be static, determined at compile time. It must not change at runtime.
	 *
	 * @return	Returns whether or not this component resets packet bit alignment.
	 */
	virtual bool DoesResetBitAlignment() PURE_VIRTUAL(Handler::Component::DoesResetBitAlignment, return false;);


protected:
	/**
	 * Sets the state of the handler
	 *
	 * @param State		The new state for the handler
	 */
	void SetState(Handler::Component::State State);

	/**
	 * Should be called when the handler is fully initialized on both remote and local
	 */
	void Initialized();


public:
	/** The manager of the handler, set in initialization */
	PacketHandler* Handler; 

protected:
	/** The state of this handler */
	Handler::Component::State State;

private:
	/** Whether this handler is active, which dictates whether it will receive incoming and outgoing packets. */
	bool bActive;

	/** Whether this handler is fully initialized on both remote and local */
	bool bInitialized;
};

/**
 * PacketHandler Module Interface
 */
class FPacketHandlerComponentModuleInterface : public IModuleInterface
{
public:
	/* Creates an instance of this component */
	virtual TSharedPtr<HandlerComponent> CreateComponentInstance(FString& Options)
		PURE_VIRTUAL(FPacketHandlerModuleInterface::CreateComponentInstance, return TSharedPtr<HandlerComponent>(NULL););
};
