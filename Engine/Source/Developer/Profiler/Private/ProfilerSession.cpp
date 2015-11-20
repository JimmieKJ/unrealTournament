// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ProfilerPrivatePCH.h"

/*-----------------------------------------------------------------------------
	FProfilerStat, FProfilerGroup
-----------------------------------------------------------------------------*/

FProfilerStat FProfilerStat::Default;
FProfilerGroup FProfilerGroup::Default;

FProfilerStat::FProfilerStat( const uint32 InStatID /*= 0 */ ) 
	: _Name( TEXT("(Stat-Default)") )
	, _OwningGroupPtr( FProfilerGroup::GetDefaultPtr() )
	, _ID( InStatID )
	, _Type( EProfilerSampleTypes::InvalidOrMax )
{}

/*-----------------------------------------------------------------------------
	FProfilerSession
-----------------------------------------------------------------------------*/

FProfilerSession::FProfilerSession( EProfilerSessionTypes::Type InSessionType, const ISessionInstanceInfoPtr InSessionInstanceInfo, FGuid InSessionInstanceID, FString InDataFilepath )
: ClientStatMetadata( nullptr )
, bRequestStatMetadataUpdate( false )
, bLastPacket( false )
, StatMetaDataSize( 0 )
, OnTick( FTickerDelegate::CreateRaw( this, &FProfilerSession::HandleTicker ) )
, DataProvider( MakeShareable( new FArrayDataProvider() ) )
, StatMetaData( MakeShareable( new FProfilerStatMetaData() ) )
, EventGraphDataTotal( MakeShareable( new FEventGraphData() ) )
, EventGraphDataMaximum( MakeShareable( new FEventGraphData() ) )
, EventGraphDataCurrent( MakeShareable( new FEventGraphData() ) )
, CreationTime( FDateTime::Now() )
, SessionType( InSessionType )
, SessionInstanceInfo( InSessionInstanceInfo )
, SessionInstanceID( InSessionInstanceID )
, DataFilepath( InDataFilepath )
, bDataPreviewing( false )
, bDataCapturing( false )
, bHasAllProfilerData( false )

, FPSAnalyzer( MakeShareable( new FFPSAnalyzer( 5, 0, 60 ) ) )
{}

FProfilerSession::FProfilerSession( const ISessionInstanceInfoPtr InSessionInstanceInfo )
: ClientStatMetadata( nullptr )
, bRequestStatMetadataUpdate( false )
, bLastPacket( false )
, StatMetaDataSize( 0 )
, OnTick( FTickerDelegate::CreateRaw( this, &FProfilerSession::HandleTicker ) )
, DataProvider( MakeShareable( new FArrayDataProvider() ) )
, StatMetaData( MakeShareable( new FProfilerStatMetaData() ) )
, EventGraphDataTotal( MakeShareable( new FEventGraphData() ) )
, EventGraphDataMaximum( MakeShareable( new FEventGraphData() ) )
, EventGraphDataCurrent( MakeShareable( new FEventGraphData() ) )
, CreationTime( FDateTime::Now() )
, SessionType( EProfilerSessionTypes::Live )
, SessionInstanceInfo( InSessionInstanceInfo )
, SessionInstanceID( InSessionInstanceInfo->GetInstanceId() )
, DataFilepath( TEXT("") )
, bDataPreviewing( false )
, bDataCapturing( false )
, bHasAllProfilerData( false )

, FPSAnalyzer( MakeShareable( new FFPSAnalyzer( 5, 0, 60 ) ) )
{
	// Randomize creation time to test loading profiler captures with different creation time and different amount of data.
	CreationTime = FDateTime::Now() += FTimespan( 0, 0, FMath::RandRange( 2, 8 ) );
	OnTickHandle = FTicker::GetCoreTicker().AddTicker( OnTick );
}

FProfilerSession::FProfilerSession( const FString& InDataFilepath )
: ClientStatMetadata( nullptr )
, bRequestStatMetadataUpdate( false )
, bLastPacket( false )
, StatMetaDataSize( 0 )
, OnTick( FTickerDelegate::CreateRaw( this, &FProfilerSession::HandleTicker ) )
, DataProvider( MakeShareable( new FArrayDataProvider() ) )
, StatMetaData( MakeShareable( new FProfilerStatMetaData() ) )
, EventGraphDataTotal( MakeShareable( new FEventGraphData() ) )
, EventGraphDataMaximum( MakeShareable( new FEventGraphData() ) )
, EventGraphDataCurrent( MakeShareable( new FEventGraphData() ) )
, CreationTime( FDateTime::Now() )
, SessionType( EProfilerSessionTypes::StatsFile )
, SessionInstanceInfo( nullptr )
, SessionInstanceID( FGuid::NewGuid() )
, DataFilepath( InDataFilepath.Replace( *FStatConstants::StatsFileExtension, TEXT( "" ) ) )
, bDataPreviewing( false )
, bDataCapturing( false )
, bHasAllProfilerData( false )

, FPSAnalyzer( MakeShareable( new FFPSAnalyzer( 5, 0, 60 ) ) )
{
	// Randomize creation time to test loading profiler captures with different creation time and different amount of data.
	CreationTime = FDateTime::Now() += FTimespan( 0, 0, FMath::RandRange( 2, 8 ) );
	OnTickHandle = FTicker::GetCoreTicker().AddTicker( OnTick );
}

FProfilerSession::~FProfilerSession()
{
	FTicker::GetCoreTicker().RemoveTicker( OnTickHandle );	
}

FGraphDataSourceRefConst FProfilerSession::CreateGraphDataSource( const uint32 InStatID )
{
	FGraphDataSource* GraphDataSource = new FGraphDataSource( AsShared(), InStatID );
	return MakeShareable( GraphDataSource );
}

FEventGraphDataRef FProfilerSession::CreateEventGraphData( const uint32 FrameStartIndex, const uint32 FrameEndIndex, const EEventGraphTypes::Type EventGraphType )
{
	static FTotalTimeAndCount Current(0.0f, 0);
	PROFILER_SCOPE_LOG_TIME( TEXT( "FProfilerSession::CreateEventGraphData" ), &Current );

	FEventGraphData* EventGraphData = new FEventGraphData();

	if( EventGraphType == EEventGraphTypes::Average )
	{
		for( uint32 FrameIndex = FrameStartIndex; FrameIndex < FrameEndIndex+1; ++FrameIndex )
		{
			// Create a temporary event graph data for the specified frame.
			const FEventGraphData CurrentEventGraphData( AsShared(), FrameIndex );
			EventGraphData->CombineAndAdd( CurrentEventGraphData );
		}
	
		EventGraphData->Advance( FrameStartIndex, FrameEndIndex+1 );
		EventGraphData->Divide( (double)EventGraphData->GetNumFrames() );
	}
	else if( EventGraphType == EEventGraphTypes::Maximum )
	{
		for( uint32 FrameIndex = FrameStartIndex; FrameIndex < FrameEndIndex+1; ++FrameIndex )
		{
			// Create a temporary event graph data for the specified frame.
			const FEventGraphData CurrentEventGraphData( AsShared(), FrameIndex );
			EventGraphData->CombineAndFindMax( CurrentEventGraphData );
		}
		EventGraphData->Advance( FrameStartIndex, FrameEndIndex+1 );
	}

	return MakeShareable( EventGraphData );
}

void FProfilerSession::UpdateAggregatedStats( const uint32 FrameIndex )
{
	const FIntPoint& IndicesForFrame = DataProvider->GetSamplesIndicesForFrame( FrameIndex );
	const uint32 SampleStartIndex = IndicesForFrame.X;
	const uint32 SampleEndIndex = IndicesForFrame.Y;

	const FProfilerSampleArray& Collection = DataProvider->GetCollection();

	for( uint32 SampleIndex = SampleStartIndex; SampleIndex < SampleEndIndex; SampleIndex++ )
	{
		const FProfilerSample& ProfilerSample = Collection[ SampleIndex ];
		
		const uint32 StatID = ProfilerSample.StatID();
		FProfilerAggregatedStat* AggregatedStat = AggregatedStats.Find( StatID );
		if( !AggregatedStat )
		{
			const FProfilerStat& ProfilerStat = GetMetaData()->GetStatByID( StatID );
			AggregatedStat = &AggregatedStats.Add( ProfilerSample.StatID(), FProfilerAggregatedStat( ProfilerStat.Name(), ProfilerStat.OwningGroup().Name(), ProfilerSample.Type() ) );
		}
		(*AggregatedStat) += ProfilerSample;
	}

	for( auto It = AggregatedStats.CreateIterator(); It; ++It )
	{
		FProfilerAggregatedStat& AggregatedStat = It.Value();
		AggregatedStat.Advance();
	}

	// @TODO: Create map for stats TMap<uint32, TArray<uint32> >; StatID -> Sample indices for faster lookup in data providers
}

void FProfilerSession::EventGraphCombineAndMax( const FEventGraphDataRef Current, const uint32 NumFrames )
{
	EventGraphDataMaximum->CombineAndFindMax( Current.Get() );
	EventGraphDataMaximum->Advance( 0, NumFrames );
}

void FProfilerSession::EventGraphCombineAndAdd( const FEventGraphDataRef Current, const uint32 NumFrames )
{
	EventGraphDataTotal->CombineAndAdd( Current.Get() );
	EventGraphDataTotal->Advance( 0, NumFrames );
}

void FProfilerSession::UpdateAggregatedEventGraphData( const uint32 FrameIndex )
{
	if( CompletionSync.GetReference() && !CompletionSync->IsComplete() )
	{
		static FTotalTimeAndCount JoinTasksTimeAndCont(0.0f, 0);
		PROFILER_SCOPE_LOG_TIME( TEXT( "FProfilerSession::CombineJoinAndContinue" ), &JoinTasksTimeAndCont );

		FTaskGraphInterface::Get().WaitUntilTaskCompletes(CompletionSync, ENamedThreads::GameThread);
	}

	static FTotalTimeAndCount TimeAndCount(0.0f, 0);
	PROFILER_SCOPE_LOG_TIME( TEXT( "FProfilerSession::UpdateAggregatedEventGraphData" ), &TimeAndCount );

	// Create a temporary event graph data for the specified frame.
	EventGraphDataCurrent = MakeShareable( new FEventGraphData( AsShared(), FrameIndex ) );
	const uint32 NumFrames = DataProvider->GetNumFrames();

	static const bool bUseTaskGraph = true;

	if( bUseTaskGraph )
	{
		FGraphEventArray EventGraphCombineTasks;

		DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.EventGraphData.CombineAndFindMax"),
			STAT_FSimpleDelegateGraphTask_EventGraphData_CombineAndFindMax,
			STATGROUP_TaskGraphTasks);
		DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.EventGraphData.EventGraphCombineAndAdd"),
			STAT_FSimpleDelegateGraphTask_EventGraphData_EventGraphCombineAndAdd,
			STATGROUP_TaskGraphTasks);

		new (EventGraphCombineTasks)FGraphEventRef(FSimpleDelegateGraphTask::CreateAndDispatchWhenReady
		(
			FSimpleDelegateGraphTask::FDelegate::CreateRaw( this, &FProfilerSession::EventGraphCombineAndMax, EventGraphDataCurrent, NumFrames ), 
			GET_STATID(STAT_FSimpleDelegateGraphTask_EventGraphData_CombineAndFindMax), nullptr
		));

		new (EventGraphCombineTasks) FGraphEventRef(FSimpleDelegateGraphTask::CreateAndDispatchWhenReady
		(
			FSimpleDelegateGraphTask::FDelegate::CreateRaw( this, &FProfilerSession::EventGraphCombineAndAdd, EventGraphDataCurrent, NumFrames ), 
			GET_STATID(STAT_FSimpleDelegateGraphTask_EventGraphData_EventGraphCombineAndAdd), nullptr
		));

		DECLARE_CYCLE_STAT(TEXT("FNullGraphTask.EventGraphData.CombineJoinAndContinue"),
			STAT_FNullGraphTask_EventGraphData_CombineJoinAndContinue,
			STATGROUP_TaskGraphTasks);

		// JoinThreads
		CompletionSync = TGraphTask<FNullGraphTask>::CreateTask( &EventGraphCombineTasks, ENamedThreads::GameThread )
			.ConstructAndDispatchWhenReady(GET_STATID(STAT_FNullGraphTask_EventGraphData_CombineJoinAndContinue), ENamedThreads::AnyThread);
	}
	else
	{
		EventGraphCombineAndMax( EventGraphDataCurrent, NumFrames );
		EventGraphCombineAndAdd( EventGraphDataCurrent, NumFrames );
	}
}

bool FProfilerSession::HandleTicker( float DeltaTime )
{
	static FTotalTimeAndCount Current( 0.0f, 0 );
	PROFILER_SCOPE_LOG_TIME( TEXT( "FProfilerSession::HandleTicker" ), &Current );

	// Update metadata if needed
	if( bRequestStatMetadataUpdate )
	{
		StatMetaData->Update( *ClientStatMetadata );
		bRequestStatMetadataUpdate = false;
	}

	// Limit processing to 30ms per frame.
	const double TimeLimit = 30 / 1000.0;
	double Seconds = 0;
	
	for( int32 FrameNumber = 0; FrameNumber < FrameToProcess.Num(); FrameNumber++ )
	{	
		if( Seconds > TimeLimit )
		{
			break;
		}

		FSimpleScopeSecondsCounter SecondsCounter(Seconds);

		const uint32 FrameIndex = FrameToProcess[0];
		FrameToProcess.RemoveAt( 0 );

		FProfilerDataFrame& CurrentProfilerData = FrameToProfilerDataMapping.FindChecked( FrameIndex );

		const FProfilerStatMetaDataRef MetaData = GetMetaData();
		TMap<uint32, float> ThreadMS;

		// Preprocess the hierarchical samples for the specified frame.
		const TMap<uint32, FProfilerCycleGraph>& CycleGraphs = CurrentProfilerData.CycleGraphs;

		// Add a root sample for this frame.
		// HACK 2013-07-19, 13:44 STAT_Root == ThreadRoot in stats2
		const uint32 FrameRootSampleIndex = DataProvider->AddHierarchicalSample( 0, MetaData->GetStatByID(1).OwningGroup().ID(), 1, 0.0f, 0.0f, 1 );
		
		double GameThreadTimeMS = 0.0f;
		double MaxThreadTimeMS = 0.0f;

		double ThreadStartTimeMS = 0.0;
		for( auto ThreadIt = CycleGraphs.CreateConstIterator(); ThreadIt; ++ThreadIt )
		{
			const uint32 ThreadID = ThreadIt.Key();
			const FProfilerCycleGraph& ThreadGraph = ThreadIt.Value();

			// Calculate total time for this thread.
			double ThreadDurationTimeMS = 0.0;
			ThreadStartTimeMS = CurrentProfilerData.FrameStart;
			for( int32 Index = 0; Index < ThreadGraph.Children.Num(); Index++ )
			{
				ThreadDurationTimeMS += MetaData->ConvertCyclesToMS( ThreadGraph.Children[Index].Value );
			}

			if (ThreadDurationTimeMS > 0.0)
			{
				// Check for game thread.
				const FString GameThreadName = FName( NAME_GameThread ).GetPlainNameString();
				const bool bGameThreadFound = MetaData->GetThreadDescriptions().FindChecked( ThreadID ).Contains( GameThreadName );
				if( bGameThreadFound )
				{
					GameThreadTimeMS = ThreadDurationTimeMS;
				}

				// Add a root sample for each thread.
				const uint32& NewThreadID = MetaData->ThreadIDtoStatID.FindChecked( ThreadID );
				const uint32 ThreadRootSampleIndex = DataProvider->AddHierarchicalSample
				( 
					NewThreadID/*ThreadID*/, 
					MetaData->GetStatByID(NewThreadID).OwningGroup().ID(), 
					NewThreadID, 
					ThreadStartTimeMS, 
					ThreadDurationTimeMS, 
					1, 
					FrameRootSampleIndex 
				);
				ThreadMS.FindOrAdd( ThreadID ) = (float)ThreadDurationTimeMS;

				// Recursively add children and parent to the root samples.
				for( int32 Index = 0; Index < ThreadGraph.Children.Num(); Index++ )
				{
					const FProfilerCycleGraph& CycleGraph = ThreadGraph.Children[Index];
					const double CycleDurationMS = MetaData->ConvertCyclesToMS( CycleGraph.Value );
					const double CycleStartTimeMS = ThreadStartTimeMS;

					if (CycleDurationMS > 0.0)
					{
						PopulateHierarchy_Recurrent( CycleGraph, CycleStartTimeMS, CycleDurationMS, ThreadRootSampleIndex );
					}
					ThreadStartTimeMS += CycleDurationMS;
				}

				MaxThreadTimeMS = FMath::Max( MaxThreadTimeMS, ThreadDurationTimeMS );
			}
		}

		// Fix the root stat time.
		FProfilerSampleArray& MutableCollection = const_cast<FProfilerSampleArray&>(DataProvider->GetCollection());
		MutableCollection[FrameRootSampleIndex].SetDurationMS( GameThreadTimeMS != 0.0f ? GameThreadTimeMS : MaxThreadTimeMS );

		FPSAnalyzer->AddSample( GameThreadTimeMS > 0.0f ? 1000.0f/GameThreadTimeMS : 0.0f );

		// Process the non-hierarchical samples for the specified frame.
		{
			// Process integer counters.
			for( int32 Index = 0; Index < CurrentProfilerData.CountAccumulators.Num(); Index++ )
			{
				const FProfilerCountAccumulator& IntCounter = CurrentProfilerData.CountAccumulators[Index];
				const EProfilerSampleTypes::Type ProfilerSampleType = MetaData->GetSampleTypeForStatID( IntCounter.StatId );
				DataProvider->AddCounterSample( MetaData->GetStatByID(IntCounter.StatId).OwningGroup().ID(), IntCounter.StatId, (double)IntCounter.Value, ProfilerSampleType );
			}

			// Process floating point counters.
			for( int32 Index = 0; Index < CurrentProfilerData.FloatAccumulators.Num(); Index++ )
			{
				const FProfilerFloatAccumulator& FloatCounter = CurrentProfilerData.FloatAccumulators[Index];
				DataProvider->AddCounterSample( MetaData->GetStatByID(FloatCounter.StatId).OwningGroup().ID(), FloatCounter.StatId, (double)FloatCounter.Value, EProfilerSampleTypes::NumberFloat );
			}
		}

		// Advance frame
		const uint32 LastFrameIndex = DataProvider->GetNumFrames();
		DataProvider->AdvanceFrame( MaxThreadTimeMS );

		// Update aggregated stats
		UpdateAggregatedStats( LastFrameIndex );

		// Update aggregated events.
		UpdateAggregatedEventGraphData( LastFrameIndex );

		// Update mini-view.
		OnAddThreadTime.ExecuteIfBound( LastFrameIndex, ThreadMS, StatMetaData );

		FrameToProfilerDataMapping.Remove( FrameIndex );
	}	

	if( SessionType == EProfilerSessionTypes::StatsFile )
	{
		if( FrameToProcess.Num() == 0 && bHasAllProfilerData )
		{
			// Broadcast that a capture file has been fully processed.
			OnCaptureFileProcessed.ExecuteIfBound( GetInstanceID() );

			// Disable tick method as we no longer need to tick.
			return false;
		}
	}

	return true;
}

void FProfilerSession::PopulateHierarchy_Recurrent
( 
	const FProfilerCycleGraph& ParentGraph, 
	const double ParentStartTimeMS, 
	const double ParentDurationMS,
	const uint32 ParentSampleIndex 
)
{
	const FProfilerStatMetaDataRef MetaData = GetMetaData();
	const uint32& ThreadID = MetaData->ThreadIDtoStatID.FindChecked( ParentGraph.ThreadId );

	const uint32 SampleIndex = DataProvider->AddHierarchicalSample
	( 
		ThreadID, 
		MetaData->GetStatByID(ParentGraph.StatId).OwningGroup().ID(), 
		ParentGraph.StatId, 
		ParentStartTimeMS, ParentDurationMS, 
		ParentGraph.CallsPerFrame, 
		ParentSampleIndex 
	);

	double ChildStartTimeMS = ParentStartTimeMS;
	double ChildrenDurationMS = 0.0f;

	for( int32 DataIndex = 0; DataIndex < ParentGraph.Children.Num(); DataIndex++ )
	{
		const FProfilerCycleGraph& ChildCyclesCounter = ParentGraph.Children[DataIndex];
		const double ChildDurationMS = MetaData->ConvertCyclesToMS( ChildCyclesCounter.Value );

		if (ChildDurationMS > 0.0)
		{
			PopulateHierarchy_Recurrent( ChildCyclesCounter, ChildStartTimeMS, ChildDurationMS, SampleIndex );
		}
		ChildStartTimeMS += ChildDurationMS;
		ChildrenDurationMS += ChildDurationMS;
	}

	const double SelfTimeMS = ParentDurationMS - ChildrenDurationMS;
	if( SelfTimeMS > 0.0f && ParentGraph.Children.Num() > 0 )
	{
		const FName& ParentStatName = MetaData->GetStatByID( ParentGraph.StatId ).Name();
		const FName& ParentGroupName = MetaData->GetStatByID( ParentGraph.StatId ).OwningGroup().Name();

		// Create a fake stat that represents this profiler sample's exclusive time.
		// This is required if we want to create correct combined event graphs later.
		DataProvider->AddHierarchicalSample
		(
			ThreadID,
			MetaData->GetStatByID(0).OwningGroup().ID(),
			0, // @see FProfilerStatMetaData.Update, 0 means "Self"
			ChildStartTimeMS, 
			SelfTimeMS,
			1,
			SampleIndex
		);
	}
}

void FProfilerSession::LoadComplete()
{
	bHasAllProfilerData = true;
}

void FProfilerSession::UpdateProfilerData( const FProfilerDataFrame& Content )
{
	FrameToProfilerDataMapping.FindOrAdd( Content.Frame ) = Content;
	FrameToProcess.Add( Content.Frame );
}

void FProfilerSession::UpdateMetadata( const FStatMetaData& InClientStatMetaData )
{
	const uint32 CurrentStatMetaDataSize = InClientStatMetaData.GetMetaDataSize();
	if( CurrentStatMetaDataSize != StatMetaDataSize )
	{
		ClientStatMetadata = &InClientStatMetaData;
		bRequestStatMetadataUpdate = true;

		StatMetaDataSize = CurrentStatMetaDataSize;
	}
}

/*-----------------------------------------------------------------------------
	FRawProfilerSession
-----------------------------------------------------------------------------*/

FRawProfilerSession::FRawProfilerSession( const FString& InRawStatsFileFileath )
: FProfilerSession( EProfilerSessionTypes::StatsFileRaw, nullptr, FGuid::NewGuid(), InRawStatsFileFileath.Replace( *FStatConstants::StatsFileRawExtension, TEXT( "" ) ) )
, CurrentMiniViewFrame( 0 )
{
	OnTick = FTickerDelegate::CreateRaw( this, &FRawProfilerSession::HandleTicker );
}

FRawProfilerSession::~FRawProfilerSession()
{
	FTicker::GetCoreTicker().RemoveTicker( OnTickHandle );
}

bool FRawProfilerSession::HandleTicker( float DeltaTime )
{
#if	0
	StatsThreadStats;
	Stream;

	enum
	{
		MAX_NUM_DATA_PER_TICK = 30
	};
	int32 NumDataThisTick = 0;

	// Add the data to the mini-view.
	for( int32 FrameIndex = CurrentMiniViewFrame; FrameIndex < Stream.FramesInfo.Num(); ++FrameIndex )
	{
		const FStatsFrameInfo& StatsFrameInfo = Stream.FramesInfo[FrameIndex];
		// Convert from cycles to ms.
		TMap<uint32, float> ThreadMS;
		for( auto InnerIt = StatsFrameInfo.ThreadCycles.CreateConstIterator(); InnerIt; ++InnerIt )
		{
			ThreadMS.Add( InnerIt.Key(), StatMetaData->ConvertCyclesToMS( InnerIt.Value() ) );
		}

		// Pass the reference to the stats' metadata.
		// @TODO yrx 2014-04-03 Figure out something better later.
		OnAddThreadTime.ExecuteIfBound( FrameIndex, ThreadMS, StatMetaData );

		//CurrentMiniViewFrame++;
		NumDataThisTick++;
		if( NumDataThisTick > MAX_NUM_DATA_PER_TICK )
		{
			break;
		}
	}
#endif // 0

	return true;
}

static double GetSecondsPerCycle( const FStatPacketArray& Frame )
{
	const FName SecondsPerCycleFName = FName(TEXT("//STATGROUP_Engine//STAT_SecondsPerCycle///Seconds$32$Per$32$Cycle///////STATCAT_Advanced////"));
	double Result = 0;

	for( int32 PacketIndex = 0; PacketIndex < Frame.Packets.Num(); PacketIndex++ )
	{
		const FStatPacket& Packet = *Frame.Packets[PacketIndex];
		const FStatMessagesArray& Data = Packet.StatMessages;

		if( Packet.ThreadType == EThreadType::Game )
		{
			for( int32 Index = 0; Index < Data.Num(); Index++ )
			{
				const FStatMessage& Item = Data[Index];
				check( Item.NameAndInfo.GetFlag( EStatMetaFlags::DummyAlwaysOne ) ); 

				const FName LongName = Item.NameAndInfo.GetEncodedName();
				const FName RawName = Item.NameAndInfo.GetRawName();
				if( LongName.IsEqual( SecondsPerCycleFName, ENameCase::IgnoreCase, false ) )
				{
					Result = Item.GetValue_double();
					UE_LOG( LogStats, Log, TEXT( "STAT_SecondsPerCycle is %f [ns]" ), Result*1000*1000 );

					PacketIndex = Frame.Packets.Num();
					break;
				}
			}	
		}
			
	}
	return Result;
}

static int64 GetFastThreadFrameTimeInternal( const FStatPacketArray& Frame, EThreadType::Type ThreadType )
{
	int64 Result = 0;

	for( int32 PacketIndex = 0; PacketIndex < Frame.Packets.Num(); PacketIndex++ )
	{
		const FStatPacket& Packet = *Frame.Packets[PacketIndex];

		if( Packet.ThreadType == ThreadType )
		{
			const FStatMessagesArray& Data = Packet.StatMessages;
			for (int32 Index = 0; Index < Data.Num(); Index++)
			{
				FStatMessage const& Item = Data[Index];
				EStatOperation::Type Op = Item.NameAndInfo.GetField<EStatOperation>();
				FName LongName = Item.NameAndInfo.GetRawName();
				if (Op == EStatOperation::CycleScopeStart)
				{
					check(Item.NameAndInfo.GetFlag(EStatMetaFlags::IsCycle));
					Result -= Item.GetValue_int64();
					break;
				}
			}
			for (int32 Index = Data.Num() - 1; Index >= 0; Index--)
			{
				FStatMessage const& Item = Data[Index];
				EStatOperation::Type Op = Item.NameAndInfo.GetField<EStatOperation>();
				FName LongName = Item.NameAndInfo.GetRawName();
				if (Op == EStatOperation::CycleScopeEnd)
				{

					check(Item.NameAndInfo.GetFlag(EStatMetaFlags::IsCycle));
					Result += Item.GetValue_int64();
					break;
				}
			}
		}
	}
	return Result;
}

void FRawProfilerSession::PrepareLoading()
{
	SCOPE_LOG_TIME_FUNC();

	const FString Filepath = DataFilepath + FStatConstants::StatsFileRawExtension;
	const int64 Size = IFileManager::Get().FileSize( *Filepath );
	if( Size < 4 )
	{
		UE_LOG( LogStats, Error, TEXT( "Could not open: %s" ), *Filepath );
		return;
	}
	TAutoPtr<FArchive> FileReader( IFileManager::Get().CreateFileReader( *Filepath ) );
	if( !FileReader )
	{
		UE_LOG( LogStats, Error, TEXT( "Could not open: %s" ), *Filepath );
		return;
	}

	if( !Stream.ReadHeader( *FileReader ) )
	{
		UE_LOG( LogStats, Error, TEXT( "Could not open, bad magic: %s" ), *Filepath );
		return;
	}

	const bool bIsFinalized = Stream.Header.IsFinalized();
	check( bIsFinalized );
	check( Stream.Header.Version == EStatMagicWithHeader::VERSION_5 );
	StatsThreadStats.MarkAsLoaded();

	TArray<FStatMessage> Messages;
	if( Stream.Header.bRawStatsFile )
	{
		// Read metadata.
		TArray<FStatMessage> MetadataMessages;
		Stream.ReadFNamesAndMetadataMessages( *FileReader, MetadataMessages );
		StatsThreadStats.ProcessMetaDataOnly( MetadataMessages );

		const FName F00245 = FName(245, 245, 0);
		
		const FName F11602 = FName(11602, 11602, 0);
		const FName F06394 = FName(6394, 6394, 0);

		const int64 CurrentFilePos = FileReader->Tell();

		// Update profiler's metadata.
		StatMetaData->UpdateFromStatsState( StatsThreadStats );
		const uint32 GameThreadID = GetMetaData()->GetGameThreadID();

		// Read frames offsets.
		Stream.ReadFramesOffsets( *FileReader );

		// Buffer used to store the compressed and decompressed data.
		TArray<uint8> SrcArray;
		TArray<uint8> DestArray;
		const bool bHasCompressedData = Stream.Header.HasCompressedData();
		check(bHasCompressedData);

		TMap<int64, FStatPacketArray> CombinedHistory;
		int64 TotalPacketSize = 0;
		int64 MaximumPacketSize = 0;
		// Read all packets sequentially, force by the memory profiler which is now a part of the raw stats.
		// !!CAUTION!! Frame number in the raw stats is pointless, because it is time based, not frame based.
		// Background threads usually execute time consuming operations, so the frame number won't be valid.
		// Needs to be combined by the thread and the time, not by the frame number.
		{
			int64 FrameOffset0 = Stream.FramesInfo[0].FrameFileOffset;
			FileReader->Seek( FrameOffset0 );

			const int64 FileSize = FileReader->TotalSize();

			while( FileReader->Tell() < FileSize )
			{
				// Read the compressed data.
				FCompressedStatsData UncompressedData( SrcArray, DestArray );
				*FileReader << UncompressedData;
				if( UncompressedData.HasReachedEndOfCompressedData() )
				{
					break;
				}

				FMemoryReader MemoryReader( DestArray, true );

				FStatPacket* StatPacket = new FStatPacket();
				Stream.ReadStatPacket( MemoryReader, *StatPacket );
				
				const int64 FrameNum = StatPacket->Frame;
				FStatPacketArray& Frame = CombinedHistory.FindOrAdd(FrameNum);
			
				// Check if we need to combine packets from the same thread.
				FStatPacket** CombinedPacket = Frame.Packets.FindByPredicate([&](FStatPacket* Item) -> bool
				{
					return Item->ThreadId == StatPacket->ThreadId;
				});
				
				if( CombinedPacket )
				{
					(*CombinedPacket)->StatMessages += StatPacket->StatMessages;
				}
				else
				{
					Frame.Packets.Add(StatPacket);
				}

				const int64 CurrentPos = FileReader->Tell();
				const int32 PctPos = int32(100.0f*CurrentPos/FileSize);

				UE_LOG( LogStats, Log, TEXT( "%3i Processing FStatPacket: Frame %5i for thread %5i with %6i messages (%.1f MB)" ), 
					PctPos, 
					StatPacket->Frame, 
					StatPacket->ThreadId, 
					StatPacket->StatMessages.Num(), 
					StatPacket->StatMessages.GetAllocatedSize()/1024.0f/1024.0f );

				const int64 PacketSize = StatPacket->StatMessages.GetAllocatedSize();
				TotalPacketSize += PacketSize;
				MaximumPacketSize = FMath::Max( MaximumPacketSize, PacketSize );
			}
		}

		UE_LOG( LogStats, Log, TEXT( "TotalPacketSize: %.1f MB, Max: %1f MB" ), 
			TotalPacketSize/1024.0f/1024.0f, 
			MaximumPacketSize/1024.0f/1024.0f );

		TArray<int64> Frames;
		CombinedHistory.GenerateKeyArray(Frames);
		Frames.Sort();
		const int64 MiddleFrame = Frames[Frames.Num()/2];


		// Remove all frames without the game thread messages.
		for (int32 FrameIndex = 0; FrameIndex < Frames.Num(); ++FrameIndex)
		{
			const int64 TargetFrame = Frames[FrameIndex];
			const FStatPacketArray& Frame = CombinedHistory.FindChecked( TargetFrame );

			const double GameThreadTimeMS = GetMetaData()->ConvertCyclesToMS( GetFastThreadFrameTimeInternal( Frame, EThreadType::Game ) );

			if (GameThreadTimeMS == 0.0f)
			{
				CombinedHistory.Remove( TargetFrame );
				Frames.RemoveAt( FrameIndex );
				FrameIndex--;
			}
		}
		
	
		StatMetaData->SecondsPerCycle = GetSecondsPerCycle( CombinedHistory.FindChecked(MiddleFrame) );
		check( StatMetaData->GetSecondsPerCycle() > 0.0 );

		//const int32 FirstGameThreadFrame = FindFirstFrameWithGameThread( CombinedHistory, Frames );

		// Prepare profiler frame.
		{
			SCOPE_LOG_TIME( TEXT( "Preparing profiler frames" ), nullptr );

			// Prepare profiler frames.
			double ElapsedTimeMS = 0;

			for( int32 FrameIndex = 0; FrameIndex < Frames.Num(); ++FrameIndex )
			{
				const int64 TargetFrame = Frames[FrameIndex];
				const FStatPacketArray& Frame = CombinedHistory.FindChecked(TargetFrame);

				const double GameThreadTimeMS = GetMetaData()->ConvertCyclesToMS( GetFastThreadFrameTimeInternal(Frame,EThreadType::Game) );

				if( GameThreadTimeMS == 0.0f )
				{
					continue;
				}

				const double RenderThreadTimeMS = GetMetaData()->ConvertCyclesToMS( GetFastThreadFrameTimeInternal(Frame,EThreadType::Renderer) );

				// Update mini-view, convert from cycles to ms.
				TMap<uint32, float> ThreadTimesMS;
				ThreadTimesMS.Add( GameThreadID, GameThreadTimeMS );
				ThreadTimesMS.Add( GetMetaData()->GetRenderThreadID()[0], RenderThreadTimeMS );

				// Pass the reference to the stats' metadata.
				OnAddThreadTime.ExecuteIfBound( FrameIndex, ThreadTimesMS, StatMetaData );

				// Create a new profiler frame and add it to the stream.
				ElapsedTimeMS += GameThreadTimeMS;
				FProfilerFrame* ProfilerFrame = new FProfilerFrame( TargetFrame, GameThreadTimeMS, ElapsedTimeMS );
				ProfilerFrame->ThreadTimesMS = ThreadTimesMS;
				ProfilerStream.AddProfilerFrame( TargetFrame, ProfilerFrame );
			}
		}
	
		// Process the raw stats data.
		{
			SCOPE_LOG_TIME( TEXT( "Processing the raw stats" ), nullptr );

			double CycleCounterAdjustmentMS = 0.0f;

			// Read the raw stats messages.
			for( int32 FrameIndex = 0; FrameIndex < Frames.Num()-1; ++FrameIndex )
			{
				const int64 TargetFrame = Frames[FrameIndex];
				const FStatPacketArray& Frame = CombinedHistory.FindChecked(TargetFrame);

				FProfilerFrame* ProfilerFrame = ProfilerStream.GetProfilerFrame( FrameIndex );

				UE_CLOG( FrameIndex % 8 == 0, LogStats, Log, TEXT( "Processing raw stats frame: %4i/%4i" ), FrameIndex, Frames.Num() );

				ProcessStatPacketArray( Frame, *ProfilerFrame, FrameIndex ); // or ProfilerFrame->TargetFrame

				// Find the first cycle counter for the game thread.
				if( CycleCounterAdjustmentMS == 0.0f )
				{
					CycleCounterAdjustmentMS = ProfilerFrame->Root->CycleCounterStartTimeMS;
				}

				// Update thread time and mark profiler frame as valid and ready for use.
				ProfilerFrame->MarkAsValid();
			}

			// Adjust all profiler frames.
			ProfilerStream.AdjustCycleCounters( CycleCounterAdjustmentMS );
		}
	}

	const int64 AllocatedSize = ProfilerStream.GetAllocatedSize();

	// We have the whole metadata and basic information about the raw stats file, start ticking the profiler session.
	//OnTickHandle = FTicker::GetCoreTicker().AddTicker( OnTick, 0.25f );

#if	0
	if( SessionType == EProfilerSessionTypes::OfflineRaw )
	{
		// Broadcast that a capture file has been fully processed.
		OnCaptureFileProcessed.ExecuteIfBound( GetInstanceID() );
	}
#endif // 0
}


void FProfilerStatMetaData::Update( const FStatMetaData& ClientStatMetaData )
{
	PROFILER_SCOPE_LOG_TIME( TEXT( "FProfilerStatMetaData.Update" ), nullptr );

	FStatMetaData LocalCopy;
	{
		FScopeLock Lock( ClientStatMetaData.CriticalSection );
		LocalCopy = ClientStatMetaData;
	}

	// Iterate through all thread descriptions.
	ThreadDescriptions.Append( LocalCopy.ThreadDescriptions );

	// Initialize fake stat for Self.
	const uint32 NoGroupID = 0;

	InitializeGroup( NoGroupID, "NoGroup" );
	InitializeStat( 0, NoGroupID, TEXT( "Self" ), STATTYPE_CycleCounter );
	InitializeStat( 1, NoGroupID, FStatConstants::NAME_ThreadRoot.GetPlainNameString(), STATTYPE_CycleCounter, FStatConstants::NAME_ThreadRoot );

	// Iterate through all stat group descriptions.
	for( auto It = LocalCopy.GroupDescriptions.CreateConstIterator(); It; ++It )
	{
		const FStatGroupDescription& GroupDesc = It.Value();
		InitializeGroup( GroupDesc.ID, GroupDesc.Name );
	}

	// Iterate through all stat descriptions.
	for( auto It = LocalCopy.StatDescriptions.CreateConstIterator(); It; ++It )
	{
		const FStatDescription& StatDesc = It.Value();
		InitializeStat( StatDesc.ID, StatDesc.GroupID, StatDesc.Name, (EStatType)StatDesc.StatType );
	}

	SecondsPerCycle = LocalCopy.SecondsPerCycle;
}


void FProfilerStatMetaData::UpdateFromStatsState( const FStatsThreadState& StatsThreadStats )
{
	TMap<FName, int32> GroupFNameIDs;

	for( auto It = StatsThreadStats.Threads.CreateConstIterator(); It; ++It )
	{
		ThreadDescriptions.Add( It.Key(), It.Value().ToString() );
	}

	const uint32 NoGroupID = 0;
	const uint32 ThreadGroupID = 1;

	// Special groups.
	InitializeGroup( NoGroupID, "NoGroup" );

	// Self must be 0.
	InitializeStat( 0, NoGroupID, TEXT( "Self" ), STATTYPE_CycleCounter );

	// ThreadRoot must be 1.
	InitializeStat( 1, NoGroupID, FStatConstants::NAME_ThreadRoot.GetPlainNameString(), STATTYPE_CycleCounter, FStatConstants::NAME_ThreadRoot );

	int32 UniqueID = 15;

	TArray<FName> GroupFNames;
	StatsThreadStats.Groups.MultiFind( NAME_Groups, GroupFNames );
	for( const auto& GroupFName : GroupFNames  )
	{
		UniqueID++;
		InitializeGroup( UniqueID, GroupFName.ToString() );
		GroupFNameIDs.Add( GroupFName, UniqueID );
	}

	for( auto It = StatsThreadStats.ShortNameToLongName.CreateConstIterator(); It; ++It )
	{
		const FStatMessage& LongName = It.Value();
		
		const FName GroupName = LongName.NameAndInfo.GetGroupName();
		if( GroupName == NAME_Groups )
		{
			continue;
		}
		const int32 GroupID = GroupFNameIDs.FindChecked( GroupName );

		const FName StatName = It.Key();
		UniqueID++;

		EStatType StatType = STATTYPE_Error;
		if( LongName.NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_int64 )
		{
			if( LongName.NameAndInfo.GetFlag( EStatMetaFlags::IsCycle ) )
			{
				StatType = STATTYPE_CycleCounter;
			}
			else if( LongName.NameAndInfo.GetFlag( EStatMetaFlags::IsMemory ) )
			{
				StatType = STATTYPE_MemoryCounter;
			}
			else
			{
				StatType = STATTYPE_AccumulatorDWORD;
			}
		}
		else if( LongName.NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_double )
		{
			StatType = STATTYPE_AccumulatorFLOAT;
		}
		else if( LongName.NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_Ptr )
		{
			// Not supported at this moment.
			continue;
		}

		check( StatType != STATTYPE_Error );

		int32 StatID = UniqueID;
		// Some hackery.
		if( StatName == TEXT( "STAT_FrameTime" ) )
		{
			StatID = 2;
		}

		const FString Description = LongName.NameAndInfo.GetDescription();
		const FString StatDesc = !Description.IsEmpty() ? Description : StatName.ToString();

		InitializeStat( StatID, GroupID, StatDesc, StatType, StatName );

		// Setup thread id to stat id.
		if( GroupName == FStatConstants::NAME_ThreadGroup )
		{
			uint32 ThreadID = 0;
			for( auto ThreadsIt = StatsThreadStats.Threads.CreateConstIterator(); ThreadsIt; ++ThreadsIt )
			{
				if (ThreadsIt.Value() == StatName)
				{
					ThreadID = ThreadsIt.Key();
					break;
				}
			}
			ThreadIDtoStatID.Add( ThreadID, StatID );

			// Game thread is always NAME_GameThread
			if( StatName == NAME_GameThread )
			{
				GameThreadID = ThreadID;
			}
			// Rendering thread may be "Rendering thread" or NAME_RenderThread with an index
			else if( StatName.GetPlainNameString().Contains( FName(NAME_RenderThread).GetPlainNameString() ) )
			{
				RenderThreadIDs.AddUnique( ThreadID );
			}
			else if( StatName.GetPlainNameString().Contains( TEXT( "RenderingThread" ) ) )
			{
				RenderThreadIDs.AddUnique( ThreadID );
			}
		}
	}
}

void FRawProfilerSession::ProcessStatPacketArray( const FStatPacketArray& StatPacketArray, FProfilerFrame& out_ProfilerFrame, int32 FrameIndex )
{
	// @TODO yrx 2014-03-24 Standardize thread names and id
	// @TODO yrx 2014-04-22 Remove all references to the data provider, event graph etc once data graph can visualize.

	// Raw stats callstack for this stat packet array.
	TMap<FName,FProfilerStackNode*> ThreadNodes;

	const FProfilerStatMetaDataRef MetaData = GetMetaData();
	
	FProfilerSampleArray& MutableCollection = const_cast<FProfilerSampleArray&>(DataProvider->GetCollection());

	// Add a root sample for this frame.
	const uint32 FrameRootSampleIndex = DataProvider->AddHierarchicalSample( 0, MetaData->GetStatByID( 1 ).OwningGroup().ID(), 1, 0.0f, 0.0f, 1 );

	// Iterate through all stats packets and raw stats messages.
	FName GameThreadFName = NAME_None;
	for( int32 PacketIndex = 0; PacketIndex < StatPacketArray.Packets.Num(); PacketIndex++ )
	{
		const FStatPacket& StatPacket = *StatPacketArray.Packets[PacketIndex];
		FName ThreadFName = StatsThreadStats.Threads.FindChecked( StatPacket.ThreadId );
		const uint32 NewThreadID = MetaData->ThreadIDtoStatID.FindChecked( StatPacket.ThreadId );

		// @TODO yrx 2014-04-29 Only game or render thread is supported at this moment.
		if( StatPacket.ThreadType != EThreadType::Game && StatPacket.ThreadType != EThreadType::Renderer )
		{
			continue;
		}

		// Workaround for issue with rendering thread names.
		if( StatPacket.ThreadType == EThreadType::Renderer )
		{
			ThreadFName = NAME_RenderThread;
		}
		else if( StatPacket.ThreadType == EThreadType::Game )
		{
			GameThreadFName = ThreadFName;
		}

		FProfilerStackNode* ThreadNode = ThreadNodes.FindRef( ThreadFName );
		if( !ThreadNode )
		{
			FString ThreadIdName = FStatsUtils::BuildUniqueThreadName( StatPacket.ThreadId );
			FStatMessage ThreadMessage( ThreadFName, EStatDataType::ST_int64, STAT_GROUP_TO_FStatGroup( STATGROUP_Threads )::GetGroupName(), STAT_GROUP_TO_FStatGroup( STATGROUP_Threads )::GetGroupCategory(), *ThreadIdName, true, true );
			//FStatMessage ThreadMessage( ThreadFName, EStatDataType::ST_int64, nullptr, nullptr, TEXT( "" ), true, true );
			ThreadMessage.NameAndInfo.SetFlag( EStatMetaFlags::IsPackedCCAndDuration, true );
			ThreadMessage.Clear();

			// Add a thread sample.
			const uint32 ThreadRootSampleIndex = DataProvider->AddHierarchicalSample
			(
				NewThreadID,
				MetaData->GetStatByID( NewThreadID ).OwningGroup().ID(),
				NewThreadID,
				-1.0f,
				-1.0f,
				1,
				FrameRootSampleIndex
			);

			ThreadNode = ThreadNodes.Add( ThreadFName, new FProfilerStackNode( nullptr, ThreadMessage, ThreadRootSampleIndex, FrameIndex ) );
		}


		TArray<const FStatMessage*> StartStack;
		TArray<FProfilerStackNode*> Stack;
		Stack.Add( ThreadNode );
		FProfilerStackNode* Current = Stack.Last();

		const FStatMessagesArray& Data = StatPacket.StatMessages;
		for( int32 Index = 0; Index < Data.Num(); Index++ )
		{
			const FStatMessage& Item = Data[Index];

			const EStatOperation::Type Op = Item.NameAndInfo.GetField<EStatOperation>();
			const FName LongName = Item.NameAndInfo.GetRawName();
			const FName ShortName = Item.NameAndInfo.GetShortName();

			const FName RenderingThreadTickCommandName = TEXT("RenderingThreadTickCommand");

			// Workaround for render thread hierarchy. EStatOperation::AdvanceFrameEventRenderThread is called within the scope.
			if( ShortName == RenderingThreadTickCommandName )
			{
				continue;
			}

			if( Op == EStatOperation::CycleScopeStart || Op == EStatOperation::CycleScopeEnd || Op == EStatOperation::AdvanceFrameEventRenderThread )
			{
				//check( Item.NameAndInfo.GetFlag( EStatMetaFlags::IsCycle ) );
				if( Op == EStatOperation::CycleScopeStart )
				{
					FProfilerStackNode* ChildNode = new FProfilerStackNode( Current, Item, -1, FrameIndex );
					Current->Children.Add( ChildNode );

					// Add a child sample.
					const uint32 SampleIndex = DataProvider->AddHierarchicalSample
					(
						NewThreadID,
						MetaData->GetStatByFName( ShortName ).OwningGroup().ID(), // GroupID
						MetaData->GetStatByFName( ShortName ).ID(), // StatID
						MetaData->ConvertCyclesToMS( ChildNode->CyclesStart ), // StartMS 
						MetaData->ConvertCyclesToMS( 0 ), // DurationMS
						1,
						Current->SampleIndex
					);
					ChildNode->SampleIndex = SampleIndex;

					Stack.Add( ChildNode );
					StartStack.Add( &Item );
					Current = ChildNode;
				}
				// Workaround for render thread hierarchy. EStatOperation::AdvanceFrameEventRenderThread is called within the scope.
				if( Op == EStatOperation::AdvanceFrameEventRenderThread )
				{
					int k=0;k++;
				}
				if( Op == EStatOperation::CycleScopeEnd )
				{
					const FStatMessage ScopeStart = *StartStack.Pop();
					const FStatMessage ScopeEnd = Item;
					const int64 Delta = int32( uint32( ScopeEnd.GetValue_int64() ) - uint32( ScopeStart.GetValue_int64() ) );
					Current->CyclesEnd = Current->CyclesStart + Delta;

					Current->CycleCounterStartTimeMS = MetaData->ConvertCyclesToMS( Current->CyclesStart );
					Current->CycleCounterEndTimeMS = MetaData->ConvertCyclesToMS( Current->CyclesEnd );

					if( Current->CycleCounterStartTimeMS > Current->CycleCounterEndTimeMS )
					{
						int k=0;k++;
					}

					check( Current->CycleCounterEndTimeMS >= Current->CycleCounterStartTimeMS );

					FProfilerStackNode* ChildNode = Current;

					// Update the child sample's DurationMS.
					MutableCollection[ChildNode->SampleIndex].SetDurationMS( MetaData->ConvertCyclesToMS( Delta ) );

					verify( Current == Stack.Pop() );
					Current = Stack.Last();				
				}
			}
		}
	}

	// Calculate thread times.
	for( auto It = ThreadNodes.CreateIterator(); It; ++It )
	{
		FProfilerStackNode& ThreadNode = *It.Value();
		const int32 ChildrenNum = ThreadNode.Children.Num();
		if( ChildrenNum > 0 )
		{
			const int32 LastChildIndex = ThreadNode.Children.Num() - 1;
			ThreadNode.CyclesStart = ThreadNode.Children[0]->CyclesStart;
			ThreadNode.CyclesEnd = ThreadNode.Children[LastChildIndex]->CyclesEnd;
			ThreadNode.CycleCounterStartTimeMS = MetaData->ConvertCyclesToMS( ThreadNode.CyclesStart );
			ThreadNode.CycleCounterEndTimeMS = MetaData->ConvertCyclesToMS( ThreadNode.CyclesEnd );

			FProfilerSample& ProfilerSample = MutableCollection[ThreadNode.SampleIndex];
			ProfilerSample.SetStartAndEndMS( MetaData->ConvertCyclesToMS( ThreadNode.CyclesStart ), MetaData->ConvertCyclesToMS( ThreadNode.CyclesEnd ) );
		}
	}

	// Get the game thread time.
	check( GameThreadFName != NAME_None );
	const FProfilerStackNode& GameThreadNode = *ThreadNodes.FindChecked( GameThreadFName );
	const double GameThreadStartMS = MetaData->ConvertCyclesToMS( GameThreadNode.CyclesStart );
	const double GameThreadEndMS = MetaData->ConvertCyclesToMS( GameThreadNode.CyclesEnd );
	MutableCollection[FrameRootSampleIndex].SetStartAndEndMS( GameThreadStartMS, GameThreadEndMS );
	
	// Advance frame
	const uint32 LastFrameIndex = DataProvider->GetNumFrames();
	DataProvider->AdvanceFrame( GameThreadEndMS - GameThreadStartMS );
 
	// Update aggregated stats
	UpdateAggregatedStats( LastFrameIndex );
 
	// Update aggregated events.
	UpdateAggregatedEventGraphData( LastFrameIndex );

	// RootNode is the same as the game thread node.
	out_ProfilerFrame.Root->CycleCounterStartTimeMS = GameThreadStartMS;
	out_ProfilerFrame.Root->CycleCounterEndTimeMS = GameThreadEndMS;

	for( auto It = ThreadNodes.CreateIterator(); It; ++It )
	{
		out_ProfilerFrame.AddChild( It.Value() );
	}

	out_ProfilerFrame.SortChildren();
}