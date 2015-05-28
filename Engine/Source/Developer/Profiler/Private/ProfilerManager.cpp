// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ProfilerPrivatePCH.h"
#include "RenderingThread.h"

#define LOCTEXT_NAMESPACE "FProfilerCommands"

/*-----------------------------------------------------------------------------
	Globals and statics
-----------------------------------------------------------------------------*/
DEFINE_LOG_CATEGORY(Profiler);
FProfilerSettings FProfilerSettings::Defaults( true );

/*-----------------------------------------------------------------------------
	Stat declarations.
-----------------------------------------------------------------------------*/



DEFINE_STAT(STAT_DG_OnPaint);
DEFINE_STAT(STAT_PM_HandleProfilerData);
DEFINE_STAT(STAT_PM_Tick);
DEFINE_STAT(STAT_PM_MemoryUsage);

/*-----------------------------------------------------------------------------
	Profiler manager implementation
-----------------------------------------------------------------------------*/

TSharedPtr<FProfilerManager> FProfilerManager::Instance = nullptr;
FThreadSafeCounter FProfilerManager::ProcessingLock;

struct FEventGraphSampleLess
{
	FORCEINLINE bool operator()(const FEventGraphSample& A, const FEventGraphSample& B) const 
	{ 
		return A._InclusiveTimeMS < B._InclusiveTimeMS; 
	}
};

FProfilerManager::FProfilerManager( const ISessionManagerRef& InSessionManager )
	: SessionManager( InSessionManager )
	, CommandList( new FUICommandList() )
	, ProfilerActionManager( this )
	, Settings()

	, ProfilerType( EProfilerSessionTypes::InvalidOrMax )
	, bLivePreview( false )
	, bHasCaptureFileFullyProcessed( false )
{
	FEventGraphSample::InitializePropertyManagement();

#if	0
	// Performance tests.
	static FTotalTimeAndCount CurrentNative(0.0f, 0);
	static FTotalTimeAndCount CurrentPointer(0.0f, 0);
	static FTotalTimeAndCount CurrentShared(0.0f, 0);

	for( int32 Lx = 0; Lx < 16; ++Lx )
	{
		FRandomStream RandomStream( 666 );
		TArray<FEventGraphSample> EventsNative;
		TArray<FEventGraphSample*> EventsPointer;
		TArray<FEventGraphSamplePtr> EventsShared;

		const int32 NumEvents = 16384;
		for( int32 Nx = 0; Nx < NumEvents; ++Nx )
		{
			const double Rnd = RandomStream.FRandRange( 0.0f, 16384.0f );
			const FString EventName = TTypeToString<double>::ToString( Rnd );
			FEventGraphSample NativeEvent( *EventName );
			NativeEvent._InclusiveTimeMS = Rnd;

			FEventGraphSamplePtr SharedEvent = FEventGraphSample::CreateNamedEvent( *EventName );
			SharedEvent->_InclusiveTimeMS = Rnd;

			EventsNative.Add(NativeEvent);
			EventsPointer.Add(SharedEvent.Get());
			EventsShared.Add(SharedEvent);
		}

		{
			FProfilerScopedLogTime ScopedLog( TEXT("1.NativeSorting"), &CurrentNative );
			EventsNative.Sort( FEventGraphSampleLess() );
		}

		{
			FProfilerScopedLogTime ScopedLog( TEXT("2PointerSorting"), &CurrentPointer );
			EventsPointer.Sort( FEventGraphSampleLess() );
		}

		{
			FProfilerScopedLogTime ScopedLog( TEXT("3.SharedSorting"), &CurrentShared );
			FEventArraySorter::Sort( EventsShared, FEventGraphSample::GetEventPropertyByIndex(EEventPropertyIndex::InclusiveTimeMS).Name, EEventCompareOps::Less );
		}
	}
#endif // 0
}

void FProfilerManager::PostConstructor()
{
	// Register tick functions.
	OnTick = FTickerDelegate::CreateSP( this, &FProfilerManager::Tick );
	OnTickHandle = FTicker::GetCoreTicker().AddTicker( OnTick );

	// Create profiler client.
	ProfilerClient = FModuleManager::GetModuleChecked<IProfilerClientModule>("ProfilerClient").CreateProfilerClient();
	
	// Register profiler client delegates.
	ProfilerClient->OnProfilerData().AddSP(this, &FProfilerManager::ProfilerClient_OnProfilerData);
	ProfilerClient->OnProfilerClientConnected().AddSP(this, &FProfilerManager::ProfilerClient_OnClientConnected);
	ProfilerClient->OnProfilerClientDisconnected().AddSP(this, &FProfilerManager::ProfilerClient_OnClientDisconnected);
	
	ProfilerClient->OnLoadStarted().AddSP(this, &FProfilerManager::ProfilerClient_OnLoadStarted);
	ProfilerClient->OnLoadedMetaData().AddSP(this, &FProfilerManager::ProfilerClient_OnLoadedMetaData);
	ProfilerClient->OnLoadCompleted().AddSP(this, &FProfilerManager::ProfilerClient_OnLoadCompleted);

	ProfilerClient->OnMetaDataUpdated().AddSP(this, &FProfilerManager::ProfilerClient_OnMetaDataUpdated);

	ProfilerClient->OnProfilerFileTransfer().AddSP(this, &FProfilerManager::ProfilerClient_OnProfilerFileTransfer);

	SessionManager->OnCanSelectSession().AddSP( this, &FProfilerManager::SessionManager_OnCanSelectSession );
	SessionManager->OnSelectedSessionChanged().AddSP( this, &FProfilerManager::SessionManager_OnSelectedSessionChanged );
	SessionManager->OnInstanceSelectionChanged().AddSP( this, &FProfilerManager::SessionManager_OnInstanceSelectionChanged );

	SetDataPreview( false );
	SetDataCapture( false );

	FProfilerCommands::Register();
	BindCommands();
}

void FProfilerManager::BindCommands()
{
	ProfilerActionManager.Map_ProfilerManager_Load();
	ProfilerActionManager.Map_ToggleDataPreview_Global();
	ProfilerActionManager.Map_ProfilerManager_ToggleLivePreview_Global();
	ProfilerActionManager.Map_ToggleDataCapture_Global();
	ProfilerActionManager.Map_OpenSettings_Global();
}

FProfilerManager::~FProfilerManager()
{
	FProfilerCommands::Unregister();

	// Unregister tick function.
	FTicker::GetCoreTicker().RemoveTicker( OnTickHandle );

	// Remove ourselves from the profiler client.
	if( ProfilerClient.IsValid() )
	{
		ProfilerClient->Unsubscribe();

		ProfilerClient->OnProfilerData().RemoveAll(this);
		ProfilerClient->OnProfilerClientConnected().RemoveAll(this);
		ProfilerClient->OnProfilerClientDisconnected().RemoveAll(this);
		ProfilerClient->OnMetaDataUpdated().RemoveAll(this);
		ProfilerClient->OnLoadedMetaData().RemoveAll(this);
		ProfilerClient->OnLoadCompleted().RemoveAll(this);
		ProfilerClient->OnLoadStarted().RemoveAll(this);
		ProfilerClient->OnProfilerFileTransfer().RemoveAll(this);
	}

	// Remove ourselves from the session manager.
	if( SessionManager.IsValid() )
	{
		SessionManager->OnCanSelectSession().RemoveAll(this);
		SessionManager->OnSelectedSessionChanged().RemoveAll(this);
		SessionManager->OnInstanceSelectionChanged().RemoveAll(this);

		// clear the selected session
		ISessionInfoPtr Ptr;
		Ptr.Reset();
		SessionManager->SelectSession(Ptr);
	}

	ClearStatsAndInstances();
}


void FProfilerManager::LoadProfilerCapture( const FString& ProfilerCaptureFilepath, const bool bAdd /*= false*/ )
{
	// deselect the active session
	if (ActiveSession.IsValid())
	{
		SessionManager->SelectSession(NULL);
	}

	if( bAdd == false )
	{
		ClearStatsAndInstances();
	}

	FProfilerSessionRef ProfilerSession = MakeShareable( new FProfilerSession( ProfilerCaptureFilepath ) );
	auto Var = ProfilerSession->AsShared();
	const FGuid ProfilerInstanceID = ProfilerSession->GetInstanceID();

	ProfilerSession->
		SetOnCaptureFileProcessed( FProfilerSession::FCaptureFileProcessedDelegate::CreateSP( this, &FProfilerManager::ProfilerSession_OnCaptureFileProcessed ) )
		.SetOnAddThreadTime( FProfilerSession::FAddThreadTimeDelegate::CreateSP( this, &FProfilerManager::ProfilerSession_OnAddThreadTime ) );

	ProfilerSessionInstances.Add( ProfilerInstanceID, ProfilerSession );
	{
		PROFILER_SCOPE_LOG_TIME( TEXT( "ProfilerClient->LoadCapture" ), nullptr );
		ProfilerClient->LoadCapture( ProfilerCaptureFilepath, ProfilerInstanceID );
	}

	SessionInstancesUpdatedEvent.Broadcast();
	ProfilerType = EProfilerSessionTypes::StatsFile;
	
	GetProfilerWindow()->ManageEventGraphTab( ProfilerInstanceID, true, ProfilerSession->GetName() );
	SetViewMode( EProfilerViewMode::LineIndexBased );
}

void FProfilerManager::LoadRawStatsFile( const FString& RawStatsFileFileath )
{
	if( ActiveSession.IsValid() )
	{
		SessionManager->SelectSession( NULL );
	}
	ClearStatsAndInstances();

	TSharedRef<FRawProfilerSession> ProfilerSession = MakeShareable( new FRawProfilerSession( RawStatsFileFileath ) );
	const FGuid ProfilerInstanceID = ProfilerSession->GetInstanceID();
	ProfilerSessionInstances.Add( ProfilerInstanceID, ProfilerSession );

	ProfilerSession->
		//SetOnCaptureFileProcessed( FProfilerSession::FCaptureFileProcessedDelegate::CreateSP( this, /&FProfilerManager::ProfilerSession_OnCaptureFileProcessed ) )
		SetOnAddThreadTime( FProfilerSession::FAddThreadTimeDelegate::CreateSP( this, &FProfilerManager::ProfilerSession_OnAddThreadTime ) );

	ProfilerSessionInstances.Add( ProfilerInstanceID, ProfilerSession );

	ProfilerSession->PrepareLoading();
	RequestFilterAndPresetsUpdateEvent.Broadcast();
	//ProfilerSession_OnCaptureFileProcessed( ProfilerInstanceID );
	bHasCaptureFileFullyProcessed = true;
	//TrackDefaultStats();

	SessionInstancesUpdatedEvent.Broadcast();
	ProfilerType = EProfilerSessionTypes::StatsFileRaw;
	
	GetProfilerWindow()->ManageEventGraphTab( ProfilerInstanceID, true, ProfilerSession->GetName() );
	SetViewMode( EProfilerViewMode::ThreadViewTimeBased );

	GetProfilerWindow()->GraphPanel->ThreadView->AttachProfilerStream( ProfilerSession->GetStream() );
}


bool FProfilerManager::Tick( float DeltaTime )
{
	SCOPE_CYCLE_COUNTER(STAT_PM_Tick);

#if	0
	static int32 NumIdleTicks = 0;
	NumIdleTicks++;
	if( NumIdleTicks > 60 )
	{
		LoadRawStatsFile( TEXT( "U:/P4EPIC2/UE4/QAGame/Saved/Profiling/UnrealStats/RenderTestMap-Windows-03.19-13.49.04/RenderTestMap-Windows-19-13.52.19.ue4statsraw" ) );
		return false;
	}
#endif // 0

	return true;
}

void FProfilerManager::ProfilerSession_OnCaptureFileProcessed( const FGuid ProfilerInstanceID )
{
	const FProfilerSessionRef* ProfilerSession = FindSessionInstance( ProfilerInstanceID );
	if( ProfilerSession && ProfilerWindow.IsValid())
	{
		RequestFilterAndPresetsUpdateEvent.Broadcast();

		GetProfilerWindow()->UpdateEventGraph( ProfilerInstanceID, (*ProfilerSession)->GetEventGraphDataAverage(), (*ProfilerSession)->GetEventGraphDataMaximum(), true );
		bHasCaptureFileFullyProcessed = true;
	}
}

void FProfilerManager::SetDataCapture( const bool bRequestedDataCaptureState )
{
	ProfilerClient->SetCaptureState( bRequestedDataCaptureState );
	for( auto It = GetProfilerInstancesIterator(); It; ++It )
	{
		FProfilerSessionRef ProfilerSession = It.Value();
		ProfilerSession->bDataCapturing = bRequestedDataCaptureState;
	}
}

/*-----------------------------------------------------------------------------
	ProfilerClient
-----------------------------------------------------------------------------*/

void FProfilerManager::ProfilerClient_OnProfilerData( const FGuid& InstanceID, const FProfilerDataFrame& Content, const float DataLoadingProgress )
{
	SCOPE_CYCLE_COUNTER(STAT_PM_HandleProfilerData);

	const FProfilerSessionRef* ProfilerSession = FindSessionInstance( InstanceID );
	if( ProfilerSession && ProfilerWindow.IsValid())
	{
		(*ProfilerSession)->UpdateProfilerData( Content );
		// Game thread should always be enabled.
		TrackDefaultStats();

		// Update the notification that a file is being loaded.
		GetProfilerWindow()->ManageLoadingProgressNotificationState( (*ProfilerSession)->GetName(), EProfilerNotificationTypes::LoadingOfflineCapture, ELoadingProgressStates::InProgress, DataLoadingProgress );
	}
}

void FProfilerManager::ProfilerClient_OnMetaDataUpdated( const FGuid& InstanceID )
{
	const FProfilerSessionRef* ProfilerSession = FindSessionInstance( InstanceID );
	if( ProfilerSession && ProfilerWindow.IsValid())
	{
		(*ProfilerSession)->UpdateMetadata( ProfilerClient->GetStatMetaData( InstanceID ) );

		if( (*ProfilerSession)->GetSessionType() == EProfilerSessionTypes::Live )
		{
			RequestFilterAndPresetsUpdateEvent.Broadcast();
		}
	}
}

void FProfilerManager::ProfilerClient_OnLoadedMetaData( const FGuid& InstanceID )
{
	//TrackDefaultStats();
}

void FProfilerManager::ProfilerClient_OnLoadStarted( const FGuid& InstanceID )
{
	const FProfilerSessionRef* ProfilerSession = FindSessionInstance( InstanceID );
	if( ProfilerSession && GetProfilerWindow().IsValid())
	{
		const FString Description = (*ProfilerSession)->GetName();

		// Display the notification that a file is being loaded.
		GetProfilerWindow()->ManageLoadingProgressNotificationState( (*ProfilerSession)->GetName(), EProfilerNotificationTypes::LoadingOfflineCapture, ELoadingProgressStates::Started, 0.0f );
	}
}

void FProfilerManager::ProfilerClient_OnLoadCompleted( const FGuid& InstanceID )
{
	// Inform that the file has been loaded and we can hide the notification.
	const FProfilerSessionRef* ProfilerSession = FindSessionInstance( InstanceID );
	if( ProfilerSession && GetProfilerWindow().IsValid())
	{
		(*ProfilerSession)->LoadComplete();

		// Update the notification that a file has been loaded, to be precise it should be loaded on the next tick...
		GetProfilerWindow()->ManageLoadingProgressNotificationState( (*ProfilerSession)->GetName(), EProfilerNotificationTypes::LoadingOfflineCapture, ELoadingProgressStates::Loaded, 1.0f );
	}
}


void FProfilerManager::ProfilerClient_OnProfilerFileTransfer( const FString& Filename, int64 FileProgress, int64 FileSize )
{
	// Display and update the notification a file that is being sent.

	const float Progress = (double)FileProgress/(double)FileSize;

	ELoadingProgressStates::Type ProgressState = ELoadingProgressStates::InvalidOrMax;
	if( FileProgress == 0 )
	{
		ProgressState = ELoadingProgressStates::Started;
	}
	else if( FileProgress == -1 && FileSize == -1 )
	{
		ProgressState = ELoadingProgressStates::Failed;
	}
	else if( FileProgress > 0 && FileProgress < FileSize )
	{
		ProgressState = ELoadingProgressStates::InProgress;
	}
	else if( FileProgress == FileSize )
	{
		ProgressState = ELoadingProgressStates::Loaded;
	}
	
	if( ProfilerWindow.IsValid() )
	{
		ProfilerWindow.Pin()->ManageLoadingProgressNotificationState( Filename, EProfilerNotificationTypes::SendingServiceSideCapture, ProgressState, Progress );
	}
}

void FProfilerManager::ProfilerClient_OnClientConnected( const FGuid& SessionID, const FGuid& InstanceID )
{
}

void FProfilerManager::ProfilerClient_OnClientDisconnected( const FGuid& SessionID, const FGuid& InstanceID )
{
}

/*-----------------------------------------------------------------------------
	SessionManager
-----------------------------------------------------------------------------*/

void FProfilerManager::SessionManager_OnCanSelectSession( const ISessionInfoPtr& SelectedSession, bool& CanSelect )
{
}

void FProfilerManager::SessionManager_OnSelectedSessionChanged( const ISessionInfoPtr& InActiveSession )
{
	SessionManager_OnInstanceSelectionChanged();
}

void FProfilerManager::SessionManager_OnInstanceSelectionChanged()
{
	const ISessionInfoPtr& SelectedSession = SessionManager->GetSelectedSession();
	const bool SessionIsValid = SelectedSession.IsValid() && (SelectedSession->GetSessionOwner() == FPlatformProcess::UserName(true));

	if( ActiveSession != SelectedSession || FProfilerManager::GetSettings().bSingleInstanceMode )
	{
		ClearStatsAndInstances();

		if (SessionIsValid)
		{
			ActiveSession = SelectedSession;
			ProfilerClient->Subscribe( ActiveSession->GetSessionId() );
			ProfilerType = EProfilerSessionTypes::Live;
			SetViewMode( EProfilerViewMode::LineIndexBased );
		}
		else
		{
			ActiveSession = nullptr;
			ProfilerType = EProfilerSessionTypes::InvalidOrMax;
		}
	}

	if( ActiveSession.IsValid() )
	{
		// Track all selected session instances.
		SessionManager->GetSelectedInstances( SelectedSessionInstances );
		const int32 NumSelectedInstances = SelectedSessionInstances.Num();
		const int32 NumInstances = FMath::Min( NumSelectedInstances, FProfilerManager::GetSettings().bSingleInstanceMode ? 1 : NumSelectedInstances );

		for( int32 Index = 0; Index < NumInstances; ++Index )
		{
			const ISessionInstanceInfoPtr SessionInstanceInfo = SelectedSessionInstances[Index];
			const FGuid ProfilerInstanceID = SessionInstanceInfo->GetInstanceId();
			const bool bAlreadyAdded = ProfilerSessionInstances.Contains( ProfilerInstanceID );

			if( !bAlreadyAdded )
			{
				FProfilerSessionRef ProfilerSession = MakeShareable( new FProfilerSession( SessionInstanceInfo ) );
				ProfilerSession->SetOnAddThreadTime( FProfilerSession::FAddThreadTimeDelegate::CreateSP( this, &FProfilerManager::ProfilerSession_OnAddThreadTime ) );

				ProfilerSessionInstances.Add( ProfilerSession->GetInstanceID(), ProfilerSession );
				ProfilerClient->Track( ProfilerInstanceID );
				TSharedPtr<SProfilerWindow> ProfilerWindowPtr = GetProfilerWindow();
				if (ProfilerWindowPtr.IsValid())
				{
					ProfilerWindowPtr->ManageEventGraphTab(ProfilerInstanceID, true, ProfilerSession->GetName());
				}
			}
		}
	}

	SessionInstancesUpdatedEvent.Broadcast();
}

/*-----------------------------------------------------------------------------
	Misc
-----------------------------------------------------------------------------*/

FCombinedGraphDataSourceRef FProfilerManager::CreateCombinedGraphDataSource( const uint32 StatID )
{
	FCombinedGraphDataSource* CombinedGraphDataSource = new FCombinedGraphDataSource( StatID, FTimeAccuracy::FPS060 );
	return MakeShareable( CombinedGraphDataSource );
}

/*-----------------------------------------------------------------------------
	Stat tracking
-----------------------------------------------------------------------------*/

bool FProfilerManager::TrackStat( const uint32 StatID )
{
	bool bAdded = false;

	// Check if all profiler instances have this stat ready.
	int32 NumReadyStats = 0;
	for( auto It = GetProfilerInstancesIterator(); It; ++It )
	{
		const FProfilerSessionRef ProfilerSession = It.Value();
		NumReadyStats += ProfilerSession->GetAggregatedStat(StatID) != nullptr ? 1 : 0;
	}
	const bool bStatIsReady = NumReadyStats == GetProfilerInstancesNum();

	if( StatID != 0 && bStatIsReady )
	{
		FTrackedStat* TrackedStat = TrackedStats.Find( StatID );

		if( TrackedStat == nullptr )
		{
			// R = H, G = S, B = V
			const FLinearColor& ColorAverage = GetColorForStatID( StatID );
			const FLinearColor ColorAverageHSV = ColorAverage.LinearRGBToHSV();

			FLinearColor ColorBackgroundHSV = ColorAverageHSV;
			ColorBackgroundHSV.G = FMath::Max( 0.0f, ColorBackgroundHSV.G-0.25f );

			FLinearColor ColorExtremesHSV = ColorAverageHSV;
			ColorExtremesHSV.G = FMath::Min( 1.0f, ColorExtremesHSV.G+0.25f );
			ColorExtremesHSV.B = FMath::Min( 1.0f, ColorExtremesHSV.B+0.25f );

			const FLinearColor ColorBackground = ColorBackgroundHSV.HSVToLinearRGB();
			const FLinearColor ColorExtremes = ColorExtremesHSV.HSVToLinearRGB();

			TrackedStat = &TrackedStats.Add( StatID, FTrackedStat(CreateCombinedGraphDataSource( StatID ),ColorAverage,ColorExtremes,ColorBackground,StatID) );
			bAdded = true;

			// @TODO: Convert a reference parameter to copy parameter/sharedptr/ref/weak, to avoid problems when a reference is no longer valid.
			TrackedStatChangedEvent.Broadcast( *TrackedStat, true );
		}

		if( TrackedStat != nullptr )
		{
			uint32 NumAddedInstances = 0;
			bool bMetadataInitialized = false;

			for( auto It = GetProfilerInstancesIterator(); It; ++It )
			{
				const FGuid& SessionInstanceID = It.Key();// ProfilerSessionInstanceID, ProfilerInstanceID, InstanceID
				const FProfilerSessionRef ProfilerSession = It.Value();
				const bool bInstanceAdded = TrackStatForSessionInstance( StatID, SessionInstanceID );
				NumAddedInstances += bInstanceAdded ? 1 : 0;

				// Initialize metadata for combine graph data source.
				// TODO: This should be checked against the remaining elements to detect inconsistent data.
				// The first instance should be the main.
				if( !bMetadataInitialized )
				{
					const bool bIsStatReady = ProfilerSession->GetMetaData()->IsStatInitialized( StatID );
					if( bIsStatReady )
					{
						const FProfilerStatMetaDataRef MetaData = ProfilerSession->GetMetaData();
						const FProfilerStat& Stat = MetaData->GetStatByID( StatID );
						const FProfilerGroup& Group = Stat.OwningGroup();

						TrackedStat->CombinedGraphDataSource->Initialize( Stat.Name().GetPlainNameString(), Group.ID(), Group.Name().GetPlainNameString(), Stat.Type(), ProfilerSession->GetCreationTime() );
						bMetadataInitialized = true;
					}
				}
			}
		}
	}

	return bAdded;
}

bool FProfilerManager::UntrackStat( const uint32 StatID )
{
	bool bRemoved = false;

	const FTrackedStat* TrackedStat = TrackedStats.Find( StatID );
	if( TrackedStat )
	{
		TrackedStatChangedEvent.Broadcast( *TrackedStat, false );
		TrackedStats.Remove( StatID );
		bRemoved = true;
	}

	return bRemoved;
}

void FProfilerManager::ClearStatsAndInstances()
{
	CloseAllEventGraphTabs();

	ProfilerType = EProfilerSessionTypes::InvalidOrMax;
	ViewMode = EProfilerViewMode::InvalidOrMax;
	SetDataPreview( false );
	bLivePreview = false;
	SetDataCapture( false );

	bHasCaptureFileFullyProcessed = false;

	for( auto It = TrackedStats.CreateConstIterator(); It; ++It )
	{
		const FTrackedStat& TrackedStat = It.Value();
		UntrackStat( TrackedStat.StatID );
	}
	for( auto It = GetProfilerInstancesIterator(); It; ++It )
	{
		const FGuid ProfilerInstanceID = It.Key();
		ProfilerClient->Untrack( ProfilerInstanceID );
	}
	ProfilerSessionInstances.Empty();
}

const bool FProfilerManager::IsStatTracked( const uint32 StatID ) const
{
	return TrackedStats.Contains( StatID );
}

bool FProfilerManager::TrackStatForSessionInstance( const uint32 StatID, const FGuid& SessionInstanceID )
{
	bool bAdded = false;

	const FProfilerSessionRef* ProfilerSessionPtr = FindSessionInstance( SessionInstanceID );
	const FTrackedStat* TrackedStat = TrackedStats.Find( StatID );

	if( ProfilerSessionPtr && TrackedStat )
	{
		const bool bIsStatReady = (*ProfilerSessionPtr)->GetMetaData()->IsStatInitialized( StatID );
		const bool bIsExist = TrackedStat->CombinedGraphDataSource->IsProfilerSessionRegistered( SessionInstanceID );

		if( bIsStatReady && !bIsExist )
		{
			// Create graph data provider for tracked stat.
			FGraphDataSourceRefConst	GraphDataSource = (*ProfilerSessionPtr)->CreateGraphDataSource( StatID );
			TrackedStat->CombinedGraphDataSource->RegisterWithProfilerSession( SessionInstanceID, GraphDataSource );
			bAdded = true;
		}
	}

	return bAdded;
}

bool FProfilerManager::UntrackStatForSessionInstance( const uint32 StatID, const FGuid& SessionInstanceID )
{
	bool bRemoved = false;

	//FProfilerSessionRef* ProfilerSessionPtr = FindSessionInstance( SessionInstanceID );
	const FTrackedStat* TrackedStat = TrackedStats.Find( StatID );

	if( TrackedStat )
	{
		const bool bIsExist = TrackedStat->CombinedGraphDataSource->IsProfilerSessionRegistered( SessionInstanceID );
		if( bIsExist )
		{
			TrackedStat->CombinedGraphDataSource->UnregisterWithProfilerSession( SessionInstanceID );
			bRemoved = true;
		}
	}

	return bRemoved;
}

const bool FProfilerManager::IsStatTrackedForSessionInstance( const uint32 StatID, const FGuid& SessionInstanceID ) const
{
	bool bResult = false;
	const FTrackedStat* TrackedStat = TrackedStats.Find( StatID );
	if( TrackedStat )
	{
		bResult = (*TrackedStat).CombinedGraphDataSource->IsProfilerSessionRegistered( SessionInstanceID );
	}

	return bResult;
}

bool FProfilerManager::TrackSessionInstance( const FGuid& SessionInstanceID )
{
	for( auto It = TrackedStats.CreateConstIterator(); It; ++It )
	{
		const FTrackedStat& TrackedStat = It.Value();
		TrackStatForSessionInstance( TrackedStat.StatID, SessionInstanceID );
	}
	return true;
}

bool FProfilerManager::UntrackSessionInstance( const FGuid& SessionInstanceID )
{
	for( auto It = TrackedStats.CreateConstIterator(); It; ++It )
	{
		const FTrackedStat& TrackedStat = It.Value();
		UntrackStatForSessionInstance( TrackedStat.StatID, SessionInstanceID );
	}
	return true;
}

const bool FProfilerManager::IsSessionInstanceTracked( const FGuid& SessionInstanceID ) const
{
	const FProfilerSessionRef* ProfilerSessionPtr = FindSessionInstance( SessionInstanceID );
	if( ProfilerSessionPtr )
	{
		for( auto It = TrackedStats.CreateConstIterator(); It; ++It )
		{
			const FTrackedStat& TrackedStat = It.Value();
			const bool bIsSessionInstanceTracked = TrackedStat.CombinedGraphDataSource->IsProfilerSessionRegistered( SessionInstanceID );
			if( bIsSessionInstanceTracked )
			{
				return true;
			}
		}
	}

	return false;
}

//-----------------------------------------------------------------------------

// @TODO: Move to profiler settings
const FLinearColor& FProfilerManager::GetColorForStatID( const uint32 StatID ) const
{
	static TMap<uint32,FLinearColor> StatID2ColorMapping;

	FLinearColor* Color = StatID2ColorMapping.Find( StatID );
	if( !Color )
	{
		const FColor RandomColor = FColor::MakeRandomColor();
		Color = &StatID2ColorMapping.Add( StatID, FLinearColor(RandomColor) );
	}

	return *Color;
}

void FProfilerManager::TrackDefaultStats()
{
	// Find StatId for the game thread.
	for( auto It = GetProfilerInstancesIterator(); It; ++It )
	{
		FProfilerSessionRef ProfilerSession = It.Value();
		if( ProfilerSession->GetMetaData()->IsReady() )
		{
			TrackStat( ProfilerSession->GetMetaData()->GetGameThreadStatID() );
		}
		break;
	}
}

/*-----------------------------------------------------------------------------
		Event graphs management
-----------------------------------------------------------------------------*/

void FProfilerManager::CloseAllEventGraphTabs()
{
	TSharedPtr<SProfilerWindow> ProfilerWindowPtr = GetProfilerWindow();
	if( ProfilerWindowPtr.IsValid() )
	{
		// Iterate through all profiler sessions.
		for( auto It = GetProfilerInstancesIterator(); It; ++It )
		{
			FProfilerSessionRef ProfilerSession = It.Value();
			ProfilerWindowPtr->ManageEventGraphTab( ProfilerSession->GetInstanceID(), false, TEXT("") );
		}

		ProfilerWindowPtr->ProfilerMiniView->Reset();
	}
}

void FProfilerManager::DataGraph_OnSelectionChangedForIndex( uint32 FrameStartIndex, uint32 FrameEndIndex )
{
	PROFILER_SCOPE_LOG_TIME( TEXT( "FProfilerManager::DataGraph_OnSelectionChangedForIndex" ), nullptr );

	for( auto It = GetProfilerInstancesIterator(); It; ++It )
	{
		FProfilerSessionRef ProfilerSession = It.Value();
	
		FEventGraphDataRef EventGraphDataAverage = ProfilerSession->CreateEventGraphData( FrameStartIndex, FrameEndIndex, EEventGraphTypes::Average );
		FEventGraphDataRef EventGraphDataMaximum = ProfilerSession->CreateEventGraphData( FrameStartIndex, FrameEndIndex, EEventGraphTypes::Maximum );

		GetProfilerWindow()->UpdateEventGraph( ProfilerSession->GetInstanceID(), EventGraphDataAverage, EventGraphDataMaximum, false );
	}
}

void FProfilerManager::ProfilerSession_OnAddThreadTime( int32 FrameIndex, const TMap<uint32, float>& ThreadMS, const FProfilerStatMetaDataRef& StatMetaData )
{
	TSharedPtr<SProfilerWindow> ProfilerWindowPtr = GetProfilerWindow();
	if( ProfilerWindowPtr.IsValid() )
	{
		ProfilerWindowPtr->ProfilerMiniView->AddThreadTime( FrameIndex, ThreadMS, StatMetaData );
	}
}

void FProfilerManager::SetViewMode( EProfilerViewMode::Type NewViewMode )
{
	if( NewViewMode != ViewMode )
	{
		// Broadcast.
		OnViewModeChangedEvent.Broadcast( NewViewMode );
		ViewMode = NewViewMode;
	}
}

#undef LOCTEXT_NAMESPACE