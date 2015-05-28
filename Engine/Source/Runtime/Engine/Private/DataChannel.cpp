// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DataChannel.cpp: Unreal datachannel implementation.
=============================================================================*/

#include "EnginePrivate.h"
#include "Net/DataChannel.h"
#include "Net/DataReplication.h"
#include "Net/NetworkProfiler.h"
#include "Net/UnrealNetwork.h"
#include "Engine/ActorChannel.h"
#include "Engine/ControlChannel.h"
#include "Engine/PackageMapClient.h"

DEFINE_LOG_CATEGORY(LogNet);
DEFINE_LOG_CATEGORY(LogNetPlayerMovement);
DEFINE_LOG_CATEGORY(LogNetTraffic);
DEFINE_LOG_CATEGORY(LogNetDormancy);
DEFINE_LOG_CATEGORY_STATIC(LogNetPartialBunch, Warning, All);

extern FAutoConsoleVariable CVarDoReplicationContextString;

static TAutoConsoleVariable<int32> CVarNetReliableDebug(
	TEXT("net.Reliable.Debug"),
	0,
	TEXT("Print all reliable bunches sent over the network\n")
	TEXT(" 0: no print.\n")
	TEXT(" 1: Print bunches as they are sent.\n")
	TEXT(" 2: Print reliable bunch buffer each net update"),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarNetProcessQueuedBunchesMillisecondLimit(
	TEXT("net.ProcessQueuedBunchesMillisecondLimit"),
	30.0f,
	TEXT("Time threshold for processing queued bunches. If it takes longer than this in a single frame, wait until the next frame to continue processing queued bunches."));

/*-----------------------------------------------------------------------------
	UChannel implementation.
-----------------------------------------------------------------------------*/

UChannel::UChannel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UChannel::Init( UNetConnection* InConnection, int32 InChIndex, bool InOpenedLocally )
{
	// if child connection then use its parent
	if (InConnection->GetUChildConnection() != NULL)
	{
		Connection = ((UChildConnection*)InConnection)->Parent;
	}
	else
	{
		Connection = InConnection;
	}
	ChIndex			= InChIndex;
	OpenedLocally	= InOpenedLocally;
	OpenPacketId	= FPacketIdRange();
}


void UChannel::SetClosingFlag()
{
	Closing = 1;
}


void UChannel::Close()
{
	check(OpenedLocally || ChIndex == 0);		// We are only allowed to close channels that we opened locally (except channel 0, so the server can notify disconnected clients)
	check(Connection->Channels[ChIndex]==this);

	if ( !Closing && ( Connection->State == USOCK_Open || Connection->State == USOCK_Pending ) )
	{
		if ( ChIndex == 0 )
		{
			UE_LOG(LogNet, Log, TEXT("UChannel::Close: Sending CloseBunch. ChIndex == 0. Name: %s"), *GetName());
		}

		UE_LOG(LogNetDormancy, Verbose, TEXT("UChannel::Close: Sending CloseBunch. ChIndex: %d, Name: %s, Dormant: %d"), ChIndex, *GetName(), Dormant );

		// Send a close notify, and wait for ack.
		FOutBunch CloseBunch( this, 1 );

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		CloseBunch.DebugString = FString::Printf(TEXT("%.2f Close: %s"), Connection->Driver->Time, *Describe());
#endif
		check(!CloseBunch.IsError());
		check(CloseBunch.bClose);
		CloseBunch.bReliable = 1;
		CloseBunch.bDormant = Dormant;
		SendBunch( &CloseBunch, 0 );
	}
}

void UChannel::ConditionalCleanUp( const bool bForDestroy )
{
	if ( !IsPendingKill() )
	{
		// CleanUp can return false to signify that we shouldn't mark pending kill quite yet
		// We'll need to call cleanup again later on
		if ( CleanUp( bForDestroy ) )
		{
			MarkPendingKill();
		}
	}
}

bool UChannel::CleanUp( const bool bForDestroy )
{
	checkSlow(Connection != NULL);
	checkSlow(Connection->Channels[ChIndex] == this);

	// if this is the control channel, make sure we properly killed the connection
	if (ChIndex == 0 && !Closing)
	{
		UE_LOG(LogNet, Log, TEXT("UChannel::CleanUp: [%s] [%s] [%s]. ChIndex == 0. Closing connection."), 
			Connection->Driver ? *Connection->Driver->NetDriverName.ToString() : TEXT("NULL"),
			Connection->PlayerController ? *Connection->PlayerController->GetName() : TEXT("NoPC"),
			Connection->OwningActor ? *Connection->OwningActor->GetName() : TEXT("No Owner"));

		Connection->Close();
	}

	// remember sequence number of first non-acked outgoing reliable bunch for this slot
	if (OutRec != NULL && !Connection->InternalAck)
	{
		Connection->PendingOutRec[ChIndex] = OutRec->ChSequence;
		//UE_LOG(LogNetTraffic, Log, TEXT("%i save pending out bunch %i"),ChIndex,Connection->PendingOutRec[ChIndex]);
	}
	// Free any pending incoming and outgoing bunches.
	for (FOutBunch* Out = OutRec, *NextOut; Out != NULL; Out = NextOut)
	{
		NextOut = Out->Next;
		delete Out;
	}
	for (FInBunch* In = InRec, *NextIn; In != NULL; In = NextIn)
	{
		NextIn = In->Next;
		delete In;
	}
	if (InPartialBunch != NULL)
	{
		delete InPartialBunch;
		InPartialBunch = NULL;
	}

	// Remove from connection's channel table.
	verifySlow(Connection->OpenChannels.Remove(this) == 1);
	Connection->Channels[ChIndex] = NULL;
	Connection = NULL;

	return true;
}


void UChannel::BeginDestroy()
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		ConditionalCleanUp( true );
	}
	
	Super::BeginDestroy();
}


void UChannel::ReceivedAcks()
{
	check(Connection->Channels[ChIndex]==this);

	/*
	// Verify in sequence.
	for( FOutBunch* Out=OutRec; Out && Out->Next; Out=Out->Next )
		check(Out->Next->ChSequence>Out->ChSequence);
	*/

	// Release all acknowledged outgoing queued bunches.
	bool DoClose = 0;
	while( OutRec && OutRec->ReceivedAck )
	{
		if (OutRec->bOpen)
		{
			bool OpenFinished = true;
			if (OutRec->bPartial)
			{
				// Partial open bunches: check that all open bunches have been ACKd before trashing them
				FOutBunch*	OpenBunch = OutRec;
				while (OpenBunch)
				{
					UE_LOG(LogNet, VeryVerbose, TEXT("   Channel %i open partials %d ackd %d final %d "), ChIndex, OpenBunch->PacketId, OpenBunch->ReceivedAck, OpenBunch->bPartialFinal);

					if (!OpenBunch->ReceivedAck)
					{
						OpenFinished = false;
						break;
					}
					if(OpenBunch->bPartialFinal)
					{
						break;
					}

					OpenBunch = OpenBunch->Next;
				}
			}
			if (OpenFinished)
			{
				UE_LOG(LogNet, VeryVerbose, TEXT("Channel %i is fully acked. PacketID: %d"), ChIndex, OutRec->PacketId );
				OpenAcked = 1;
			}
			else
			{
				// Don't delete this bunch yet until all open bunches are Ackd.
				break;
			}
		}

		DoClose = DoClose || !!OutRec->bClose;
		FOutBunch* Release = OutRec;
		OutRec = OutRec->Next;
		delete Release;
		NumOutRec--;
	}

	// If a close has been acknowledged in sequence, we're done.
	if( DoClose || (OpenTemporary && OpenAcked) )
	{
		UE_LOG(LogNetDormancy, Verbose, TEXT("Channel[%d] '%s' DoClose. Dormant: %d"), ChIndex, *Describe(), Dormant );		

		check(!OutRec);
		ConditionalCleanUp();
	}

}

void UChannel::Tick()
{
	checkSlow(Connection->Channels[ChIndex]==this);
	if (bPendingDormancy && ReadyForDormancy())
	{
		BecomeDormant();
	}
}


void UChannel::AssertInSequenced()
{
#if DO_CHECK
	// Verify that buffer is in order with no duplicates.
	for( FInBunch* In=InRec; In && In->Next; In=In->Next )
		check(In->Next->ChSequence>In->ChSequence);
#endif
}


bool UChannel::ReceivedSequencedBunch( FInBunch& Bunch )
{
	// Handle a regular bunch.
	if ( !Closing )
	{
		ReceivedBunch( Bunch );
	}

	// We have fully received the bunch, so process it.
	if( Bunch.bClose )
	{
		Dormant = Bunch.bDormant;

		// Handle a close-notify.
		if( InRec )
		{
			ensureMsgf(false, TEXT("Close Anomaly %i / %i"), Bunch.ChSequence, InRec->ChSequence );
		}

		if ( ChIndex == 0 )
		{
			UE_LOG(LogNet, Log, TEXT("UChannel::ReceivedSequencedBunch: Bunch.bClose == true. ChIndex == 0. Calling ConditionalCleanUp.") );
		}

		UE_LOG(LogNetTraffic, Log, TEXT("UChannel::ReceivedSequencedBunch: Bunch.bClose == true. Calling ConditionalCleanUp. ChIndex: %i"), ChIndex );

		ConditionalCleanUp();
		return 1;
	}
	return 0;
}

void UChannel::ReceivedRawBunch( FInBunch & Bunch, bool & bOutSkipAck )
{
	// Immediately consume the NetGUID portion of this bunch, regardless if it is partial or reliable.
	if ( Bunch.bHasGUIDs )
	{
		Cast<UPackageMapClient>( Connection->PackageMap )->ReceiveNetGUIDBunch( Bunch );

		if ( Bunch.IsError() )
		{
			UE_LOG( LogNetTraffic, Error, TEXT( "UChannel::ReceivedRawBunch: Bunch.IsError() after ReceiveNetGUIDBunch. ChIndex: %i" ), ChIndex );
			return;
		}
	}

	check(Connection->Channels[ChIndex]==this);

	if ( Bunch.bReliable && Bunch.ChSequence != Connection->InReliable[ChIndex] + 1 )
	{
		// We shouldn't hit this path on 100% reliable connections
		check( !Connection->InternalAck );
		// If this bunch has a dependency on a previous unreceived bunch, buffer it.
		checkSlow(!Bunch.bOpen);

		// Verify that UConnection::ReceivedPacket has passed us a valid bunch.
		check(Bunch.ChSequence>Connection->InReliable[ChIndex]);

		// Find the place for this item, sorted in sequence.
		UE_LOG(LogNetTraffic, Log, TEXT("      Queuing bunch with unreceived dependency: %d / %d"), Bunch.ChSequence, Connection->InReliable[ChIndex]+1 );
		FInBunch** InPtr;
		for( InPtr=&InRec; *InPtr; InPtr=&(*InPtr)->Next )
		{
			if( Bunch.ChSequence==(*InPtr)->ChSequence )
			{
				// Already queued.
				return;
			}
			else if( Bunch.ChSequence<(*InPtr)->ChSequence )
			{
				// Stick before this one.
				break;
			}
		}
		FInBunch* New = new FInBunch(Bunch);
		New->Next     = *InPtr;
		*InPtr        = New;
		NumInRec++;

		if ( NumInRec >= RELIABLE_BUFFER )
		{
			Bunch.SetError();
			UE_LOG( LogNetTraffic, Error, TEXT( "UChannel::ReceivedRawBunch: Too many reliable messages queued up" ) );
			return;
		}

		checkSlow(NumInRec<=RELIABLE_BUFFER);
		//AssertInSequenced();
	}
	else
	{
		bool bDeleted = ReceivedNextBunch( Bunch, bOutSkipAck );

		if ( Bunch.IsError() )
		{
			UE_LOG( LogNetTraffic, Error, TEXT( "UChannel::ReceivedRawBunch: Bunch.IsError() after ReceivedNextBunch 1" ) );
			return;
		}

		if (bDeleted)
		{
			return;
		}
		
		// Dispatch any waiting bunches.
		while( InRec )
		{
			// We shouldn't hit this path on 100% reliable connections
			check( !Connection->InternalAck );

			if( InRec->ChSequence!=Connection->InReliable[ChIndex]+1 )
				break;
			UE_LOG(LogNetTraffic, Log, TEXT("      Channel %d Unleashing queued bunch"), ChIndex );
			FInBunch* Release = InRec;
			InRec = InRec->Next;
			NumInRec--;
			
			// Just keep a local copy of the bSkipAck flag, since these have already been acked and it doesn't make sense on this context
			// Definitely want to warn when this happens, since it's really not possible
			bool bLocalSkipAck = false;

			bDeleted = ReceivedNextBunch( *Release, bLocalSkipAck );

			if ( bLocalSkipAck )
			{
				UE_LOG( LogNetTraffic, Warning, TEXT( "UChannel::ReceivedRawBunch: bLocalSkipAck == true for already acked packet" ) );
			}

			if ( Bunch.IsError() )
			{
				UE_LOG( LogNetTraffic, Error, TEXT( "UChannel::ReceivedRawBunch: Bunch.IsError() after ReceivedNextBunch 2" ) );
				return;
			}

			delete Release;
			if (bDeleted)
			{
				return;
			}
			//AssertInSequenced();
		}
	}
}

bool UChannel::ReceivedNextBunch( FInBunch & Bunch, bool & bOutSkipAck )
{
	// We received the next bunch. Basically at this point:
	//	-We know this is in order if reliable
	//	-We dont know if this is partial or not
	// If its not a partial bunch, of it completes a partial bunch, we can call ReceivedSequencedBunch to actually handle it
	
	// Note this bunch's retirement.
	if ( Bunch.bReliable )
	{
		// Reliables should be ordered properly at this point
		check( Bunch.ChSequence == Connection->InReliable[Bunch.ChIndex] + 1 );

		Connection->InReliable[Bunch.ChIndex] = Bunch.ChSequence;
	}

	FInBunch* HandleBunch = &Bunch;
	if (Bunch.bPartial)
	{
		HandleBunch = NULL;
		if (Bunch.bPartialInitial)
		{
			// Create new InPartialBunch if this is the initial bunch of a new sequence.

			if (InPartialBunch != NULL)
			{
				if (!InPartialBunch->bPartialFinal)
				{
					if ( InPartialBunch->bReliable )
					{
						check( !Bunch.bReliable );		// FIXME: Disconnect client in this case
						UE_LOG(LogNetPartialBunch, Log, TEXT( "Unreliable partial trying to destroy reliable partial 1") );
						bOutSkipAck = true;
						return false;
					}

					// We didn't complete the last partial bunch - this isn't fatal since they can be unreliable, but may want to log it.
					UE_LOG(LogNetPartialBunch, Verbose, TEXT("Incomplete partial bunch. Channel: %d ChSequence: %d"), InPartialBunch->ChIndex, InPartialBunch->ChSequence);
				}
				
				delete InPartialBunch;
				InPartialBunch = NULL;
			}

			InPartialBunch = new FInBunch(Bunch, false);
			if ( !Bunch.bHasGUIDs && Bunch.GetBitsLeft() > 0 )
			{
				check( Bunch.GetBitsLeft() % 8 == 0); // Starting partial bunches should always be byte aligned.

				InPartialBunch->AppendDataFromChecked( Bunch.GetDataPosChecked(), Bunch.GetBitsLeft() );
				UE_LOG(LogNetPartialBunch, Verbose, TEXT("Received New partial bunch. Channel: %d ChSequence: %d. NumBits Total: %d. NumBits LefT: %d.  Reliable: %d"), InPartialBunch->ChIndex, InPartialBunch->ChSequence, InPartialBunch->GetNumBits(),  Bunch.GetBytesLeft(), Bunch.bReliable);
			}
			else
			{
				UE_LOG(LogNetPartialBunch, Verbose, TEXT("Received New partial bunch. It only contained NetGUIDs.  Channel: %d ChSequence: %d. Reliable: %d"), InPartialBunch->ChIndex, InPartialBunch->ChSequence, Bunch.bReliable);
			}
		}
		else
		{
			// Merge in next partial bunch to InPartialBunch if:
			//	-We have a valid InPartialBunch
			//	-The current InPartialBunch wasn't already complete
			//  -ChSequence is next in partial sequence
			//	-Reliability flag matches

			bool bSequenceMatches = false;
			if (InPartialBunch)
			{
				const bool bReliableSequencesMatches = Bunch.ChSequence == InPartialBunch->ChSequence + 1;
				const bool bUnreliableSequenceMatches = bReliableSequencesMatches || (Bunch.ChSequence == InPartialBunch->ChSequence);

				// Unreliable partial bunches use the packet sequence, and since we can merge multiple bunches into a single packet,
				// it's perfectly legal for the ChSequence to match in this case.
				// Reliable partial bunches must be in consecutive order though
				bSequenceMatches = InPartialBunch->bReliable ? bReliableSequencesMatches : bUnreliableSequenceMatches;
			}

			if ( InPartialBunch && !InPartialBunch->bPartialFinal && bSequenceMatches && InPartialBunch->bReliable == Bunch.bReliable )
			{
				// Merge.
				UE_LOG(LogNetPartialBunch, Verbose, TEXT("Merging Partial Bunch: %d Bytes"), Bunch.GetBytesLeft() );

				if ( !Bunch.bHasGUIDs && Bunch.GetBitsLeft() > 0 )
				{
					InPartialBunch->AppendDataFromChecked( Bunch.GetDataPosChecked(), Bunch.GetBitsLeft() );
				}

				// Only the final partial bunch should ever be non byte aligned. This is enforced during partial bunch creation
				// This is to ensure fast copies/appending of partial bunches. The final partial bunch may be non byte aligned.
				check( Bunch.bHasGUIDs || Bunch.bPartialFinal || Bunch.GetBitsLeft() % 8 == 0 );

				// Advance the sequence of the current partial bunch so we know what to expect next
				InPartialBunch->ChSequence = Bunch.ChSequence;

				if (Bunch.bPartialFinal)
				{
					if (UE_LOG_ACTIVE(LogNetPartialBunch,Verbose)) // Don't want to call appMemcrc unless we need to
					{
						UE_LOG(LogNetPartialBunch, Verbose, TEXT("Completed Partial Bunch: Channel: %d ChSequence: %d. Num: %d Rel: %d CRC 0x%X"), InPartialBunch->ChIndex, InPartialBunch->ChSequence, InPartialBunch->GetNumBits(), Bunch.bReliable, FCrc::MemCrc_DEPRECATED(InPartialBunch->GetData(), InPartialBunch->GetNumBytes()));
					}

					check( !Bunch.bHasGUIDs );		// Shouldn't have these, they only go in initial partial export bunches

					HandleBunch = InPartialBunch;

					InPartialBunch->bPartialFinal			= true;
					InPartialBunch->bClose					= Bunch.bClose;
					InPartialBunch->bDormant				= Bunch.bDormant;
					InPartialBunch->bHasMustBeMappedGUIDs	= Bunch.bHasMustBeMappedGUIDs;
				}
				else
				{
					if (UE_LOG_ACTIVE(LogNetPartialBunch,Verbose)) // Don't want to call appMemcrc unless we need to
					{
						UE_LOG(LogNetPartialBunch, Verbose, TEXT("Received Partial Bunch: Channel: %d ChSequence: %d. Num: %d Rel: %d CRC 0x%X"), InPartialBunch->ChIndex, InPartialBunch->ChSequence, InPartialBunch->GetNumBits(), Bunch.bReliable, FCrc::MemCrc_DEPRECATED(InPartialBunch->GetData(), InPartialBunch->GetNumBytes()));
					}
				}
			}
			else
			{
				// Merge problem - delete InPartialBunch. This is mainly so that in the unlikely chance that ChSequence wraps around, we wont merge two completely separate partial bunches.

				// We shouldn't hit this path on 100% reliable connections
				check( !Connection->InternalAck );
				
				bOutSkipAck = true;	// Don't ack the packet, since we didn't process the bunch

				if ( InPartialBunch && InPartialBunch->bReliable )
				{
					check( !Bunch.bReliable );		// FIXME: Disconnect client in this case
					UE_LOG( LogNetPartialBunch, Log, TEXT( "Unreliable partial trying to destroy reliable partial 2" ) );
					return false;
				}

				if (InPartialBunch)
				{
					delete InPartialBunch;
					InPartialBunch = NULL;
				}

				if (UE_LOG_ACTIVE(LogNetPartialBunch,Verbose)) // Don't want to call appMemcrc unless we need to
				{
					UE_LOG(LogNetPartialBunch, Verbose, TEXT("Received Partial Bunch Out of Sequence: Channel: %d ChSequence: %d/%d. Num: %d Rel: %d CRC 0x%X"), InPartialBunch->ChIndex, InPartialBunch->ChSequence, Bunch.ChSequence, InPartialBunch->GetNumBits(), Bunch.bReliable, FCrc::MemCrc_DEPRECATED(InPartialBunch->GetData(), InPartialBunch->GetNumBytes()));
				}
			}
		}

		// Fairly large number, and probably a bad idea to even have a bunch this size, but want to be safe for now and not throw out legitimate data
		static const int32 MAX_CONSTRUCTED_PARTIAL_SIZE_IN_BYTES = 1024 * 64;		

		if ( !Connection->InternalAck && InPartialBunch != NULL && InPartialBunch->GetNumBytes() > MAX_CONSTRUCTED_PARTIAL_SIZE_IN_BYTES )
		{
			UE_LOG( LogNetPartialBunch, Error, TEXT( "Final partial bunch too large" ) );
			Bunch.SetError();
			return false;
		}
	}

	if ( HandleBunch != NULL )
	{
		if ( HandleBunch->bOpen )
		{
			if ( ChType != CHTYPE_Voice )	// Voice channels can open from both side simultaneously, so ignore this logic until we resolve this
			{
				check( !OpenedLocally );					// If we opened the channel, we shouldn't be receiving bOpen commands from the other side
				check( OpenPacketId.First == INDEX_NONE );	// This should be the first and only assignment of the packet range (we should only receive one bOpen bunch)
				check( OpenPacketId.Last == INDEX_NONE );	// This should be the first and only assignment of the packet range (we should only receive one bOpen bunch)
			}

			// Remember the range.
			// In the case of a non partial, HandleBunch == Bunch
			// In the case of a partial, HandleBunch should == InPartialBunch, and Bunch should be the last bunch.
			OpenPacketId.First = HandleBunch->PacketId;
			OpenPacketId.Last = Bunch.PacketId;
			OpenAcked = true;

			UE_LOG( LogNetTraffic, Verbose, TEXT( "ReceivedNextBunch: Channel now fully open. ChIndex: %i, OpenPacketId.First: %i, OpenPacketId.Last: %i" ), ChIndex, OpenPacketId.First, OpenPacketId.Last );
		}

		if ( ChType != CHTYPE_Voice )	// Voice channels can open from both side simultaneously, so ignore this logic until we resolve this
		{
			// Don't process any packets until we've fully opened this channel 
			// (unless we opened it locally, in which case it's safe to process packets)
			if ( !OpenedLocally && !OpenAcked )
			{
				// If we receive a reliable at this point, this means reliables are out of order, which shouldn't be possible
				check( !HandleBunch->bReliable );
				check( !Connection->InternalAck );	// Shouldn't be possible for 100% reliable connections

				// Don't ack this packet (since we won't process all of it)
				bOutSkipAck = true;

				UE_LOG( LogNetTraffic, Warning, TEXT( "ReceivedNextBunch: Skipping bunch since channel isn't fully open. ChIndex: %i" ), ChIndex );
				return false;
			}

			// At this point, we should have the open packet range
			// This is because if we opened the channel locally, we set it immediately when we sent the first bOpen bunch
			// If we opened it from a remote connection, then we shouldn't be processing any packets until it's fully opened (which is handled above)
			check( OpenPacketId.First != INDEX_NONE );
			check( OpenPacketId.Last != INDEX_NONE );
		}

		// Receive it in sequence.
		return ReceivedSequencedBunch( *HandleBunch );
	}

	return false;
}

void UChannel::AppendExportBunches( TArray<FOutBunch *>& OutExportBunches )
{
	UPackageMapClient * PackageMapClient = CastChecked< UPackageMapClient >( Connection->PackageMap );

	// Let the package map add any outgoing bunches it needs to send
	PackageMapClient->AppendExportBunches( OutExportBunches );
}

void UChannel::AppendMustBeMappedGuids( FOutBunch* Bunch )
{
	UPackageMapClient * PackageMapClient = CastChecked< UPackageMapClient >( Connection->PackageMap );

	TArray< FNetworkGUID >& MustBeMappedGuidsInLastBunch = PackageMapClient->GetMustBeMappedGuidsInLastBunch();

	if ( MustBeMappedGuidsInLastBunch.Num() > 0 )
	{
		// Rewrite the bunch with the unique guids in front
		FOutBunch TempBunch( *Bunch );

		Bunch->Reset();

		// Write all the guids out
		uint16 NumMustBeMappedGUIDs = MustBeMappedGuidsInLastBunch.Num();
		*Bunch << NumMustBeMappedGUIDs;
		for ( int32 i = 0; i < MustBeMappedGuidsInLastBunch.Num(); i++ )
		{
			*Bunch << MustBeMappedGuidsInLastBunch[i];
		}

		NETWORK_PROFILER(GNetworkProfiler.TrackMustBeMappedGuids(NumMustBeMappedGUIDs, Bunch->GetNumBits()));

		// Append the original bunch data at the end
		Bunch->SerializeBits( TempBunch.GetData(), TempBunch.GetNumBits() );

		Bunch->bHasMustBeMappedGUIDs = 1;

		MustBeMappedGuidsInLastBunch.Empty();
	}
}

void UActorChannel::AppendExportBunches( TArray<FOutBunch *>& OutExportBunches )
{
	Super::AppendExportBunches( OutExportBunches );

	// Let the profiler know about exported GUID bunches
	for (const FOutBunch* ExportBunch : QueuedExportBunches )
	{
		if (ExportBunch != nullptr)
		{
			NETWORK_PROFILER(GNetworkProfiler.TrackExportBunch(ExportBunch->GetNumBits()));
		}
	}

	if ( QueuedExportBunches.Num() )
	{
		OutExportBunches.Append( QueuedExportBunches );
		QueuedExportBunches.Empty();
	}
}

void UActorChannel::AppendMustBeMappedGuids( FOutBunch* Bunch )
{
	if ( QueuedMustBeMappedGuidsInLastBunch.Num() > 0 )
	{
		// Just add our list to the main list on package map so we can re-use the code in UChannel to add them all together
		UPackageMapClient * PackageMapClient = CastChecked< UPackageMapClient >( Connection->PackageMap );

		PackageMapClient->GetMustBeMappedGuidsInLastBunch().Append( QueuedMustBeMappedGuidsInLastBunch );

		QueuedMustBeMappedGuidsInLastBunch.Empty();
	}

	// Actually add them to the bunch
	// NOTE - We do this LAST since we want to capture the append that happened above
	Super::AppendMustBeMappedGuids( Bunch );
}

FPacketIdRange UChannel::SendBunch( FOutBunch* Bunch, bool Merge )
{
	check(!Closing);
	check(Connection->Channels[ChIndex]==this);
	check(!Bunch->IsError());
	check( !Bunch->bHasGUIDs );

	// Set bunch flags.
	if( OpenPacketId.First==INDEX_NONE && OpenedLocally )
	{
		Bunch->bOpen = 1;
		OpenTemporary = !Bunch->bReliable;
	}

	// If channel was opened temporarily, we are never allowed to send reliable packets on it.
	check(!OpenTemporary || !Bunch->bReliable);

	// This is the max number of bits we can have in a single bunch
	const int64 MAX_SINGLE_BUNCH_SIZE_BITS  = Connection->MaxPacket*8-MAX_BUNCH_HEADER_BITS-MAX_PACKET_TRAILER_BITS-MAX_PACKET_HEADER_BITS;

	// Max bytes we'll put in a partial bunch
	const int64 MAX_SINGLE_BUNCH_SIZE_BYTES = MAX_SINGLE_BUNCH_SIZE_BITS / 8;

	// Max bits will put in a partial bunch (byte aligned, we dont want to deal with partial bytes in the partial bunches)
	const int64 MAX_PARTIAL_BUNCH_SIZE_BITS = MAX_SINGLE_BUNCH_SIZE_BYTES * 8;

	TArray<FOutBunch *> OutgoingBunches;

	// Add any export bunches
	AppendExportBunches( OutgoingBunches );

	if ( OutgoingBunches.Num() )
	{
		// Don't merge if we are exporting guid's
		// We can't be for sure if the last bunch has exported guids as well, so this just simplifies things
		Merge = false;
	}

	if ( Connection->Driver->IsServer() )
	{
		// Append any "must be mapped" guids to front of bunch from the packagemap
		AppendMustBeMappedGuids( Bunch );

		if ( Bunch->bHasMustBeMappedGUIDs )
		{
			// We can't merge with this, since we need all the unique static guids in the front
			Merge = false;
		}
	}

	//-----------------------------------------------------
	// Contemplate merging.
	//-----------------------------------------------------
	int32 PreExistingBits = 0;
	FOutBunch* OutBunch = NULL;
	if
	(	Merge
	&&	Connection->LastOut.ChIndex == Bunch->ChIndex
	&&	Connection->AllowMerge
	&&	Connection->LastEnd.GetNumBits()
	&&	Connection->LastEnd.GetNumBits()==Connection->SendBuffer.GetNumBits()
	&&	Connection->LastOut.GetNumBits() + Bunch->GetNumBits() <= MAX_SINGLE_BUNCH_SIZE_BITS )
	{
		// Merge.
		check(!Connection->LastOut.IsError());
		PreExistingBits = Connection->LastOut.GetNumBits();
		Connection->LastOut.SerializeBits( Bunch->GetData(), Bunch->GetNumBits() );
		Connection->LastOut.bReliable |= Bunch->bReliable;
		Connection->LastOut.bOpen     |= Bunch->bOpen;
		Connection->LastOut.bClose    |= Bunch->bClose;
		OutBunch                       = Connection->LastOutBunch;
		Bunch                          = &Connection->LastOut;
		check(!Bunch->IsError());
		Connection->PopLastStart();
		Connection->Driver->OutBunches--;
	}

	//-----------------------------------------------------
	// Possibly split large bunch into list of smaller partial bunches
	//-----------------------------------------------------
	if( Bunch->GetNumBits() > MAX_SINGLE_BUNCH_SIZE_BITS )
	{
		uint8 *data = Bunch->GetData();
		int64 bitsLeft = Bunch->GetNumBits();
		Merge = false;

		while(bitsLeft > 0)
		{
			FOutBunch * PartialBunch = new FOutBunch(this, false);
			int64 bitsThisBunch = FMath::Min<int64>(bitsLeft, MAX_PARTIAL_BUNCH_SIZE_BITS);
			PartialBunch->SerializeBits(data, bitsThisBunch);
			OutgoingBunches.Add(PartialBunch);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			PartialBunch->DebugString = FString::Printf(TEXT("Partial[%d]: %s"), OutgoingBunches.Num(), *Bunch->DebugString);
#endif
		
			bitsLeft -= bitsThisBunch;
			data += (bitsThisBunch >> 3);

			UE_LOG(LogNetPartialBunch, Log, TEXT("	Making partial bunch from content bunch. bitsThisBunch: %d bitsLeft: %d"), bitsThisBunch, bitsLeft );
			
			ensure(bitsLeft == 0 || bitsThisBunch % 8 == 0); // Byte aligned or it was the last bunch
		}
	}
	
	else
	{
		OutgoingBunches.Add(Bunch);
	}

	//-----------------------------------------------------
	// Send all the bunches we need to
	//	Note: this is done all at once. We could queue this up somewhere else before sending to Out.
	//-----------------------------------------------------
	FPacketIdRange PacketIdRange;

	if (Bunch->bReliable && (NumOutRec + OutgoingBunches.Num() >= RELIABLE_BUFFER + Bunch->bClose))
	{
		UE_LOG(LogNetPartialBunch, Warning, TEXT("Channel[%d] - %s. Reliable partial bunch overflows reliable buffer!"), ChIndex, *Describe() );
		UE_LOG(LogNetPartialBunch, Warning, TEXT("   Num OutgoingBunches: %d. NumOutRec: %d"), OutgoingBunches.Num(), NumOutRec );
		PrintReliableBunchBuffer();

		// Bail out, we can't recover from this (without increasing RELIABLE_BUFFER)
		FString ErrorMsg = NSLOCTEXT("NetworkErrors", "ClientReliableBufferOverflow", "Outgoing reliable buffer overflow").ToString();
		FNetControlMessage<NMT_Failure>::Send(Connection, ErrorMsg);
		Connection->FlushNet(true);
		Connection->Close();
	
		return PacketIdRange;
	}

	UE_CLOG((OutgoingBunches.Num() > 1), LogNetPartialBunch, Log, TEXT("Sending %d Bunches. Channel: %d "), OutgoingBunches.Num(), Bunch->ChIndex);
	for( int32 PartialNum = 0; PartialNum < OutgoingBunches.Num(); ++PartialNum)
	{
		FOutBunch * NextBunch = OutgoingBunches[PartialNum];

		NextBunch->bReliable = Bunch->bReliable;
		NextBunch->bOpen = Bunch->bOpen;
		NextBunch->bClose = Bunch->bClose;
		NextBunch->bDormant = Bunch->bDormant;
		NextBunch->ChIndex = Bunch->ChIndex;
		NextBunch->ChType = Bunch->ChType;

		if ( !NextBunch->bHasGUIDs )
		{
			NextBunch->bHasMustBeMappedGUIDs |= Bunch->bHasMustBeMappedGUIDs;
		}

		if (OutgoingBunches.Num() > 1)
		{
			NextBunch->bPartial = 1;
			NextBunch->bPartialInitial = (PartialNum == 0 ? 1: 0);
			NextBunch->bPartialFinal = (PartialNum == OutgoingBunches.Num() - 1 ? 1: 0);
			NextBunch->bOpen &= (PartialNum == 0);											// Only the first bunch should have the bOpen bit set
			NextBunch->bClose = (Bunch->bClose && (OutgoingBunches.Num()-1 == PartialNum)); // Only last bunch should have bClose bit set
		}

		FOutBunch *ThisOutBunch = PrepBunch(NextBunch, OutBunch, Merge); // This handles queuing reliable bunches into the ack list

		if (UE_LOG_ACTIVE(LogNetPartialBunch,Verbose) && (OutgoingBunches.Num() > 1)) // Don't want to call appMemcrc unless we need to
		{
			UE_LOG(LogNetPartialBunch, Verbose, TEXT("	Bunch[%d]: Bytes: %d Bits: %d ChSequence: %d 0x%X"), PartialNum, ThisOutBunch->GetNumBytes(), ThisOutBunch->GetNumBits(), ThisOutBunch->ChSequence, FCrc::MemCrc_DEPRECATED(ThisOutBunch->GetData(), ThisOutBunch->GetNumBytes()));
		}

		// Update Packet Range
		int32 PacketId = SendRawBunch(ThisOutBunch, Merge);
		if (PartialNum == 0)
		{
			PacketIdRange = FPacketIdRange(PacketId);
		}
		else
		{
			PacketIdRange.Last = PacketId;
		}

		// Update channel sequence count.
		Connection->LastOut = *ThisOutBunch;
		Connection->LastEnd	= FBitWriterMark( Connection->SendBuffer );
	}

	// Update open range if necessary
	if (Bunch->bOpen)
	{
		OpenPacketId = PacketIdRange;		
	}


	// Destroy outgoing bunches now that they are sent, except the one that was passed into ::SendBunch
	//	This is because the one passed in ::SendBunch is the responsibility of the caller, the other bunches in OutgoingBunches
	//	were either allocated in this function for partial bunches, or taken from the package map, which expects us to destroy them.
	for (auto It = OutgoingBunches.CreateIterator(); It; ++It)
	{
		FOutBunch *DeleteBunch = *It;
		if (DeleteBunch != Bunch)
			delete DeleteBunch;
	}

	return PacketIdRange;
}

/** This returns a pointer to Bunch, but it may either be a direct pointer, or a pointer to a copied instance of it */

// OUtbunch is a bunch that was new'd by the network system or NULL. It should never be one created on the stack
FOutBunch* UChannel::PrepBunch(FOutBunch* Bunch, FOutBunch* OutBunch, bool Merge)
{
	// Find outgoing bunch index.
	if( Bunch->bReliable )
	{
		// Find spot, which was guaranteed available by FOutBunch constructor.
		if( OutBunch==NULL )
		{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			if (!(NumOutRec<RELIABLE_BUFFER-1+Bunch->bClose))
			{
				UE_LOG(LogNetTraffic, Warning, TEXT("Reliable buffer overflow! %s"), *Describe());
				PrintReliableBunchBuffer();
			}
#else
			check(NumOutRec<RELIABLE_BUFFER-1+Bunch->bClose);
#endif

			Bunch->Next	= NULL;
			Bunch->ChSequence = ++Connection->OutReliable[ChIndex];
			NumOutRec++;
			OutBunch = new FOutBunch(*Bunch);
			FOutBunch** OutLink = &OutRec;
			while(*OutLink) // This was rewritten from a single-line for loop due to compiler complaining about empty body for loops (-Wempty-body)
			{
				OutLink=&(*OutLink)->Next;
			}
			*OutLink = OutBunch;
		}
		else
		{
			Bunch->Next = OutBunch->Next;
			*OutBunch = *Bunch;
		}
		Connection->LastOutBunch = OutBunch;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (CVarNetReliableDebug.GetValueOnGameThread() == 1)
		{
			UE_LOG(LogNetTraffic, Warning, TEXT("%s. Reliable: %s"), *Describe(), *Bunch->DebugString);
		}
		if (CVarNetReliableDebug.GetValueOnGameThread() == 2)
		{
			UE_LOG(LogNetTraffic, Warning, TEXT("%s. Reliable: %s"), *Describe(), *Bunch->DebugString);
			PrintReliableBunchBuffer();
			UE_LOG(LogNetTraffic, Warning, TEXT(""));
		}
#endif

	}
	else
	{
		OutBunch = Bunch;
		Connection->LastOutBunch = NULL;//warning: Complex code, don't mess with this!
	}

	return OutBunch;
}

int32 UChannel::SendRawBunch(FOutBunch* OutBunch, bool Merge)
{
	// Send the raw bunch.
	OutBunch->ReceivedAck = 0;
	int32 PacketId = Connection->SendRawBunch(*OutBunch, Merge);
	if( OpenPacketId.First==INDEX_NONE && OpenedLocally )
		OpenPacketId = FPacketIdRange(PacketId);
	if( OutBunch->bClose )
		SetClosingFlag();

	return PacketId;
}

FString UChannel::Describe()
{
	return FString(TEXT("State=")) + (Closing ? TEXT("closing") : TEXT("open") );
}


int32 UChannel::IsNetReady( bool Saturate )
{
	// If saturation allowed, ignore queued byte count.
	if( NumOutRec>=RELIABLE_BUFFER-1 )
	{
		return 0;
	}
	return Connection->IsNetReady( Saturate );
}


bool UChannel::IsKnownChannelType( int32 Type )
{
	return Type>=0 && Type<CHTYPE_MAX && ChannelClasses[Type];
}


void UChannel::ReceivedNak( int32 NakPacketId )
{
	for( FOutBunch* Out=OutRec; Out; Out=Out->Next )
	{
		// Retransmit reliable bunches in the lost packet.
		if( Out->PacketId==NakPacketId && !Out->ReceivedAck )
		{
			check(Out->bReliable);
			UE_LOG(LogNetTraffic, Log, TEXT("      Channel %i nak); resending %i..."), Out->ChIndex, Out->ChSequence );
			Connection->SendRawBunch( *Out, 0 );
		}
	}
}

void UChannel::PrintReliableBunchBuffer()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	for( FOutBunch* Out=OutRec; Out; Out=Out->Next )
	{
		UE_LOG(LogNetTraffic, Warning, TEXT("Out: %s"), *Out->DebugString );
	}
	UE_LOG(LogNetTraffic, Warning, TEXT("-------------------------\n"));
#endif
}


UClass* UChannel::ChannelClasses[CHTYPE_MAX]={0,0,0,0,0,0,0,0};

/*-----------------------------------------------------------------------------
	UControlChannel implementation.
-----------------------------------------------------------------------------*/

const TCHAR* FNetControlMessageInfo::Names[256];

// control channel message implementation
IMPLEMENT_CONTROL_CHANNEL_MESSAGE(Hello);
IMPLEMENT_CONTROL_CHANNEL_MESSAGE(Welcome);
IMPLEMENT_CONTROL_CHANNEL_MESSAGE(Upgrade);
IMPLEMENT_CONTROL_CHANNEL_MESSAGE(Challenge);
IMPLEMENT_CONTROL_CHANNEL_MESSAGE(Netspeed);
IMPLEMENT_CONTROL_CHANNEL_MESSAGE(Login);
IMPLEMENT_CONTROL_CHANNEL_MESSAGE(Failure);
IMPLEMENT_CONTROL_CHANNEL_MESSAGE(Join);
IMPLEMENT_CONTROL_CHANNEL_MESSAGE(JoinSplit);
IMPLEMENT_CONTROL_CHANNEL_MESSAGE(Skip);
IMPLEMENT_CONTROL_CHANNEL_MESSAGE(Abort);
IMPLEMENT_CONTROL_CHANNEL_MESSAGE(PCSwap);
IMPLEMENT_CONTROL_CHANNEL_MESSAGE(ActorChannelFailure);
IMPLEMENT_CONTROL_CHANNEL_MESSAGE(DebugText);
IMPLEMENT_CONTROL_CHANNEL_MESSAGE(BeaconWelcome);
IMPLEMENT_CONTROL_CHANNEL_MESSAGE(BeaconJoin);
IMPLEMENT_CONTROL_CHANNEL_MESSAGE(BeaconAssignGUID);
IMPLEMENT_CONTROL_CHANNEL_MESSAGE(BeaconNetGUIDAck);

void UControlChannel::Init( UNetConnection* InConnection, int32 InChannelIndex, bool InOpenedLocally )
{
	Super::Init( InConnection, InChannelIndex, InOpenedLocally );

	// If we are opened as a server connection, do the endian checking
	// The client assumes that the data will always have the correct byte order
	if (!InOpenedLocally)
	{
		// Mark this channel as needing endianess determination
		bNeedsEndianInspection = true;
	}
}


bool UControlChannel::CheckEndianess(FInBunch& Bunch)
{
	// Assume the packet is bogus and the connection needs closing
	bool bConnectionOk = false;
	// Get pointers to the raw packet data
	const uint8* HelloMessage = Bunch.GetData();
	// Check for a packet that is big enough to look at (message ID (1 byte) + platform identifier (1 byte))
	if (Bunch.GetNumBytes() >= 2)
	{
		if (HelloMessage[0] == NMT_Hello)
		{
			// Get platform id
			uint8 OtherPlatformIsLittle = HelloMessage[1];
			checkSlow(OtherPlatformIsLittle == !!OtherPlatformIsLittle); // should just be zero or one, we use check slow because we don't want to crash in the wild if this is a bad value.
			uint8 IsLittleEndian = uint8(PLATFORM_LITTLE_ENDIAN);
			check(IsLittleEndian == !!IsLittleEndian); // should only be one or zero

			UE_LOG(LogNet, Log, TEXT("Remote platform little endian=%d"), int32(OtherPlatformIsLittle));
			UE_LOG(LogNet, Log, TEXT("This platform little endian=%d"), int32(IsLittleEndian));
			// Check whether the other platform needs byte swapping by
			// using the value sent in the packet. Note: we still validate it
			if (OtherPlatformIsLittle ^ IsLittleEndian)
			{
				// Client has opposite endianess so swap this bunch
				// and mark the connection as needing byte swapping
				Bunch.SetByteSwapping(true);
				Connection->bNeedsByteSwapping = true;
			}
			else
			{
				// Disable all swapping
				Bunch.SetByteSwapping(false);
				Connection->bNeedsByteSwapping = false;
			}
			// We parsed everything so keep the connection open
			bConnectionOk = true;
			bNeedsEndianInspection = false;
		}
	}
	return bConnectionOk;
}


void UControlChannel::ReceivedBunch( FInBunch& Bunch )
{
	check(!Closing);
	// If this is a new client connection inspect the raw packet for endianess
	if (Connection && bNeedsEndianInspection && !CheckEndianess(Bunch))
	{
		// Send close bunch and shutdown this connection
		UE_LOG(LogNet, Warning, TEXT("UControlChannel::ReceivedBunch: NetConnection::Close() [%s] [%s] [%s] from CheckEndianess(). FAILED. Closing connection."),
			Connection->Driver ? *Connection->Driver->NetDriverName.ToString() : TEXT("NULL"),
			Connection->PlayerController ? *Connection->PlayerController->GetName() : TEXT("NoPC"),
			Connection->OwningActor ? *Connection->OwningActor->GetName() : TEXT("No Owner"));

		Connection->Close();
		return;
	}

	// Process the packet
	while (!Bunch.AtEnd() && Connection != NULL && Connection->State != USOCK_Closed) // if the connection got closed, we don't care about the rest
	{
		uint8 MessageType;
		Bunch << MessageType;
		if (Bunch.IsError())
		{
			break;
		}
		int32 Pos = Bunch.GetPosBits();
		
		// we handle Actor channel failure notifications ourselves
		if (MessageType == NMT_ActorChannelFailure)
		{
			if (Connection->Driver->ServerConnection == NULL)
			{
				UE_LOG(LogNet, Log, TEXT("Server connection received: %s"), FNetControlMessageInfo::GetName(MessageType));
				int32 ChannelIndex;
				FNetControlMessage<NMT_ActorChannelFailure>::Receive(Bunch, ChannelIndex);
				if (ChannelIndex < ARRAY_COUNT(Connection->Channels))
				{
					UActorChannel* ActorChan = Cast<UActorChannel>(Connection->Channels[ChannelIndex]);
					if (ActorChan != NULL && ActorChan->Actor != NULL)
					{
						// if the client failed to initialize the PlayerController channel, the connection is broken
						if (ActorChan->Actor == Connection->PlayerController)
						{
							UE_LOG(LogNet, Warning, TEXT("UControlChannel::ReceivedBunch: NetConnection::Close() [%s] [%s] [%s] from failed to initialize the PlayerController channel. Closing connection."), 
								Connection->Driver ? *Connection->Driver->NetDriverName.ToString() : TEXT("NULL"),
								Connection->PlayerController ? *Connection->PlayerController->GetName() : TEXT("NoPC"),
								Connection->OwningActor ? *Connection->OwningActor->GetName() : TEXT("No Owner"));

							Connection->Close();
						}
						else if (Connection->PlayerController != NULL)
						{
							Connection->PlayerController->NotifyActorChannelFailure(ActorChan);
						}
					}
				}
			}
		}
		else
		{
			// Process control message on client/server connection
			Connection->Driver->Notify->NotifyControlMessage(Connection, MessageType, Bunch);
		}

		// if the message was not handled, eat it ourselves
		if (Pos == Bunch.GetPosBits() && !Bunch.IsError())
		{
			switch (MessageType)
			{
				case NMT_Hello:
					FNetControlMessage<NMT_Hello>::Discard(Bunch);
					break;
				case NMT_Welcome:
					FNetControlMessage<NMT_Welcome>::Discard(Bunch);
					break;
				case NMT_Upgrade:
					FNetControlMessage<NMT_Upgrade>::Discard(Bunch);
					break;
				case NMT_Challenge:
					FNetControlMessage<NMT_Challenge>::Discard(Bunch);
					break;
				case NMT_Netspeed:
					FNetControlMessage<NMT_Netspeed>::Discard(Bunch);
					break;
				case NMT_Login:
					FNetControlMessage<NMT_Login>::Discard(Bunch);
					break;
				case NMT_Failure:
					FNetControlMessage<NMT_Failure>::Discard(Bunch);
					break;
				case NMT_Join:
					//FNetControlMessage<NMT_Join>::Discard(Bunch);
					break;
				case NMT_JoinSplit:
					FNetControlMessage<NMT_JoinSplit>::Discard(Bunch);
					break;
				case NMT_Skip:
					FNetControlMessage<NMT_Skip>::Discard(Bunch);
					break;
				case NMT_Abort:
					FNetControlMessage<NMT_Abort>::Discard(Bunch);
					break;
				case NMT_PCSwap:
					FNetControlMessage<NMT_PCSwap>::Discard(Bunch);
					break;
				case NMT_ActorChannelFailure:
					FNetControlMessage<NMT_ActorChannelFailure>::Discard(Bunch);
					break;
				case NMT_DebugText:
					FNetControlMessage<NMT_DebugText>::Discard(Bunch);
					break;
				case NMT_NetGUIDAssign:
					FNetControlMessage<NMT_NetGUIDAssign>::Discard(Bunch);
				case NMT_BeaconWelcome:
					//FNetControlMessage<NMT_BeaconWelcome>::Discard(Bunch);
					break;
				case NMT_BeaconJoin:
					FNetControlMessage<NMT_BeaconJoin>::Discard(Bunch);
					break;
				case NMT_BeaconAssignGUID:
					FNetControlMessage<NMT_BeaconAssignGUID>::Discard(Bunch);
					break;
				case NMT_BeaconNetGUIDAck:
					FNetControlMessage<NMT_BeaconNetGUIDAck>::Discard(Bunch);
					break;
				default:
					check(!FNetControlMessageInfo::IsRegistered(MessageType)); // if this fails, a case is missing above for an implemented message type

					UE_LOG(LogNet, Error, TEXT("Received unknown control channel message"));
					ensureMsgf(false, TEXT("Failed to read control channel message %i"), int32(MessageType));
					Connection->Close();
					return;
			}
		}
		if ( Bunch.IsError() )
		{
			UE_LOG( LogNet, Error, TEXT( "Failed to read control channel message '%s'" ), FNetControlMessageInfo::GetName( MessageType ) );
			break;
		}
	}

	if ( Bunch.IsError() )
	{
		UE_LOG( LogNet, Error, TEXT( "UControlChannel::ReceivedBunch: Failed to read control channel message" ) );

		if (Connection != NULL)
		{
			Connection->Close();
		}
	}
}

void UControlChannel::QueueMessage(const FOutBunch* Bunch)
{
	if (QueuedMessages.Num() >= MAX_QUEUED_CONTROL_MESSAGES)
	{
		// we're out of room in our extra buffer as well, so kill the connection
		UE_LOG(LogNet, Log, TEXT("Overflowed control channel message queue, disconnecting client"));
		// intentionally directly setting State as the messaging in Close() is not going to work in this case
		Connection->State = USOCK_Closed;
	}
	else
	{
		int32 Index = QueuedMessages.AddZeroed();
		QueuedMessages[Index].AddUninitialized(Bunch->GetNumBytes());
		FMemory::Memcpy(QueuedMessages[Index].GetData(), Bunch->GetData(), Bunch->GetNumBytes());
	}
}

FPacketIdRange UControlChannel::SendBunch(FOutBunch* Bunch, bool Merge)
{
	// if we already have queued messages, we need to queue subsequent ones to guarantee proper ordering
	if (QueuedMessages.Num() > 0 || NumOutRec >= RELIABLE_BUFFER - 1 + Bunch->bClose)
	{
		QueueMessage(Bunch);
		return FPacketIdRange(INDEX_NONE);
	}
	else
	{
		if (!Bunch->IsError())
		{
			return Super::SendBunch(Bunch, Merge);
		}
		else
		{
			// an error here most likely indicates an unfixable error, such as the text using more than the maximum packet size
			// so there is no point in queueing it as it will just fail again
			UE_LOG(LogNet, Error, TEXT("Control channel bunch overflowed"));
			ensureMsgf(false, TEXT("Control channel bunch overflowed"));
			Connection->Close();
			return FPacketIdRange(INDEX_NONE);
		}
	}
}

void UControlChannel::Tick()
{
	Super::Tick();

	if( !OpenAcked )
	{
		int32 Count = 0;
		for( FOutBunch* Out=OutRec; Out; Out=Out->Next )
			if( !Out->ReceivedAck )
				Count++;
		if ( Count > 8 )
			return;
		// Resend any pending packets if we didn't get the appropriate acks.
		for( FOutBunch* Out=OutRec; Out; Out=Out->Next )
		{
			if( !Out->ReceivedAck )
			{
				float Wait = Connection->Driver->Time-Out->Time;
				checkSlow(Wait>=0.f);
				if( Wait>1.f )
				{
					UE_LOG(LogNetTraffic, Log, TEXT("Channel %i ack timeout); resending %i..."), ChIndex, Out->ChSequence );
					check(Out->bReliable);
					Connection->SendRawBunch( *Out, 0 );
				}
			}
		}
	}
	else
	{
		// attempt to send queued messages
		while (QueuedMessages.Num() > 0 && !Closing)
		{
			FControlChannelOutBunch Bunch(this, 0);
			if (Bunch.IsError())
			{
				break;
			}
			else
			{
				Bunch.bReliable = 1;
				Bunch.Serialize(QueuedMessages[0].GetData(), QueuedMessages[0].Num());
				if (!Bunch.IsError())
				{
					Super::SendBunch(&Bunch, 1);
					QueuedMessages.RemoveAt(0, 1);
				}
				else
				{
					// an error here most likely indicates an unfixable error, such as the text using more than the maximum packet size
					// so there is no point in queueing it as it will just fail again
					ensureMsgf(false, TEXT("Control channel bunch overflowed"));
					UE_LOG(LogNet, Error, TEXT("Control channel bunch overflowed"));
					Connection->Close();
					break;
				}
			}
		}
	}
}


FString UControlChannel::Describe()
{
	return FString(TEXT("Text ")) + UChannel::Describe();
}

/*-----------------------------------------------------------------------------
	UActorChannel.
-----------------------------------------------------------------------------*/

void UActorChannel::Init( UNetConnection* InConnection, int32 InChannelIndex, bool InOpenedLocally )
{
	Super::Init( InConnection, InChannelIndex, InOpenedLocally );

	RelevantTime			= Connection->Driver->Time;
	LastUpdateTime			= Connection->Driver->Time - Connection->Driver->SpawnPrioritySeconds;
	bActorMustStayDirty		= false;
	bActorStillInitial		= false;
	CustomTimeDilation		= 1.0f;
}

void UActorChannel::SetClosingFlag()
{
	if( Actor )
		Connection->ActorChannels.Remove( Actor );
	UChannel::SetClosingFlag();
}

void UActorChannel::Close()
{
	UE_LOG(LogNetTraffic, Log, TEXT("UActorChannel::Close: ChIndex: %d, Actor: %s"), ChIndex, Actor ? *Actor->GetFullName() : TEXT("NULL") );

	UChannel::Close();

	if (Actor != NULL)
	{
		bool bKeepReplicators = false;		// If we keep replicators around, we can use them to determine if the actor changed since it went dormant

		if ( Dormant )
		{
			check( Actor->NetDormancy > DORM_Awake ); // Dormancy should have been canceled if game code changed NetDormancy
			Connection->DormantActors.Add(Actor);

			// Validation checking
			static const auto ValidateCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("net.DormancyValidate"));
			if ( ValidateCVar && ValidateCVar->GetValueOnGameThread() > 0 )
			{
				bKeepReplicators = true;		// We need to keep the replicators around so we can use
			}
		}

		// SetClosingFlag() might have already done this, but we need to make sure as that won't get called if the connection itself has already been closed
		Connection->ActorChannels.Remove( Actor );

		Actor = NULL;
		CleanupReplicators( bKeepReplicators );
	}
}

void UActorChannel::CleanupReplicators( const bool bKeepReplicators )
{
	// Cleanup or save replicators
	for ( auto CompIt = ReplicationMap.CreateIterator(); CompIt; ++CompIt )
	{
		if ( bKeepReplicators )
		{
			// If we want to keep the replication state of the actor/sub-objects around, transfer ownership to the connection
			// This way, if this actor opens another channel on this connection, we can reclaim or use this replicator to compare state, etc.
			// For example, we may want to see if any state changed since the actor went dormant, and is now active again. 
			check( Connection->DormantReplicatorMap.Find( CompIt.Value()->GetObject() ) == NULL );
			Connection->DormantReplicatorMap.Add( CompIt.Value()->GetObject(), CompIt.Value() );
			CompIt.Value()->StopReplicating( this );		// Stop replicating on this channel
		}
		else
		{
			CompIt.Value()->CleanUp();
		}
	}

	ReplicationMap.Empty();

	ActorReplicator = NULL;
}

void UActorChannel::DestroyActorAndComponents()
{
	// Destroy any sub-objects we created
	for ( int32 i = 0; i < CreateSubObjects.Num(); i++ )
	{
		if ( CreateSubObjects[i].IsValid() )
		{
			UObject *SubObject = CreateSubObjects[i].Get();
			Actor->OnSubobjectDestroyFromReplication(SubObject);
			SubObject->MarkPendingKill();
		}
	}

	CreateSubObjects.Empty();

	// Destroy the actor
	if ( Actor != NULL )
	{
		Actor->Destroy( true );
	}
}

bool UActorChannel::CleanUp( const bool bForDestroy )
{
	const bool bIsServer = Connection->Driver->IsServer();

	UE_LOG( LogNetTraffic, Log, TEXT( "UActorChannel::CleanUp: Channel: %i, IsServer: %s" ), ChIndex, bIsServer ? TEXT( "YES" ) : TEXT( "NO" ) );

	// If we're the client, destroy this actor.
	if (!bIsServer)
	{
		check(Actor == NULL || Actor->IsValidLowLevel());
		checkSlow(Connection != NULL);
		checkSlow(Connection->IsValidLowLevel());
		checkSlow(Connection->Driver != NULL);
		checkSlow(Connection->Driver->IsValidLowLevel());
		if (Actor != NULL)
		{
			if (Actor->bTearOff && !Connection->Driver->ShouldClientDestroyTearOffActors())
			{
				Actor->Role = ROLE_Authority;
				Actor->SetReplicates(false);
				bTornOff = true;
				Actor->TornOff();
			}
			else if (Dormant && !Actor->bTearOff)
			{
				Connection->DormantActors.Add(Actor);
			}
			else if (!Actor->bNetTemporary && Actor->GetWorld() != NULL && !GIsRequestingExit)
			{
				UE_LOG(LogNetDormancy, Verbose, TEXT("Channel[%d] '%s' Destroying Actor."), ChIndex, *Describe() );

				DestroyActorAndComponents();
			}
		}
	}
	else if (Actor && !OpenAcked)
	{
		// Resend temporary actors if nak'd.
		Connection->SentTemporaries.Remove(Actor);
	}

	if ( !bIsServer && Dormant && QueuedBunches.Num() > 0 && ChIndex >= 0 && !bForDestroy )
	{
		if ( !ActorNetGUID.IsValid() )
		{
			UE_LOG( LogNet, Error, TEXT( "UActorChannel::CleanUp: Can't add to KeepProcessingActorChannelBunchesMap (ActorNetGUID invalid). Channel: %i" ), ChIndex );
		}
		else if ( Connection->KeepProcessingActorChannelBunchesMap.Contains( ActorNetGUID ) )
		{
			// FIXME: Handle this case!
			// We should merge the channel info here
			UE_LOG( LogNet, Error, TEXT( "UActorChannel::CleanUp: Can't add to KeepProcessingActorChannelBunchesMap (ActorNetGUID already in list). Channel: %i" ), ChIndex );
		}
		else
		{
			UE_LOG( LogNet, VeryVerbose, TEXT( "UActorChannel::CleanUp: Adding to KeepProcessingActorChannelBunchesMap. Channel: %i, Num: %i" ), ChIndex, Connection->KeepProcessingActorChannelBunchesMap.Num() );

			// Remember the connection, since CleanUp below will NULL it
			UNetConnection* OldConnection = Connection;

			// This will unregister the channel, and make it free for opening again
			// We need to do this, since the server will assume this channel is free once we ack this packet
			Super::CleanUp( bForDestroy );

			// Restore connection property since we'll need it for processing bunches (the Super::CleanUp call above NULL'd it)
			Connection = OldConnection;

			// Add this channel to the KeepProcessingActorChannelBunchesMap list
			check( !Connection->KeepProcessingActorChannelBunchesMap.Contains( ActorNetGUID ) );
			Connection->KeepProcessingActorChannelBunchesMap.Add( ActorNetGUID, this );

			// We set ChIndex to -1 to signify that we've already been "closed" but we aren't done processing bunches
			ChIndex = -1;

			// Return false so we won't do pending kill yet
			return false;
		}
	}

	// If there is another channel that was processing queued bunched for this guid, make sure to clean those up as well
	if ( Connection->KeepProcessingActorChannelBunchesMap.Contains( ActorNetGUID ) )
	{
		UActorChannel * OtherChannel = Connection->KeepProcessingActorChannelBunchesMap.FindChecked( ActorNetGUID );
		// We can ignore if this channel is already being destroyed, or if it's this actual channel (which is already being taken care of)
		if ( OtherChannel != NULL && !OtherChannel->IsPendingKill() && OtherChannel != this )
		{
			// Reset a few things so that the ConditionalCleanUp doesn't do work we will do here
			OtherChannel->Actor		= NULL;
			OtherChannel->Dormant	= 0;

			OtherChannel->ConditionalCleanUp( true );
		}
	}

	// Remove from hash and stuff.
	SetClosingFlag();

	CleanupReplicators();

	// We don't care about any leftover pending guids at this point
	PendingGuidResolves.Empty();

	// Free export bunches list
	for ( int32 i = 0; i < QueuedExportBunches.Num(); i++ )
	{
		delete QueuedExportBunches[i];
	}

	QueuedExportBunches.Empty();

	// Free the must be mapped list
	QueuedMustBeMappedGuidsInLastBunch.Empty();

	// Free any queued bunches
	for ( int32 i = 0; i < QueuedBunches.Num(); i++ )
	{
		delete QueuedBunches[i];
	}

	QueuedBunches.Empty();

	// We check for -1 here, which will be true if this channel has already been closed but still needed to process bunches before fully closing
	if ( ChIndex >= 0 )	
	{
		return Super::CleanUp( bForDestroy );
	}

	return true;
}

void UActorChannel::ReceivedNak( int32 NakPacketId )
{
	UChannel::ReceivedNak(NakPacketId);	
	for (auto CompIt = ReplicationMap.CreateIterator(); CompIt; ++CompIt)
	{
		CompIt.Value()->ReceivedNak(NakPacketId);
	}

	// Reset any subobject RepKeys that were sent on this packetId
	FPacketRepKeyInfo * Info = SubobjectNakMap.Find(NakPacketId % SubobjectRepKeyBufferSize);
	if (Info)
	{
		if (Info->PacketID == NakPacketId)
		{
			UE_LOG(LogNetTraffic, Verbose, TEXT("ActorChannel[%d]: Reseting object keys due to Nak: %d"), ChIndex, NakPacketId);
			for (auto It = Info->ObjKeys.CreateIterator(); It; ++It)
			{
				SubobjectRepKeyMap.FindOrAdd(*It) = INDEX_NONE;
				UE_LOG(LogNetTraffic, Verbose, TEXT("    %d"), *It);
			}
		}
	}
}

void UActorChannel::SetChannelActor( AActor* InActor )
{
	check(!Closing);
	check(Actor==NULL);

	// Set stuff.
	Actor = InActor;

	UE_LOG(LogNetTraffic, VeryVerbose, TEXT("SetChannelActor: ChIndex: %i, Actor: %s, NetGUID: %s"), ChIndex, Actor ? *Actor->GetFullName() : TEXT("NULL"), *ActorNetGUID.ToString() );

	if ( ChIndex >= 0 && Connection->PendingOutRec[ChIndex] > 0 )
	{
		// send empty reliable bunches to synchronize both sides
		// UE_LOG(LogNetTraffic, Log, TEXT("%i Actor %s WILL BE sending %i vs first %i"), ChIndex, *Actor->GetName(), Connection->PendingOutRec[ChIndex],Connection->OutReliable[ChIndex]);
		int32 RealOutReliable = Connection->OutReliable[ChIndex];
		Connection->OutReliable[ChIndex] = Connection->PendingOutRec[ChIndex] - 1;
		while ( Connection->PendingOutRec[ChIndex] <= RealOutReliable )
		{
			// UE_LOG(LogNetTraffic, Log, TEXT("%i SYNCHRONIZING by sending %i"), ChIndex, Connection->PendingOutRec[ChIndex]);

			FOutBunch Bunch( this, 0 );
			if( !Bunch.IsError() )	// FIXME: This will be an infinite loop if this happens!!!
			{
				Bunch.bReliable = true;
				SendBunch( &Bunch, 0 );
				Connection->PendingOutRec[ChIndex]++;
			}
		}

		Connection->OutReliable[ChIndex] = RealOutReliable;
		Connection->PendingOutRec[ChIndex] = 0;
	}


	// Add to map.
	Connection->ActorChannels.Add( Actor, this );

	check( !ReplicationMap.Contains( Actor ) );

	// Create the actor replicator, and store a quick access pointer to it
	ActorReplicator = &FindOrCreateReplicator( Actor ).Get();

	// Remove from connection's dormancy lists
	Connection->DormantActors.Remove( InActor );
	Connection->RecentlyDormantActors.Remove( InActor );
}

void UActorChannel::SetChannelActorForDestroy( FActorDestructionInfo *DestructInfo )
{
	check(Connection->Channels[ChIndex]==this);
	if
	(	!Closing
	&&	(Connection->State==USOCK_Open || Connection->State==USOCK_Pending) )
	{
		// Send a close notify, and wait for ack.
		FOutBunch CloseBunch( this, 1 );
		check(!CloseBunch.IsError());
		check(CloseBunch.bClose);
		CloseBunch.bReliable = 1;
		CloseBunch.bDormant = 0;

		// Serialize DestructInfo
		NET_CHECKSUM(CloseBunch); // This is to mirror the Checksum in UPackageMapClient::SerializeNewActor
		Connection->PackageMap->WriteObject( CloseBunch, DestructInfo->ObjOuter.Get(), DestructInfo->NetGUID, DestructInfo->PathName );

		UE_LOG(LogNetTraffic, Log, TEXT("SetChannelActorForDestroy: Channel %d. NetGUID <%s> Path: %s. Bits: %d"), ChIndex, *DestructInfo->NetGUID.ToString(), *DestructInfo->PathName, CloseBunch.GetNumBits() );
		UE_LOG(LogNetDormancy, Verbose, TEXT("SetChannelActorForDestroy: Channel %d. NetGUID <%s> Path: %s. Bits: %d"), ChIndex, *DestructInfo->NetGUID.ToString(), *DestructInfo->PathName, CloseBunch.GetNumBits() );

		SendBunch( &CloseBunch, 0 );
	}
}

void UActorChannel::Tick()
{
	Super::Tick();
	ProcessQueuedBunches();
}

bool UActorChannel::ProcessQueuedBunches()
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("ProcessQueuedBunches time"), STAT_ProcessQueuedBunchesTime, STATGROUP_Net);

	const uint32 QueueBunchStartCycles = FPlatformTime::Cycles();

	// Try to resolve any guids that are holding up the network stream on this channel
	for ( auto It = PendingGuidResolves.CreateIterator(); It; ++It )
	{
		if ( Connection->Driver->GuidCache->GetObjectFromNetGUID( *It, true ) != NULL )
		{
			// This guid is now resolved, we can remove it from the pending guid list
			It.RemoveCurrent();
			continue;
		}

		if ( Connection->Driver->GuidCache->IsGUIDBroken( *It, true ) )
		{
			// This guid is broken, remove it, and warn
			UE_LOG( LogNet, Warning, TEXT( "UActorChannel::ProcessQueuedBunches: Guid is broken. NetGUID: %s, ChIndex: %i, Actor: %s" ), *It->ToString(), ChIndex, Actor != NULL ? *Actor->GetPathName() : TEXT( "NULL" ) );
			It.RemoveCurrent();
			continue;
		}
	}

	const bool bHasTimeToProcess = Connection->Driver->ProcessQueuedBunchesCurrentFrameMilliseconds < CVarNetProcessQueuedBunchesMillisecondLimit.GetValueOnGameThread();

	// We can process all of the queued up bunches if ALL of these are true:
	//	1. We have queued bunches to process
	//	2. We no longer have any pending guids to load
	//	3. We aren't still processing bunches on another channel that this actor was previously on
	//	4. We haven't spent too much time yet this frame processing queued bunches
	//	5. The driver isn't requesting queuing for this GUID
	if ( QueuedBunches.Num() > 0 && PendingGuidResolves.Num() == 0 && ( ChIndex == -1 || !Connection->KeepProcessingActorChannelBunchesMap.Contains( ActorNetGUID ) ) &&
		 bHasTimeToProcess && !Connection->Driver->ShouldQueueBunchesForActorGUID( ActorNetGUID ) )
	{
		for ( int32 i = 0; i < QueuedBunches.Num(); i++ )
		{
			ProcessBunch( *QueuedBunches[i] );
			delete QueuedBunches[i];
		}

		UE_LOG( LogNet, VeryVerbose, TEXT( "UActorChannel::ProcessQueuedBunches: Flushing queued bunches. ChIndex: %i, Actor: %s, Queued: %i" ), ChIndex, Actor != NULL ? *Actor->GetPathName() : TEXT( "NULL" ), QueuedBunches.Num() );

		QueuedBunches.Empty();

		// Call any onreps that were delayed because we were queuing bunches
		for (auto& ReplicatorPair : ReplicationMap)
		{
			ReplicatorPair.Value->CallRepNotifies();
		}
	}

	// Warn when we have queued bunches for a very long time
	if ( QueuedBunches.Num() > 0 )
	{
		const double QUEUED_BUNCH_TIMEOUT_IN_SECONDS = 30;

		if ( FPlatformTime::Seconds() - QueuedBunchStartTime > QUEUED_BUNCH_TIMEOUT_IN_SECONDS )
		{
			UE_LOG( LogNet, Warning, TEXT( "UActorChannel::ProcessQueuedBunches: Queued bunches for longer than normal. ChIndex: %i, Actor: %s, Queued: %i" ), ChIndex, Actor != NULL ? *Actor->GetPathName() : TEXT( "NULL" ), QueuedBunches.Num() );
			QueuedBunchStartTime = FPlatformTime::Seconds();
		}
	}

	// Update the driver with our time spent
	const uint32 QueueBunchEndCycles = FPlatformTime::Cycles();
	const uint32 QueueBunchDeltaCycles = QueueBunchEndCycles - QueueBunchStartCycles;
	const float QueueBunchDeltaMilliseconds = FPlatformTime::ToMilliseconds(QueueBunchDeltaCycles);

	Connection->Driver->ProcessQueuedBunchesCurrentFrameMilliseconds += QueueBunchDeltaMilliseconds;

	// Return true if we are done processing queued bunches
	return QueuedBunches.Num() == 0;
}

void UActorChannel::ReceivedBunch( FInBunch & Bunch )
{
	check( !Closing );

	if ( Broken || bTornOff )
	{
		return;
	}

	if ( Connection->Driver->IsServer() )
	{
		if ( Bunch.bHasMustBeMappedGUIDs )
		{
			UE_LOG( LogNetTraffic, Error, TEXT( "UActorChannel::ReceivedBunch: Client attempted to set bHasMustBeMappedGUIDs. Actor: %s" ), Actor != NULL ? *Actor->GetName() : TEXT( "NULL" ) );
			Bunch.SetError();
			return;
		}
	}
	else
	{
		if ( Bunch.bHasMustBeMappedGUIDs )
		{
			// If this bunch has any guids that must be mapped, we need to wait until they resolve before we can 
			// process the rest of the stream on this channel
			uint16 NumMustBeMappedGUIDs = 0;
			Bunch << NumMustBeMappedGUIDs;

			//UE_LOG( LogNetTraffic, Warning, TEXT( "Read must be mapped GUID's. NumMustBeMappedGUIDs: %i" ), NumMustBeMappedGUIDs );

			UPackageMapClient * PackageMapClient = CastChecked< UPackageMapClient >( Connection->PackageMap );

			for ( int32 i = 0; i < NumMustBeMappedGUIDs; i++ )
			{
				FNetworkGUID NetGUID;
				Bunch << NetGUID;

				// This GUID better have been exported before we get here, which means it must be registered by now
				check( Connection->Driver->GuidCache->IsGUIDRegistered( NetGUID ) );

				if ( !Connection->Driver->GuidCache->IsGUIDLoaded( NetGUID ) )
				{
					PendingGuidResolves.Add( NetGUID );
				}
			}
		}

		if ( Actor == NULL && Bunch.bOpen )
		{
			// Take a sneak peak at the actor guid so we have a copy of it now
			FBitReaderMark Mark( Bunch );

			NET_CHECKSUM( Bunch );

			Bunch << ActorNetGUID;

			Mark.Pop( Bunch );
		}

		// We need to queue this bunch if any of these are true:
		//	1. We have pending guids to resolve
		//	2. We already have queued up bunches
		//	3. If this actor was previously on a channel that is now still processing bunches after a close
		//	4. The driver is requesting queuing for this GUID
		if ( PendingGuidResolves.Num() > 0 || QueuedBunches.Num() > 0 || Connection->KeepProcessingActorChannelBunchesMap.Contains( ActorNetGUID ) ||
			 ( Connection->Driver->ShouldQueueBunchesForActorGUID( ActorNetGUID ) ) )
		{
			if ( Connection->KeepProcessingActorChannelBunchesMap.Contains( ActorNetGUID ) )
			{
				UE_LOG( LogNet, Log, TEXT( "UActorChannel::ReceivedBunch: Queuing bunch because another channel (that closed) is processing bunches for this guid still. ActorNetGUID: %s" ), *ActorNetGUID.ToString() );
			}

			if ( QueuedBunches.Num() == 0 )
			{
				// Remember when we first started queuing
				QueuedBunchStartTime = FPlatformTime::Seconds();
			}

			QueuedBunches.Add( new FInBunch( Bunch ) );

			return;
		}
	}

	// We can process this bunch now
	ProcessBunch( Bunch );
}

void UActorChannel::ProcessBunch( FInBunch & Bunch )
{
	if ( Broken )
	{
		return;
	}

	const bool bIsServer = Connection->Driver->IsServer();

	FReplicationFlags RepFlags;

	// ------------------------------------------------------------
	// Initialize client if first time through.
	// ------------------------------------------------------------
	bool bSpawnedNewActor = false;	// If this turns to true, we know an actor was spawned (rather than found)
	if( Actor == NULL )
	{
		if( !Bunch.bOpen )
		{
			// This absolutely shouldn't happen anymore, since we no longer process packets until channel is fully open early on
			UE_LOG(LogNetTraffic, Error, TEXT( "UActorChannel::ProcessBunch: New actor channel received non-open packet. bOpen: %i, bClose: %i, bReliable: %i, bPartial: %i, bPartialInitial: %i, bPartialFinal: %i, ChType: %i, ChIndex: %i, Closing: %i, OpenedLocally: %i, OpenAcked: %i, NetGUID: %s" ), (int)Bunch.bOpen, (int)Bunch.bClose, (int)Bunch.bReliable, (int)Bunch.bPartial, (int)Bunch.bPartialInitial, (int)Bunch.bPartialFinal, (int)ChType, ChIndex, (int)Closing, (int)OpenedLocally, (int)OpenAcked, *ActorNetGUID.ToString() );
			return;
		}

		AActor* NewChannelActor = NULL;
		bSpawnedNewActor = Connection->PackageMap->SerializeNewActor(Bunch, this, NewChannelActor);

		// We are unsynchronized. Instead of crashing, let's try to recover.
		if (NewChannelActor == NULL || NewChannelActor->IsPendingKill())
		{
			check( !bSpawnedNewActor );
			UE_LOG(LogNet, Warning, TEXT("UActorChannel::ProcessBunch: SerializeNewActor failed to find/spawn actor. Actor: %s, Channel: %i"), NewChannelActor ? *NewChannelActor->GetFullName() : TEXT( "NULL" ), ChIndex);
			Broken = 1;
			if ( !Connection->InternalAck )
			{
				FNetControlMessage<NMT_ActorChannelFailure>::Send(Connection, ChIndex);
			}
			return;
		}

		UE_LOG(LogNetTraffic, Log, TEXT("      Channel Actor %s:"), *NewChannelActor->GetFullName() );
		SetChannelActor( NewChannelActor );

		Actor->OnActorChannelOpen(Bunch, Connection);

		RepFlags.bNetInitial = true;

		Actor->CustomTimeDilation = CustomTimeDilation;
	}
	else
	{
		UE_LOG(LogNetTraffic, Log, TEXT("      Actor %s:"), *Actor->GetFullName() );
	}

	// Owned by connection's player?
	UNetConnection* ActorConnection = Actor->GetNetConnection();
	if (ActorConnection == Connection || (ActorConnection != NULL && ActorConnection->IsA(UChildConnection::StaticClass()) && ((UChildConnection*)ActorConnection)->Parent == Connection))
	{
		RepFlags.bNetOwner = true;
	}

	// ----------------------------------------------
	//	Read chunks of actor content
	// ----------------------------------------------
	while ( !Bunch.AtEnd() && Connection != NULL && Connection->State != USOCK_Closed )
	{
		bool bObjectDeleted = false;
		UObject* RepObj = ReadContentBlockHeader( Bunch, bObjectDeleted );

		if ( Bunch.IsError() )
		{
			UE_LOG( LogNet, Error, TEXT( "UActorChannel::ReceivedBunch: ReadContentBlockHeader FAILED. Bunch.IsError() == TRUE. Closing connection. RepObj: %s, Channel: %i"), RepObj ? *RepObj->GetFullName() : TEXT( "NULL" ), ChIndex );
			Connection->Close();
			return;
		}

		if (bObjectDeleted)
		{
			// Nothing else in this block, continue on
			continue;
		}

		if ( !RepObj || RepObj->IsPendingKill() )
		{
			UE_LOG(LogNet, Warning, TEXT("UActorChannel::ProcessBunch: ReadContentBlockHeader failed to find/create object. RepObj: %s, Channel: %i"), RepObj ? *RepObj->GetFullName() : TEXT( "NULL" ), ChIndex);

			if ( !Actor || Actor->IsPendingKill() )
			{
				Broken = 1;
			}

			break;	// We can't continue reading since this will throw off the de-serialization
		}

		TSharedRef< FObjectReplicator > & Replicator = FindOrCreateReplicator( RepObj );

		bool bHasUnmapped = false;

		if ( !Replicator->ReceivedBunch( Bunch, RepFlags, bHasUnmapped ) )
		{
			UE_LOG( LogNet, Error, TEXT( "UActorChannel::ProcessBunch: Replicator.ReceivedBunch failed.  Closing connection. RepObj: %s, Channel: %i"), RepObj ? *RepObj->GetFullName() : TEXT( "NULL" ), ChIndex  );
	
			if ( Connection->InternalAck )
			{
				break;
			}

			Connection->Close();
			return;
		}

		// Check to see if the actor was destroyed
		// If so, don't continue processing packets on this channel, or we'll trigger an error otherwise
		// note that this is a legitimate occurrence, particularly on client to server RPCs
		if ( !Actor || Actor->IsPendingKill() )
		{
			UE_LOG( LogNet, Verbose, TEXT( "UActorChannel::ProcessBunch: Actor was destroyed during Replicator.ReceivedBunch processing" ) );
			// If we lose the actor on this channel, we can no longer process bunches, so consider this channel broken
			Broken = 1;		
			break;
		}

		if ( bHasUnmapped )
		{
			Connection->Driver->UnmappedReplicators.Add( Replicator );
		}
	}

	for (auto RepComp = ReplicationMap.CreateIterator(); RepComp; ++RepComp)
	{
		if ( RepComp.Key().IsValid() )
		{
			RepComp.Value()->PostReceivedBunch();
		}
	}

	// After all properties have been initialized, call PostNetInit. This should call BeginPlay() so initialization can be done with proper starting values.
	if (Actor && bSpawnedNewActor)
	{
		Actor->PostNetInit();
	}
}

bool UActorChannel::ReplicateActor()
{
	SCOPE_CYCLE_COUNTER(STAT_NetReplicateActorsTime);

	checkSlow(Actor);
	checkSlow(!Closing);

	// The package map shouldn't have any carry over guids
	if ( CastChecked< UPackageMapClient >( Connection->PackageMap )->GetMustBeMappedGuidsInLastBunch().Num() != 0 )
	{
		UE_LOG( LogNet, Warning, TEXT( "ReplicateActor: PackageMap->GetMustBeMappedGuidsInLastBunch().Num() != 0: %i" ), CastChecked< UPackageMapClient >( Connection->PackageMap )->GetMustBeMappedGuidsInLastBunch().Num() );
	}

	// Time how long it takes to replicate this particular actor
	STAT( FScopeCycleCounterUObject FunctionScope(Actor) );

	bool WroteSomethingImportant = false;

	// triggering replication of an Actor while already in the middle of replication can result in invalid data being sent and is therefore illegal
	if (bIsReplicatingActor)
	{
		FString Error(FString::Printf(TEXT("Attempt to replicate '%s' while already replicating that Actor!"), *Actor->GetName()));
		UE_LOG(LogNet, Log, TEXT("%s"), *Error);
		ensureMsg(false,*Error);
		return false;
	}

	// Create an outgoing bunch, and skip this actor if the channel is saturated.
	FOutBunch Bunch( this, 0 );
	if( Bunch.IsError() )
	{
		return false;
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (CVarNetReliableDebug.GetValueOnGameThread() > 0)
	{
		Bunch.DebugString = FString::Printf(TEXT("%.2f ActorBunch: %s"), Connection->Driver->Time, *Actor->GetName() );
	}
#endif

	bIsReplicatingActor = true;
	FReplicationFlags RepFlags;

	// Send initial stuff.
	if( OpenPacketId.First != INDEX_NONE )
	{
		if( !SpawnAcked && OpenAcked )
		{
			// After receiving ack to the spawn, force refresh of all subsequent unreliable packets, which could
			// have been lost due to ordering problems. Note: We could avoid this by doing it in FActorChannel::ReceivedAck,
			// and avoid dirtying properties whose acks were received *after* the spawn-ack (tricky ordering issues though).
			SpawnAcked = 1;
			for (auto RepComp = ReplicationMap.CreateIterator(); RepComp; ++RepComp)
			{
				RepComp.Value()->ForceRefreshUnreliableProperties();
			}
		}
	}
	else
	{
		RepFlags.bNetInitial = true;
		Bunch.bClose = Actor->bNetTemporary;
		Bunch.bReliable = !Actor->bNetTemporary;
	}

	// Owned by connection's player?
	UNetConnection* OwningConnection = Actor->GetNetConnection();
	if (OwningConnection == Connection || (OwningConnection != NULL && OwningConnection->IsA(UChildConnection::StaticClass()) && ((UChildConnection*)OwningConnection)->Parent == Connection))
	{
		RepFlags.bNetOwner = true;
	}
	else
	{
		RepFlags.bNetOwner = false;
	}


	// ----------------------------------------------------------
	// If initial, send init data.
	// ----------------------------------------------------------
	if( RepFlags.bNetInitial && OpenedLocally )
	{
		Connection->PackageMap->SerializeNewActor(Bunch, this, Actor);
		WroteSomethingImportant = true;

		Actor->OnSerializeNewActor(Bunch);
	}

	// Save out the actor's RemoteRole, and downgrade it if necessary.
	ENetRole const ActualRemoteRole = Actor->GetRemoteRole();
	if (ActualRemoteRole == ROLE_AutonomousProxy)
	{
		if (!RepFlags.bNetOwner)
		{
			Actor->SetAutonomousProxy(false);
		}
	}

	RepFlags.bNetSimulated	= ( Actor->GetRemoteRole() == ROLE_SimulatedProxy );
	RepFlags.bRepPhysics	= Actor->ReplicatedMovement.bRepPhysics;

	RepFlags.bNetInitial = RepFlags.bNetInitial || bActorStillInitial; // for replication purposes, bNetInitial stays true until all properties sent
	bActorMustStayDirty = false;
	bActorStillInitial = false;

	UE_LOG(LogNetTraffic, Log, TEXT("Replicate %s, bNetInitial: %d, bNetOwner: %d"), *Actor->GetName(), RepFlags.bNetInitial, RepFlags.bNetOwner );

	FMemMark	MemMark(FMemStack::Get());	// The calls to ReplicateProperties will allocate memory on FMemStack::Get(), and use it in ::PostSendBunch. we free it below

	bool FilledUp = false;	// For now, we cant be filled up since we do partial bunches, but there is some logic below I want to preserve in case we ever do need to cut off partial bunch size

	// ----------------------------------------------------------
	// Replicate Actor and Component properties and RPCs
	// ----------------------------------------------------------

#if USE_NETWORK_PROFILER 
	const uint32 ActorReplicateStartTime = GNetworkProfiler.IsTrackingEnabled() ? FPlatformTime::Cycles() : 0;
#endif

	// The Actor
	WroteSomethingImportant |= ActorReplicator->ReplicateProperties( Bunch, RepFlags );

	// The SubObjects
	WroteSomethingImportant |= Actor->ReplicateSubobjects(this, &Bunch, &RepFlags);

	// Look for deleted subobjects
	for (auto RepComp = ReplicationMap.CreateIterator(); RepComp; ++RepComp)
	{
		if (!RepComp.Key().IsValid())
		{
			// Write a deletion content header:
			BeginContentBlockForSubObjectDelete( Bunch, RepComp.Value()->ObjectNetGUID );

			WroteSomethingImportant = true;
			Bunch.bReliable = true;

			RepComp.Value()->CleanUp();
			RepComp.RemoveCurrent();			
		}
	}


	NETWORK_PROFILER(GNetworkProfiler.TrackReplicateActor(Actor, RepFlags, FPlatformTime::Cycles() - ActorReplicateStartTime ));

	// -----------------------------
	// Send if necessary
	// -----------------------------
	bool SentBunch = false;
	if( WroteSomethingImportant )
	{
		FPacketIdRange PacketRange = SendBunch( &Bunch, 1 );
		for (auto RepComp = ReplicationMap.CreateIterator(); RepComp; ++RepComp)
		{
			RepComp.Value()->PostSendBunch( PacketRange, Bunch.bReliable );
		}

		// If there were any subobject keys pending, add them to the NakMap
		if (PendingObjKeys.Num() >0)
		{
			// For the packet range we just sent over
			for(int32 PacketId = PacketRange.First; PacketId <= PacketRange.Last; ++PacketId)
			{
				// Get the existing set (its possible we send multiple bunches back to back and they end up on the same packet)
				FPacketRepKeyInfo &Info = SubobjectNakMap.FindOrAdd(PacketId % SubobjectRepKeyBufferSize);
				if (Info.PacketID != PacketId)
				{
					UE_LOG(LogNetTraffic, Verbose, TEXT("ActorChannel[%d]: Clearing out PacketRepKeyInfo for new packet: %d"), ChIndex, PacketId);
					Info.ObjKeys.Empty(Info.ObjKeys.Num());
				}
				Info.PacketID = PacketId;
				Info.ObjKeys.Append(PendingObjKeys);

				FString VerboseString;
				for (auto KeyIt = PendingObjKeys.CreateIterator(); KeyIt; ++KeyIt)
				{
					VerboseString += FString::Printf(TEXT(" %d"), *KeyIt);
				}
				
				UE_LOG(LogNetTraffic, Verbose, TEXT("ActorChannel[%d]: Sending ObjKeys: %s"), ChIndex, *VerboseString);
			}
		}
		
		SentBunch = true;
		if( Actor->bNetTemporary )
		{
			Connection->SentTemporaries.Add( Actor );
		}
	}

	PendingObjKeys.Empty();
	

	// If we evaluated everything, mark LastUpdateTime, even if nothing changed.
	if ( FilledUp )
	{
		UE_LOG(LogNetTraffic, Log, TEXT("Filled packet up before finishing %s still initial %d"),*Actor->GetName(),bActorStillInitial);
	}
	else
	{
		LastUpdateTime = Connection->Driver->Time;
	}

	bActorStillInitial = RepFlags.bNetInitial && (FilledUp || (!Actor->bNetTemporary && bActorMustStayDirty));

	// Reset temporary net info.
	if (Actor->GetRemoteRole() != ActualRemoteRole)
	{
		Actor->SetReplicates(ActualRemoteRole != ROLE_None);
		if (ActualRemoteRole == ROLE_AutonomousProxy)
		{
			Actor->SetAutonomousProxy(true);
		}
	}

	MemMark.Pop();

	bIsReplicatingActor = false;

	return SentBunch;
}


FString UActorChannel::Describe()
{
	if( Closing || !Actor )
		return FString(TEXT("Actor=None ")) + UChannel::Describe();
	else
		return FString::Printf(TEXT("Actor=%s (Role=%i RemoteRole=%i)"), *Actor->GetFullName(), (int32)Actor->Role, (int32)Actor->GetRemoteRole() ) + UChannel::Describe();
}


void UActorChannel::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UActorChannel* This = CastChecked<UActorChannel>(InThis);	
	Super::AddReferencedObjects( This, Collector );
}


void UActorChannel::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	
	if (Ar.IsCountingMemory())
	{
		for (auto MapIt = ReplicationMap.CreateIterator(); MapIt; ++MapIt)
		{
			MapIt.Value()->Serialize(Ar);
		}
	}
}

void UActorChannel::QueueRemoteFunctionBunch( UObject* CallTarget, UFunction* Func, FOutBunch &Bunch )
{
	FindOrCreateReplicator(CallTarget).Get().QueueRemoteFunctionBunch( Func, Bunch );
}

void UActorChannel::BecomeDormant()
{
	UE_LOG(LogNetDormancy, Verbose, TEXT("Channel[%d] '%s' BecomeDormant()"), ChIndex, *Describe() );
	bPendingDormancy = false;
	Dormant = true;
	Close();
}

bool UActorChannel::ReadyForDormancy(bool suppressLogs)
{
	for ( auto MapIt = ReplicationMap.CreateIterator(); MapIt; ++MapIt )
	{
		if ( !MapIt.Key().IsValid() )
		{
			continue;
		}

		if (!MapIt.Value()->ReadyForDormancy(suppressLogs))
		{
			return false;
		}
	}
	return true;
}

void UActorChannel::StartBecomingDormant()
{
	UE_LOG(LogNetDormancy, Verbose, TEXT("Channel[%d] '%s' StartBecomingDormant()"), ChIndex, *Describe() );

	for (auto MapIt = ReplicationMap.CreateIterator(); MapIt; ++MapIt)
	{
		MapIt.Value()->StartBecomingDormant();
	}
	bPendingDormancy = 1;
}

void UActorChannel::BeginContentBlock( UObject* Obj, FOutBunch &Bunch )
{
	const int NumStartingBits = Bunch.GetNumBits();

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (CVarDoReplicationContextString->GetInt() > 0)
	{
		Connection->PackageMap->SetDebugContextString( FString::Printf(TEXT("Content Header for object: %s (Class: %s)"), *Obj->GetPathName(), *Obj->GetClass()->GetPathName() ) );
	}
#endif

	// If we are referring to the actor on the channel, we don't need to send anything (except a bit signifying this)
	const bool IsActor = Obj == Actor;

	Bunch.WriteBit( IsActor ? 1 : 0 );

	if ( IsActor )
	{
		NETWORK_PROFILER(GNetworkProfiler.TrackBeginContentBlock(Obj, Bunch.GetNumBits() - NumStartingBits));
		return;
	}

	check(Obj);
	Bunch << Obj;
	NET_CHECKSUM(Bunch);

	if ( Connection->Driver->IsServer() )
	{
		// Only the server can tell clients to create objects, so no need for the client to send this to the server
		if ( Obj->IsNameStableForNetworking() )
		{
			Bunch.WriteBit( 1 );
		}
		else
		{
			Bunch.WriteBit( 0 );
			UClass *ObjClass = Obj->GetClass();
			Bunch << ObjClass;
		}
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (CVarDoReplicationContextString->GetInt() > 0)
	{
		Connection->PackageMap->ClearDebugContextString();
	}
#endif

	NETWORK_PROFILER(GNetworkProfiler.TrackBeginContentBlock(Obj, Bunch.GetNumBits() - NumStartingBits));
}

void UActorChannel::BeginContentBlockForSubObjectDelete( FOutBunch & Bunch, FNetworkGUID & GuidToDelete )
{
	check( Connection->Driver->IsServer() );

	const int NumStartingBits = Bunch.GetNumBits();

	// Send a 0 bit to signify we are dealing with sub-objects
	Bunch.WriteBit( 0 );

	check( GuidToDelete.IsValid() );

	//	-Deleted object's NetGUID
	Bunch << GuidToDelete;
	NET_CHECKSUM(Bunch);

	// Send a 0 bit to indicate that this is not a stably named object
	Bunch.WriteBit( 0 );

	//	-Invalid NetGUID (interpreted as delete)
	FNetworkGUID InvalidNetGUID;
	InvalidNetGUID.Reset();
	Bunch << InvalidNetGUID;

	// Since the subobject has been deleted, we don't have a valid object to pass to the profiler.
	NETWORK_PROFILER(GNetworkProfiler.TrackBeginContentBlock(nullptr, Bunch.GetNumBits() - NumStartingBits));
}

void UActorChannel::EndContentBlock( UObject *Obj, FOutBunch &Bunch, const FClassNetCache* ClassCache )
{
	check(Obj);

	const int NumStartingBits = Bunch.GetNumBits();

	if (!ClassCache)
	{
		ClassCache = Connection->Driver->NetCache->GetClassNetCache( Obj->GetClass() );
	}

	if ( Connection->InternalAck )
	{
		// Write out 0 checksum to signify done
		uint32 Checksum = 0;
		Bunch << Checksum;
	}
	else
	{
		// Write max int to signify done
		Bunch.WriteIntWrapped(ClassCache->GetMaxIndex(), ClassCache->GetMaxIndex()+1);
	}

	NETWORK_PROFILER(GNetworkProfiler.TrackEndContentBlock(Obj, Bunch.GetNumBits() - NumStartingBits));
}

UObject* UActorChannel::ReadContentBlockHeader(FInBunch & Bunch, bool& bObjectDeleted)
{
	const bool IsServer = Connection->Driver->IsServer();
	bObjectDeleted = false;

	if ( Bunch.ReadBit() )
	{
		// If this is for the actor on the channel, we don't need to read anything else
		return Actor;
	}

	//
	// We need to handle a sub-object
	//

	// Note this heavily mirrors what happens in UPackageMapClient::SerializeNewActor
	FNetworkGUID NetGUID;
	UObject* SubObj = NULL;

	// Manually serialize the object so that we can get the NetGUID (in order to assign it if we spawn the object here)
	Connection->PackageMap->SerializeObject( Bunch, UObject::StaticClass(), SubObj, &NetGUID );

	NET_CHECKSUM_OR_END( Bunch );

	if ( Bunch.IsError() )
	{
		UE_LOG( LogNetTraffic, Error, TEXT( "UActorChannel::ReadContentBlockHeader: Bunch.IsError() == true after SerializeObject. SubObj: %s, Actor: %s" ), SubObj ? *SubObj->GetName() : TEXT("Null"), *Actor->GetName() );
		Bunch.SetError();
		return NULL;
	}

	if ( Bunch.AtEnd() )
	{
		UE_LOG( LogNetTraffic, Error, TEXT( "UActorChannel::ReadContentBlockHeader: Bunch.AtEnd() == true after SerializeObject. SubObj: %s, Actor: %s" ), SubObj ? *SubObj->GetName() : TEXT("Null"), *Actor->GetName() );
		Bunch.SetError();
		return NULL;
	}

	// Validate existing sub-object
	if ( SubObj != NULL )
	{
		// Sub-objects can't be actors (should just use an actor channel in this case)
		if ( Cast< AActor >( SubObj ) != NULL )
		{
			UE_LOG( LogNetTraffic, Error, TEXT( "UActorChannel::ReadContentBlockHeader: Sub-object not allowed to be actor type. SubObj: %s, Actor: %s" ), *SubObj->GetName(), *Actor->GetName() );
			Bunch.SetError();
			return NULL;
		}

		// Sub-objects must reside within their actor parents
		if ( !SubObj->IsIn( Actor ) )
		{
			UE_LOG( LogNetTraffic, Error, TEXT( "UActorChannel::ReadContentBlockHeader: Sub-object not in parent actor. SubObj: %s, Actor: %s" ), *SubObj->GetFullName(), *Actor->GetFullName() );

			if ( IsServer )
			{
				Bunch.SetError();
				return NULL;
			}
		}
	}

	if ( IsServer )
	{
		// The server should never need to create sub objects
		if ( SubObj == NULL )
		{
			UE_LOG( LogNetTraffic, Error, TEXT( "ReadContentBlockHeader: Client attempted to create sub-object. Actor: %s" ), *Actor->GetName() );
			Bunch.SetError();
			return NULL;
		}

		return SubObj;
	}

	if ( Bunch.ReadBit() )
	{
		// If this is a stably named sub-object, we shouldn't need to create it
		if ( SubObj == NULL )
		{
			// (ignore though if this is for replays)
			if ( !Connection->InternalAck )
			{
				UE_LOG( LogNetTraffic, Error, TEXT( "ReadContentBlockHeader: Stably named sub-object not found. Actor: %s" ), *Actor->GetName() );
				Bunch.SetError();
			}

			return NULL;
		}

		return SubObj;
	}

	// Serialize the class in case we have to spawn it.
	// Manually serialize the object so that we can get the NetGUID (in order to assign it if we spawn the object here)
	FNetworkGUID ClassNetGUID;
	UObject* SubObjClassObj = NULL;
	Connection->PackageMap->SerializeObject( Bunch, UObject::StaticClass(), SubObjClassObj, &ClassNetGUID );

	// Delete sub-object
	if ( !ClassNetGUID.IsValid() )
	{
		if ( SubObj )
		{
			// Stop tracking this sub-object
			CreateSubObjects.Remove( SubObj );

			Actor->OnSubobjectDestroyFromReplication( SubObj );

			SubObj->MarkPendingKill();
		}
		bObjectDeleted = true;
		return NULL;
	}

	UClass * SubObjClass = Cast< UClass >( SubObjClassObj );

	if ( SubObjClass == NULL )
	{
		UE_LOG( LogNetTraffic, Warning, TEXT( "UActorChannel::ReadContentBlockHeader: Unable to read sub-object class. Actor: %s" ), *Actor->GetName() );

		// Valid NetGUID but no class was resolved - this is an error
		if ( SubObj == NULL )
		{
			// (unless we're using replays, which could be backwards compatibility kicking in)
			if ( !Connection->InternalAck )
			{
				UE_LOG( LogNetTraffic, Error, TEXT( "UActorChannel::ReadContentBlockHeader: Unable to read sub-object class (SubObj == NULL). Actor: %s" ), *Actor->GetName() );
				Bunch.SetError();
			}

			return NULL;
		}
	}
	else
	{
		if ( SubObjClass == UObject::StaticClass() )
		{
			UE_LOG( LogNetTraffic, Error, TEXT( "UActorChannel::ReadContentBlockHeader: SubObjClass == UObject::StaticClass(). Actor: %s" ), *Actor->GetName() );
			Bunch.SetError();
			return NULL;
		}

		if ( SubObjClass->IsChildOf( AActor::StaticClass() ) )
		{
			UE_LOG( LogNetTraffic, Error, TEXT( "UActorChannel::ReadContentBlockHeader: Sub-object cannot be actor class. Actor: %s" ), *Actor->GetName() );
			Bunch.SetError();
			return NULL;
		}
	}

	if ( SubObj == NULL )
	{
		check( !IsServer );

		// Construct the sub-object
		UE_LOG( LogNetTraffic, Log, TEXT( "UActorChannel::ReadContentBlockHeader: Instantiating sub-object. Class: %s, Actor: %s" ), *SubObjClass->GetName(), *Actor->GetName() );

		SubObj = NewObject< UObject >(Actor, SubObjClass);

		// Sanity check some things
		check( SubObj != NULL );
		check( SubObj->IsIn( Actor ) );
		check( Cast< AActor >( SubObj ) == NULL );

		// Notify actor that we created a component from replication
		Actor->OnSubobjectCreatedFromReplication( SubObj );
		
		// Register the component guid
		Connection->Driver->GuidCache->RegisterNetGUID_Client( NetGUID, SubObj );

		// Track which sub-object guids we are creating
		CreateSubObjects.AddUnique( SubObj );
	}

	return SubObj;
}

FObjectReplicator & UActorChannel::GetActorReplicationData()
{
	return ReplicationMap.FindChecked(Actor).Get();
}

TSharedRef< FObjectReplicator > & UActorChannel::FindOrCreateReplicator( UObject* Obj )
{
	// First, try to find it on the channel replication map
	TSharedRef<FObjectReplicator> * ReplicatorRefPtr = ReplicationMap.Find( Obj );

	if ( ReplicatorRefPtr == NULL )
	{
		// Didn't find it. 
		// Try to find in the dormancy map
		ReplicatorRefPtr = Connection->DormantReplicatorMap.Find( Obj );

		if ( ReplicatorRefPtr == NULL )
		{
			// Still didn't find one, need to create
			UE_LOG( LogNetTraffic, Log, TEXT( "Creating Replicator for %s" ), *Obj->GetName() );

			ReplicatorRefPtr = new TSharedRef<FObjectReplicator>( new FObjectReplicator() );
			ReplicatorRefPtr->Get().InitWithObject( Obj, Connection, true );
		}

		// Add to the replication map
		ReplicationMap.Add( Obj, *ReplicatorRefPtr );

		// Remove from dormancy map in case we found it there
		Connection->DormantReplicatorMap.Remove( Obj );

		// Start replicating with this replicator
		ReplicatorRefPtr->Get().StartReplicating( this );
	}

	return *ReplicatorRefPtr;
}

bool UActorChannel::ObjectHasReplicator(UObject *Obj)
{
	return ReplicationMap.Contains(Obj);
}

bool UActorChannel::KeyNeedsToReplicate(int32 ObjID, int32 RepKey)
{
	int32 &MapKey = SubobjectRepKeyMap.FindOrAdd(ObjID);
	if (MapKey == RepKey)
		return false;

	MapKey = RepKey;
	PendingObjKeys.Add(ObjID);
	return true;
}

bool UActorChannel::ReplicateSubobject(UObject *Obj, FOutBunch &Bunch, const FReplicationFlags &RepFlags)
{
	// Hack for now: subobjects are SupportsObject==false until they are replicated via ::ReplicateSUbobject, and then we make them supported
	// here, by forcing the packagemap to give them a NetGUID.
	//
	// Once we can lazily handle unmapped references on the client side, this can be simplified.
	if ( !Connection->Driver->GuidCache->SupportsObject( Obj ) )
	{
		FNetworkGUID NetGUID = Connection->Driver->GuidCache->AssignNewNetGUID_Server( Obj );	//Make sure he gets a NetGUID so that he is now 'supported'
	}

	bool NewSubobject = false;
	if (!ObjectHasReplicator(Obj))
	{
		// This is the first time replicating this subobject
		// This bunch should be reliable and we should always return true
		// even if the object properties did not diff from the CDO
		// (this will ensure the content header chunk is sent which is all we care about
		// to spawn this on the client).
		Bunch.bReliable = true;
		NewSubobject = true;
	}
	bool WroteSomething = FindOrCreateReplicator(Obj).Get().ReplicateProperties(Bunch, RepFlags);
	if (NewSubobject && !WroteSomething)
	{
		BeginContentBlock( Obj, Bunch );
		WroteSomething= true;
		EndContentBlock( Obj, Bunch );
	}

	return WroteSomething;
}

//------------------------------------------------------

static void	DebugNetGUIDs( UWorld* InWorld )
{
	UNetDriver *NetDriver = InWorld->NetDriver;
	if (!NetDriver)
	{
		return;
	}

	UNetConnection * Connection = (NetDriver->ServerConnection ? NetDriver->ServerConnection : (NetDriver->ClientConnections.Num() > 0 ? NetDriver->ClientConnections[0] : NULL));
	if (!Connection)
	{
		return;
	}

	Connection->PackageMap->LogDebugInfo(*GLog);
}

FAutoConsoleCommandWithWorld DormantActorCommand(
	TEXT("net.ListNetGUIDs"), 
	TEXT( "Lists NetGUIDs for actors" ), 
	FConsoleCommandWithWorldDelegate::CreateStatic(DebugNetGUIDs)
	);

//------------------------------------------------------

static void	ListOpenActorChannels( UWorld* InWorld )
{
	UNetDriver *NetDriver = InWorld->NetDriver;
	if (!NetDriver)
	{
		return;
	}

	UNetConnection * Connection = (NetDriver->ServerConnection ? NetDriver->ServerConnection : (NetDriver->ClientConnections.Num() > 0 ? NetDriver->ClientConnections[0] : NULL));
	if (!Connection)
	{
		return;
	}

	TMap<UClass*, int32> ClassMap;
	
	for (auto It = Connection->ActorChannels.CreateIterator(); It; ++It)
	{
		UActorChannel* Chan = It.Value();
		UClass *ThisClass = Chan->Actor->GetClass();
		while( Cast<UBlueprintGeneratedClass>(ThisClass))
		{
			ThisClass = ThisClass->GetSuperClass();
		}

		UE_LOG(LogNet, Warning, TEXT("Chan[%d] %s "), Chan->ChIndex, *Chan->Actor->GetFullName() );

		int32 &cnt = ClassMap.FindOrAdd(ThisClass);
		cnt++;
	}

	// Sort by the order in which categories were edited
	struct FCompareActorClassCount
	{
		FORCEINLINE bool operator()( int32 A, int32 B ) const
		{
			return A < B;
		}
	};
	ClassMap.ValueSort( FCompareActorClassCount() );

	UE_LOG(LogNet, Warning, TEXT("-----------------------------") );

	for (auto It = ClassMap.CreateIterator(); It; ++It)
	{
		UE_LOG(LogNet, Warning, TEXT("%4d - %s"), It.Value(), *It.Key()->GetName() );
	}
}

FAutoConsoleCommandWithWorld ListOpenActorChannelsCommand(
	TEXT("net.ListActorChannels"), 
	TEXT( "Lists open actor channels" ), 
	FConsoleCommandWithWorldDelegate::CreateStatic(ListOpenActorChannels)
	);

//------------------------------------------------------

static void	DeleteDormantActor( UWorld* InWorld )
{
	UNetDriver *NetDriver = InWorld->NetDriver;
	if (!NetDriver)
	{
		return;
	}

	UNetConnection * Connection = (NetDriver->ServerConnection ? NetDriver->ServerConnection : (NetDriver->ClientConnections.Num() > 0 ? NetDriver->ClientConnections[0] : NULL));
	if (!Connection)
	{
		return;
	}

	for (auto It = Connection->DormantActors.CreateIterator(); It; ++It)
	{
		AActor* ThisActor = const_cast<AActor*>(*It);

		UE_LOG(LogNet, Warning, TEXT("Deleting actor %s"), *ThisActor->GetName());

		FBox Box = ThisActor->GetComponentsBoundingBox();
		
		DrawDebugBox( InWorld, Box.GetCenter(), Box.GetExtent(), FQuat::Identity, FColor::Red, true, 30 );

		ThisActor->Destroy();

		break;
	}
}

FAutoConsoleCommandWithWorld DeleteDormantActorCommand(
	TEXT("net.DeleteDormantActor"), 
	TEXT( "Lists open actor channels" ), 
	FConsoleCommandWithWorldDelegate::CreateStatic(DeleteDormantActor)
	);

//------------------------------------------------------
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
static void	FindNetGUID( const TArray<FString>& Args, UWorld* InWorld )
{
	for (FObjectIterator ObjIt(UNetDriver::StaticClass()); ObjIt; ++ObjIt)
	{
		UNetDriver * Driver = Cast< UNetDriver >( *ObjIt );

		if ( Driver->HasAnyFlags( RF_ClassDefaultObject | RF_ArchetypeObject ) )
		{
			continue;
		}

		if (Args.Num() <= 0)
		{
			// Display all
			for (auto It = Driver->GuidCache->History.CreateIterator(); It; ++It)
			{
				FString Str = It.Value();
				FNetworkGUID NetGUID = It.Key();
				UE_LOG(LogNet, Warning, TEXT("<%s> - %s"), *NetGUID.ToString(), *Str);
			}
		}
		else
		{
			uint32 GUIDValue=0;
			TTypeFromString<uint32>::FromString(GUIDValue, *Args[0]);
			FNetworkGUID NetGUID(GUIDValue);

			// Search
			FString Str = Driver->GuidCache->History.FindRef(NetGUID);

			if (!Str.IsEmpty())
			{
				UE_LOG(LogNet, Warning, TEXT("Found: %s"), *Str);
			}
			else
			{
				UE_LOG(LogNet, Warning, TEXT("No matches") );
			}
		}
	}
}

FAutoConsoleCommandWithWorldAndArgs FindNetGUIDCommand(
	TEXT("net.Packagemap.FindNetGUID"), 
	TEXT( "Looks up object that was assigned a given NetGUID" ), 
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(FindNetGUID)
	);
#endif

//------------------------------------------------------

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
static void	TestObjectRefSerialize( const TArray<FString>& Args, UWorld* InWorld )
{
	if (!InWorld || Args.Num() <= 0)
		return;

	UObject* Object = StaticFindObject( UObject::StaticClass(), NULL, *Args[0], false );
	if (!Object)
	{
		Object = StaticLoadObject( UObject::StaticClass(), NULL, *Args[0], NULL, LOAD_NoWarn );
	}

	if (!Object)
	{
		UE_LOG(LogNet, Warning, TEXT("Couldn't find object: %s"), *Args[0]);
		return;
	}

	UE_LOG(LogNet, Warning, TEXT("Repping reference to: %s"), *Object->GetName());

	UNetDriver *NetDriver = InWorld->GetNetDriver();

	for (int32 i=0; i < NetDriver->ClientConnections.Num(); ++i)
	{
		if ( NetDriver->ClientConnections[i] && NetDriver->ClientConnections[i]->PackageMap )
		{
			FBitWriter TempOut( 1024 * 10, true);
			NetDriver->ClientConnections[i]->PackageMap->SerializeObject(TempOut, UObject::StaticClass(), Object, NULL);
		}
	}

	/*
	for( auto Iterator = InWorld->GetPlayerControllerIterator(); Iterator; ++Iterator )
	{
		APlayerController* PlayerController = *Iterator;
		PlayerController->ClientRepObjRef(Object);
	}
	*/
}

FAutoConsoleCommandWithWorldAndArgs TestObjectRefSerializeCommand(
	TEXT("net.TestObjRefSerialize"), 
	TEXT( "Attempts to replicate an object reference to all clients" ), 
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(TestObjectRefSerialize)
	);
#endif

