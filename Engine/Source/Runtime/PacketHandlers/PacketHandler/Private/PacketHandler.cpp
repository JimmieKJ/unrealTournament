// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "PacketHandlerPCH.h"

#include "PacketHandler.h"
#include "HandlerComponentFactory.h"
#include "ReliabilityHandlerComponent.h"

// @todo #JohnB: There is quite a lot of inefficient copying of packet data going on.
//					Redo the whole packet parsing/modification pipeline.


IMPLEMENT_MODULE(FPacketHandlerComponentModuleInterface, PacketHandler);

DEFINE_LOG_CATEGORY(PacketHandlerLog);


// @todo #JohnB: For the moment, disable the reliability handler, while it is causing network trouble
#define DISABLE_RELIABILITY_HANDLER 1


/**
 * PacketHandler
 */

PacketHandler::PacketHandler()
	: Mode(Handler::Mode::Client)
	, Time(0.f)
	, OutgoingPacket()
	, IncomingPacket()
	, HandlerComponents()
	, MaxPacketBits(0)
	, State(Handler::State::Uninitialized)
	, BufferedPackets()
	, QueuedPackets()
	, BufferedConnectionlessPackets()
	, QueuedConnectionlessPackets()
	, ReliabilityComponent(nullptr)
	, bRawSend(false)
{
	OutgoingPacket.SetAllowResize(true);
	OutgoingPacket.AllowAppend(true);
}

void PacketHandler::Tick(float DeltaTime)
{
	Time += DeltaTime;

	for (int32 i=0; i<HandlerComponents.Num(); ++i)
	{
		HandlerComponents[i]->Tick(DeltaTime);
	}
}

void PacketHandler::Initialize(Handler::Mode InMode, uint32 InMaxPacketBits, bool bConnectionlessOnly/*=false*/)
{
	Mode = InMode;
	MaxPacketBits = InMaxPacketBits;

	// @todo #JohnB: Redo this, so you don't load from the .ini at all, have it hardcoded elsewhere - do not want this in shipping.

	// Only UNetConnection's will load the .ini components, for now.
	if (!bConnectionlessOnly)
	{
		TArray<FString> Components;

		GConfig->GetArray(TEXT("PacketHandlerComponents"), TEXT("Components"), Components, GEngineIni);

		for (FString CurComponent : Components)
		{
			AddHandler(CurComponent, true);
		}
	}
}

void PacketHandler::InitializeComponents()
{
	if (State == Handler::State::Uninitialized)
	{
		if (HandlerComponents.Num() > 0)
		{
			SetState(Handler::State::InitializingComponents);
		}
		else
		{
			HandlerInitialized();
		}
	}

	// Trigger delayed-initialization for HandlerComponents
	for (int32 i=0; i<HandlerComponents.Num(); i++)
	{
		HandlerComponent& CurComponent = *HandlerComponents[i];

		if (!CurComponent.IsInitialized())
		{
			CurComponent.Initialize();
		}
	}

	// Called early, to ensure that all handlers report a valid reserved packet bits value (triggers an assert if not)
	GetTotalReservedPacketBits();
}

void PacketHandler::AddHandler(TSharedPtr<HandlerComponent> NewHandler, bool bDeferInitialize/*=false*/)
{
	// This is never valid. Can end up silently changing maximum allow packet size, which could cause failure to send packets.
	if (State != Handler::State::Uninitialized)
	{
		LowLevelFatalError(TEXT("Handler added during runtime."));
		return;
	}

	// This should always be fatal, as an unexpectedly missing handler, may break net compatibility with the remote server/client
	if (!NewHandler.IsValid())
	{
		LowLevelFatalError(TEXT("Failed to add handler - invalid instance."));
		return;
	}

	HandlerComponents.Add(NewHandler);
	NewHandler->Handler = this;

	if (!bDeferInitialize)
	{
		NewHandler->Initialize();
	}
}

TSharedPtr<HandlerComponent> PacketHandler::AddHandler(FString ComponentStr, bool bDeferInitialize/*=false*/)
{
	TSharedPtr<HandlerComponent> ReturnVal = nullptr;

	if (!ComponentStr.IsEmpty())
	{
		FString ComponentName;
		FString ComponentOptions;

		for (int32 i=0; i<ComponentStr.Len(); i++)
		{
			TCHAR c = ComponentStr[i];

			// Parsing Options
			if (c == '(')
			{
				// Skip '('
				++i;

				// Parse until end of options
				for (; i<ComponentStr.Len(); i++)
				{
					c = ComponentStr[i];

					// End of options
					if (c == ')')
					{
						break;
					}
					// Append char to options
					else
					{
						ComponentOptions.AppendChar(c);
					}
				}
			}
			// Append char to component name if not whitespace
			else if (c != ' ')
			{
				ComponentName.AppendChar(c);
			}
		}

		if (ComponentName != TEXT("ReliabilityHandlerComponent"))
		{
			int32 FactoryComponentDelim = ComponentName.Find(TEXT("."));

			if (FactoryComponentDelim != INDEX_NONE)
			{
				// Every HandlerComponentFactory type has one instance, loaded as a named singleton
				FString SingletonName = ComponentName.Mid(FactoryComponentDelim + 1) + TEXT("_Singleton");
				UHandlerComponentFactory* Factory = FindObject<UHandlerComponentFactory>(ANY_PACKAGE, *SingletonName);

				if (Factory == nullptr)
				{
					UClass* FactoryClass = StaticLoadClass(UHandlerComponentFactory::StaticClass(), nullptr, *ComponentName);

					if (FactoryClass != nullptr)
					{
						Factory = NewObject<UHandlerComponentFactory>(GetTransientPackage(), FactoryClass, *SingletonName);
					}
				}


				if (Factory != nullptr)
				{
					ReturnVal = Factory->CreateComponentInstance(ComponentOptions);
				}
				else
				{
					UE_LOG(PacketHandlerLog, Warning, TEXT("Unable to load HandlerComponent factory: %s"), *ComponentName);
				}
			}
			// @todo #JohnB: Deprecate non-factory components eventually
			else
			{
				TSharedPtr<IModuleInterface> Interface = FModuleManager::Get().LoadModule(FName(*ComponentName));

				if (Interface.IsValid())
				{
					TSharedPtr<FPacketHandlerComponentModuleInterface> PacketHandlerInterface =
						StaticCastSharedPtr<FPacketHandlerComponentModuleInterface>(Interface);

					if (PacketHandlerInterface.IsValid())
					{
						ReturnVal = PacketHandlerInterface->CreateComponentInstance(ComponentOptions);
					}
				}
				else
				{
					UE_LOG(PacketHandlerLog, Warning, TEXT("Unable to Load Module: %s"), *ComponentName);
				}
			}


			if (ReturnVal.IsValid())
			{
				UE_LOG(PacketHandlerLog, Log, TEXT("Loaded PacketHandler component: %s (%s)"), *ComponentName,
						*ComponentOptions);

				AddHandler(ReturnVal, bDeferInitialize);
			}
		}
		else
		{
			UE_LOG(PacketHandlerLog, Warning, TEXT("PacketHandlerComponent 'ReliabilityHandlerComponent' is internal-only."));
		}
	}

	return ReturnVal;
}

void PacketHandler::IncomingHigh(FBitReader& Reader)
{
	// @todo #JohnB
}

void PacketHandler::OutgoingHigh(FBitWriter& Writer)
{
	// @todo #JohnB
}

const ProcessedPacket PacketHandler::Incoming_Internal(uint8* Packet, int32 CountBytes, bool bConnectionless, FString Address)
{
	// @todo #JohnB: Try to optimize this function more, seeing as it will be a common codepath DoS attacks pass through

	// @todo #JohnB: Clean up returns.

	int32 CountBits = CountBytes * 8;
	bool bError = false;

	if (HandlerComponents.Num() > 0)
	{
		uint8 LastByte = Packet[CountBytes - 1];

		if (LastByte != 0)
		{
			CountBits--;

			while (!(LastByte & 0x80))
			{
				LastByte *= 2;
				CountBits--;
			}
		}
		else
		{
			bError = true;

#if !UE_BUILD_SHIPPING
			UE_LOG(PacketHandlerLog, Error, TEXT("PacketHandler parsing packet with zero's in last byte."));
#endif
		}
	}


	if (!bError)
	{
		FBitReader ProcessedPacketReader(Packet, CountBits);

		if (State == Handler::State::Uninitialized)
		{
			UpdateInitialState();
		}


		for (int32 i=HandlerComponents.Num() - 1; i>=0; --i)
		{
			HandlerComponent& CurComponent = *HandlerComponents[i];

			if (CurComponent.IsActive() && !ProcessedPacketReader.IsError() && ProcessedPacketReader.GetBitsLeft() > 0)
			{
				// Realign the packet, so the packet data starts at position 0, if necessary
				if (ProcessedPacketReader.GetPosBits() != 0 && !CurComponent.CanReadUnaligned())
				{
					RealignPacket(ProcessedPacketReader);
				}

				if (bConnectionless)
				{
					CurComponent.IncomingConnectionless(Address, ProcessedPacketReader);
				}
				else
				{
					CurComponent.Incoming(ProcessedPacketReader);
				}
			}
		}

		if (!ProcessedPacketReader.IsError())
		{
			ReplaceIncomingPacket(ProcessedPacketReader);

			return ProcessedPacket(IncomingPacket.GetData(), IncomingPacket.GetBitsLeft());
		}
		else
		{
			return ProcessedPacket();
		}
	}
	else
	{
		return ProcessedPacket();
	}
}

const ProcessedPacket PacketHandler::Outgoing_Internal(uint8* Packet, int32 CountBits, bool bConnectionless, FString Address)
{
	if (!bRawSend)
	{
		OutgoingPacket.Reset();

		if (State == Handler::State::Uninitialized)
		{
			UpdateInitialState();
		}


		if (State == Handler::State::Initialized)
		{
			OutgoingPacket.SerializeBits(Packet, CountBits);


			if (!bConnectionless)
			{
				// Queue packet for resending before handling
				if (CountBits > 0 && ReliabilityComponent.IsValid())
				{
					ReliabilityComponent->QueuePacketForResending(Packet, CountBits);
				}
			}

			for (int32 i=0; i<HandlerComponents.Num() && !OutgoingPacket.IsError(); ++i)
			{
				HandlerComponent& CurComponent = *HandlerComponents[i];

				if (CurComponent.IsActive())
				{
					if (OutgoingPacket.GetNumBits() <= CurComponent.MaxOutgoingBits)
					{
						if (bConnectionless)
						{
							CurComponent.OutgoingConnectionless(Address, OutgoingPacket);
						}
						else
						{
							CurComponent.Outgoing(OutgoingPacket);
						}
					}
					else
					{
						OutgoingPacket.SetError();

						UE_LOG(PacketHandlerLog, Error, TEXT("Packet exceeded HandlerComponents 'MaxOutgoingBits' value: %i vs %i"),
								OutgoingPacket.GetNumBits(), CurComponent.MaxOutgoingBits);

						break;
					}
				}
			}

			// Add a termination bit, the same as the UNetConnection code does, if appropriate
			if (HandlerComponents.Num() > 0)
			{
				OutgoingPacket.WriteBit(1);
			}
		}
		// Buffer any packets being sent from game code until processors are initialized
		else if (State == Handler::State::InitializingComponents && CountBits > 0)
		{
			if (bConnectionless)
			{
				BufferedConnectionlessPackets.Add(new BufferedPacket(Address, Packet, CountBits));
			}
			else
			{
				BufferedPackets.Add(new BufferedPacket(Packet, CountBits));
			}

			Packet = nullptr;
			CountBits = 0;
		}

		// @todo #JohnB: Tidy up return code
		if (!OutgoingPacket.IsError())
		{
			return ProcessedPacket(OutgoingPacket.GetData(), OutgoingPacket.GetNumBits());
		}
		else
		{
			return ProcessedPacket(Packet, CountBits);
		}
	}
	else
	{
		return ProcessedPacket(Packet, CountBits);
	}
}

void PacketHandler::ReplaceIncomingPacket(FBitReader& ReplacementPacket)
{
	if (ReplacementPacket.GetPosBits() == 0 || ReplacementPacket.GetBitsLeft() == 0)
	{
		IncomingPacket = ReplacementPacket;
	}
	else
	{
		// @todo #JohnB: Make this directly adjust and write into IncomingPacket's buffer, instead of copying - very inefficient
		TArray<uint8> NewPacketData;
		int32 NewPacketSizeBits = ReplacementPacket.GetBitsLeft();

		NewPacketData.AddUninitialized(ReplacementPacket.GetBytesLeft());
		NewPacketData[NewPacketData.Num()-1] = 0;

		ReplacementPacket.SerializeBits(NewPacketData.GetData(), NewPacketSizeBits);


		FBitReader NewPacket(NewPacketData.GetData(), NewPacketSizeBits);

		IncomingPacket = NewPacket;
	}
}

void PacketHandler::RealignPacket(FBitReader& Packet)
{
	if (Packet.GetPosBits() != 0)
	{
		uint32 BitsLeft = Packet.GetBitsLeft();

		if (BitsLeft > 0)
		{
			// @todo #JohnB: Based on above - when you optimize above, optimize this too
			TArray<uint8> NewPacketData;
			int32 NewPacketSizeBits = BitsLeft;

			NewPacketData.AddUninitialized(Packet.GetBytesLeft());
			NewPacketData[NewPacketData.Num()-1] = 0;

			Packet.SerializeBits(NewPacketData.GetData(), NewPacketSizeBits);


			FBitReader NewPacket(NewPacketData.GetData(), NewPacketSizeBits);

			Packet = NewPacket;
		}
	}
}

void PacketHandler::SetState(Handler::State InState)
{
	if (InState == State)
	{
		LowLevelFatalError(TEXT("Set new Packet Processor State to the state it is currently in."));
	} 
	else
	{
		State = InState;
	}
}

void PacketHandler::UpdateInitialState()
{
	if (State == Handler::State::Uninitialized)
	{
#if !DISABLE_RELIABILITY_HANDLER
		if (!ReliabilityComponent.IsValid())
		{
			ReliabilityComponent = MakeShareable(new ReliabilityHandlerComponent);
			AddHandler(ReliabilityComponent);
		}

		// Have handlers to initialize other than reliability
		if (HandlerComponents.Num() > 1)
#else
		if (HandlerComponents.Num() > 0)
#endif
		{
			InitializeComponents();
		} 
		else
		{
			HandlerInitialized();
		}
	}
}

void PacketHandler::HandlerInitialized()
{
	// If any buffered packets, add to queue
	for (int32 i=0; i<BufferedPackets.Num(); ++i)
	{
		QueuedPackets.Enqueue(BufferedPackets[i]);
		BufferedPackets[i] = nullptr;
	}

	BufferedPackets.Empty();

	for (int32 i=0; i<BufferedConnectionlessPackets.Num(); ++i)
	{
		QueuedConnectionlessPackets.Enqueue(BufferedConnectionlessPackets[i]);
		BufferedConnectionlessPackets[i] = nullptr;
	}

	BufferedConnectionlessPackets.Empty();

	SetState(Handler::State::Initialized);
}

void PacketHandler::HandlerComponentInitialized()
{
	// Check if all handlers are initialized
	if (State != Handler::State::Initialized)
	{
		for (int32 i=0; i<HandlerComponents.Num(); ++i)
		{
			if (HandlerComponents[i]->IsInitialized() == false)
			{
				return;
			}
		}

		HandlerInitialized();
	}
}

BufferedPacket* PacketHandler::GetQueuedPacket()
{
	BufferedPacket* QueuedPacket = nullptr;

	QueuedPackets.Dequeue(QueuedPacket);

	return QueuedPacket;
}

BufferedPacket* PacketHandler::GetQueuedConnectionlessPacket()
{
	BufferedPacket* QueuedConnectionlessPacket = nullptr;

	QueuedConnectionlessPackets.Dequeue(QueuedConnectionlessPacket);

	return QueuedConnectionlessPacket;
}

void PacketHandler::QueuePacketForSending(BufferedPacket* PacketToQueue)
{
	// @todo #JohnB: Deprecate?
}

int32 PacketHandler::GetTotalReservedPacketBits()
{
	int32 ReturnVal = 0;
	uint32 CurMaxOutgoingBits = MaxPacketBits;

	for (int32 i=HandlerComponents.Num()-1; i>=0; i--)
	{
		HandlerComponent* CurComponent = HandlerComponents[i].Get();
		int32 CurReservedBits = CurComponent->GetReservedPacketBits();

		// Specifying the reserved packet bits is mandatory, even if zero (as accidentally forgetting, leads to hard to trace issues).
		if (CurReservedBits == -1)
		{
			LowLevelFatalError(TEXT("Handler returned invalid 'GetReservedPacketBits' value."));
			continue;
		}


		// Set the maximum Outgoing packet size for the HandlerComponent
		CurComponent->MaxOutgoingBits = CurMaxOutgoingBits;
		CurMaxOutgoingBits -= CurReservedBits;

		ReturnVal += CurReservedBits;
	}


	// Reserve space for the termination bit
	if (HandlerComponents.Num() > 0)
	{
		ReturnVal++;
	}

	return ReturnVal;
}


/**
 * HandlerComponent
 */

HandlerComponent::HandlerComponent()
	: Handler(nullptr)
	, State(Handler::Component::State::UnInitialized)
	, MaxOutgoingBits(0)
	, bActive(false)
	, bInitialized(false)
{
}

bool HandlerComponent::IsActive() const
{
	return bActive;
}

void HandlerComponent::SetActive(bool Active)
{
	bActive = Active;
}

void HandlerComponent::SetState(Handler::Component::State InState)
{
	State = InState;
}

void HandlerComponent::Initialized()
{
	bInitialized = true;
	Handler->HandlerComponentInitialized();
}

bool HandlerComponent::IsInitialized() const
{
	return bInitialized;
}
