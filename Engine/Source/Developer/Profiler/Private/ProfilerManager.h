// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
 
#pragma once

/*-----------------------------------------------------------------------------
	Basic structures
-----------------------------------------------------------------------------*/

/** Contains all settings for the profiler, accessible through the profiler manager. */
class FProfilerSettings
{
public:
	FProfilerSettings( bool bInIsDefault = false )
		: bSingleInstanceMode( true )
		, bShowCoalescedViewModesInEventGraph( true )
		, bIsEditing( false )
		, bIsDefault( bInIsDefault )
	{
		if( !bIsDefault )
		{
			LoadFromConfig();
		}
	}

	~FProfilerSettings()
	{
		if( !bIsDefault )
		{
			SaveToConfig();
		}
	}

	void LoadFromConfig()
	{
		FConfigCacheIni::LoadGlobalIniFile(ProfilerSettingsIni, TEXT("ProfilerSettings"));

		GConfig->GetBool( TEXT("Profiler.ProfilerOptions"), TEXT("bSingleInstanceMode"), bSingleInstanceMode, ProfilerSettingsIni );
		GConfig->GetBool( TEXT("Profiler.ProfilerOptions"), TEXT("bShowCoalescedViewModesInEventGraph"), bShowCoalescedViewModesInEventGraph, ProfilerSettingsIni );
	}

	void SaveToConfig()
	{
		GConfig->SetBool( TEXT("Profiler.ProfilerOptions"), TEXT("bSingleInstanceMode"), bSingleInstanceMode, ProfilerSettingsIni );
		GConfig->SetBool( TEXT("Profiler.ProfilerOptions"), TEXT("bShowCoalescedViewModesInEventGraph"), bShowCoalescedViewModesInEventGraph, ProfilerSettingsIni );
		GConfig->Flush( false, ProfilerSettingsIni );
	}

	void EnterEditMode()
	{
		bIsEditing = true;
	}

	void ExitEditMode()
	{
		bIsEditing = false;
	}

	const bool IsEditing() const
	{
		return bIsEditing;
	}

	const FProfilerSettings& GetDefaults() const
	{
		return Defaults;
	}

//protected:
	/** Contains default settings. */
	static FProfilerSettings Defaults;

	/** Profiler setting filename ini. */
	FString ProfilerSettingsIni;

	/** If True, the profiler will work in the single instance mode, all functionality related to multi instances will be disabled or removed from the UI. */
	bool bSingleInstanceMode;

	/** If True, coalesced view modes related functionality will be added to the event graph. */
	bool bShowCoalescedViewModesInEventGraph;

	/** Whether profiler settings is in edit mode. */
	bool bIsEditing;

	/** Whether this instance contains defaults. */
	bool bIsDefault;
};


/** Contains basic information about tracked stat. */
class FTrackedStat //: public FNoncopyable, public TSharedFromThis<FTrackedStat>
{
public:
	/**
	 * Initialization constructor.
	 *
	 * @param InColorAverage	- color used to draw the average value
	 * @param InColorExtremes	- color used to draw the extreme values
	 * @param InColorBackground - color used to draw the background
	 * @param InStatID			- the ID of the stat which will be tracked
	 *
	 */
	FTrackedStat
	(
		FCombinedGraphDataSourceRef InCombinedGraphDataSource,
		const FLinearColor InColorAverage,
		const FLinearColor InColorExtremes,
		const FLinearColor InColorBackground,
		const uint32 InStatID	
	)
		: CombinedGraphDataSource( InCombinedGraphDataSource )
		, ColorAverage( InColorAverage )
		, ColorExtremes( InColorExtremes )
		, ColorBackground( InColorBackground )
		, StatID( InStatID )
	{}

	/** Destructor. */
	~FTrackedStat()
	{}

//protected:
	/** A shared reference to the combined graph data source for all active profiler session instances for the specified stat ID. */
	FCombinedGraphDataSourceRef CombinedGraphDataSource;

	/** A color to visualize average value for the combined data graph. */
	const FLinearColor ColorAverage;

	/** A color to visualize extremes values for the combined data graph, min and max. */
	const FLinearColor ColorExtremes;

	/** A color to visualize background area for the combined data graph. */
	const FLinearColor ColorBackground;

	/** The ID of the stat. */
	const uint32 StatID;
};

/*-----------------------------------------------------------------------------
	FProfilerManager
-----------------------------------------------------------------------------*/

namespace EProfilerViewMode
{
	enum Type
	{
		/** Regular line graphs, for the regular stats file. */
		LineIndexBased,
		/** Thread view graph, for the raw stats file. */
		ThreadViewTimeBased,
		/** Invalid enum type, may be used as a number of enumerations. */
		InvalidOrMax,
	};
}

/** 
 ** This class manages following areas:
 **		Connecting/disconnecting to source or device through session manager
 **		Grabbing data from connected source or device through Profiler Transport Layer
 **		Creating a new async tasks for processing/preparing data for displaying/filtering etc
 **		Saving and loading profiler snapshots
 **		
 **		@TBD
 ** */
class FProfilerManager 
	: public TSharedFromThis<FProfilerManager>
	, public IProfilerManager
{
	friend class FProfilerActionManager;

	/**
	 *	Global processing lock, protects from destroying the profiler while task graph is still working on the stuff.
	 *	Assumes that there is only one instance of the profiler.
	 */
	static FThreadSafeCounter ProcessingLock;

public:
	/**
	 * Creates a profiler manager, only one distance can exists.
	 *
	 * @param InSessionManager	- the session manager to use
	 *
	 */
	FProfilerManager( const ISessionManagerRef& InSessionManager );

	/** Virtual destructor. */
	virtual ~FProfilerManager();

	/**
	 * Creates an instance of the profiler manager and initializes global instance with the previously created instance of the profiler manager.
	 *
	 * @param InSessionManager	- the session manager to use
	 *
	 */
	static TSharedPtr<FProfilerManager> Initialize( const ISessionManagerRef& SessionManager )
	{
		if ( FProfilerManager::Instance.IsValid() )
		{
			FProfilerManager::Instance.Reset();
		}
		FProfilerManager::Instance = MakeShareable( new FProfilerManager(SessionManager) );
		FProfilerManager::Instance->PostConstructor();
		return FProfilerManager::Instance;
	}

	void AssignProfilerWindow( const TSharedRef<class SProfilerWindow>& InProfilerWindow )
	{
		ProfilerWindow = InProfilerWindow;
	}

	/** Shutdowns the profiler manager. */
	void Shutdown()
	{
		FProfilerManager::Instance.Reset();
	}

	/** Increases processing lock count. */
	static void IncrementProcessingLock()
	{
		ProcessingLock.Increment();
	}

	/** Decreases processing lock count, once reaches zero the profiler can proceed. */
	static void DecrementProcessingLock()
	{
		ProcessingLock.Decrement();
	}

protected:
	/**
	 * Finishes initialization of the profiler manager
	 */
	void PostConstructor();

	/**	Binds our UI commands to delegates. */
	void BindCommands();

public:
	/**
	 * @return the global instance of the FProfilerManager. 
	 * This is an internal singleton and cannot be used outside ProfilerModule. 
	 * For external use: 
	 *		IProfilerModule& ProfilerModule = FModuleManager::Get().LoadModuleChecked<IProfilerModule>("Profiler");
	 *		ProfilerModule.GetProfilerManager();
	 */
	static TSharedPtr<FProfilerManager> Get()
	{
		return FProfilerManager::Instance;
	}

	/**
	 * @return an instance of the profiler action manager.
	 */
	static FProfilerActionManager& GetActionManager()
	{
		return FProfilerManager::Instance->ProfilerActionManager;
	}

	/**
	 * @return an instance of the profiler settings.
	 */
	static FProfilerSettings& GetSettings()
	{
		return FProfilerManager::Instance->Settings;
	}

	/** @returns UI command list for the profiler manager. */
	const TSharedRef< FUICommandList > GetCommandList() const
	{
		return CommandList;
	}

	/**
	 * @return an instance of the profiler commands.
	 */
	static const FProfilerCommands& GetCommands()
	{
		return FProfilerCommands::Get();
	}

	/**
	 * Creates a combined graph data source which will provide data for graph drawing.
	 *
	 * @param StatID - the ID of the stat that will used for generating the combined graph data source.
	 *
	 */
	FCombinedGraphDataSourceRef CreateCombinedGraphDataSource( const uint32 StatID );

	/*-----------------------------------------------------------------------------
		Stat tracking, Session instance management
	-----------------------------------------------------------------------------*/

	bool TrackStat( const uint32 StatID );
	bool UntrackStat( const uint32 StatID );
	void TrackDefaultStats();
	void ClearStatsAndInstances();

	/**
	 * @return true, if the specified stat is currently tracked by the profiler.
	 */
	const bool IsStatTracked( const uint32 StatID ) const;
	bool TrackStatForSessionInstance( const uint32 StatID, const FGuid& SessionInstanceID );
	bool UntrackStatForSessionInstance( const uint32 StatID, const FGuid& SessionInstanceID );
	const bool IsStatTrackedForSessionInstance( const uint32 StatID, const FGuid& SessionInstanceID ) const;
	bool TrackSessionInstance( const FGuid& SessionInstanceID );
	bool UntrackSessionInstance( const FGuid& SessionInstanceID );

	/**
	 * @return true, if the specified session instance ID belongs to a valid profiler session.
	 */
	bool IsSessionInstanceValid( const FGuid& SessionInstanceID ) const
	{
		const bool bIsValid = SessionInstanceID.IsValid() && ProfilerSessionInstances.Contains( SessionInstanceID );
		return bIsValid;
	}

	const FProfilerSessionRef* FindSessionInstance( const FGuid& SessionInstanceID ) const
	{
		return ProfilerSessionInstances.Find( SessionInstanceID );
	}

	// TODO: At this moment SButtonRowBlock::OnIsChecked supports only Checked and Unchecked
	// ECheckBoxState.Undetermined is not supported
	/**
	 * @return true, if the specified session instance tracks any stats.
	 */
	const bool IsSessionInstanceTracked( const FGuid& SessionInstanceID ) const;

	//-----------------------------------------------------------------------------

	/**
	 * @return True, if the profiler has at least one fully processed capture file
	 */
	const bool IsCaptureFileFullyProcessed() const
	{
		return bHasCaptureFileFullyProcessed;
	}

	/**
	 * @return true, if the profiler is connected to a valid session
	 */
	const bool IsConnected() const
	{
		return ActiveSession.IsValid();
	}

	/**
	 * @return true, if the profiler is currently showing the latest data
	 */
	const bool IsLivePreview() const
	{
		return bLivePreview;
	}

public:
	/** @return true, if all session instances are previewing data */
	const bool IsDataPreviewing()
	{
		return GetProfilerInstancesNum() > 0 && GetNumDataPreviewingInstances() == GetProfilerInstancesNum();
	}

	/** @return the number of session instances with data previewing enabled */
	const int32 GetNumDataPreviewingInstances()
	{
		int32 NumDataPreviewingInstances = 0;

		for( auto It = GetProfilerInstancesIterator(); It; ++It )
		{
			FProfilerSessionRef ProfilerSession = It.Value();
			NumDataPreviewingInstances += ProfilerSession->bDataPreviewing ? 1 : 0;
		}

		return NumDataPreviewingInstances;
	}

	/**
	 * Sets the data preview state for all session instances and sends message for remote profiler services.
	 *
	 * @param bRequestedDataPreviewState - data preview state that should be set
	 *
	 */
	void SetDataPreview( const bool bRequestedDataPreviewState )
	{
		ProfilerClient->SetPreviewState( bRequestedDataPreviewState );
		for( auto It = GetProfilerInstancesIterator(); It; ++It )
		{
			FProfilerSessionRef ProfilerSession = It.Value();
			ProfilerSession->bDataPreviewing = bRequestedDataPreviewState;	
		}
	}

	/** @return true, if all sessions instances are capturing data to a file, only valid if profiler is connected to network based session */
	const bool IsDataCapturing()
	{
		return GetProfilerInstancesNum() > 0 && GetNumDataCapturingInstances() == GetProfilerInstancesNum();
	}

	/** @return the number of session instances with data capturing enabled. */
	const int32 GetNumDataCapturingInstances()
	{
		int32 NumDataCapturingInstances = 0;

		for( auto It = GetProfilerInstancesIterator(); It; ++It )
		{
			FProfilerSessionRef ProfilerSession = It.Value();
			NumDataCapturingInstances += ProfilerSession->bDataCapturing ? 1 : 0;
		}

		return NumDataCapturingInstances;
	}

	/**
	 * Sets the data capture state for all session instances and sends message for remote profiler services.
	 *
	 * @param bRequestedDataCaptureState - data capture state that should be set
	 *
	 */
	void SetDataCapture( const bool bRequestedDataCaptureState );

	void ProfilerSession_OnCaptureFileProcessed( const FGuid ProfilerInstanceID );
	void ProfilerSession_OnAddThreadTime( int32 FrameIndex, const TMap<uint32, float>& ThreadMS, const FProfilerStatMetaDataRef& StatMetaData );

	/*-----------------------------------------------------------------------------
		Event graphs management
	-----------------------------------------------------------------------------*/

	void CreateEventGraphTab( const FGuid ProfilerInstanceID );
	void CloseAllEventGraphTabs();

	/*-----------------------------------------------------------------------------
		Data graphs management
	-----------------------------------------------------------------------------*/

	void DataGraph_OnSelectionChangedForIndex( uint32 FrameStartIndex, uint32 FrameEndIndex );

	/*-----------------------------------------------------------------------------
		Events declarations
	-----------------------------------------------------------------------------*/
	
public:
	DECLARE_EVENT_OneParam( FProfilerManager, FViewModeChangedEvent, EProfilerViewMode::Type /*NewViewMode*/ );
	FViewModeChangedEvent& OnViewModeChanged()
	{
		return OnViewModeChangedEvent;
	}
	
protected:
	/** The event to execute when the profiler loaded a new stats files and the view mode needs to be changed. */
	FViewModeChangedEvent OnViewModeChangedEvent;
	

public:
	/**
	 * The event to execute when the status of specified tracked stat has changed.
	 *
	 * @param const FTrackedStat&	- a reference to the tracked stat whose status has changed, this reference is valid only within the scope of function
	 * @param bool bIsTracked		- true, if stat has been added for tracking, false, if stat has been removed from tracking
	 */
	DECLARE_EVENT_TwoParams( FProfilerManager, FTrackedStatChangedEvent, const FTrackedStat&, bool );
	FTrackedStatChangedEvent& OnTrackedStatChanged()
	{
		return TrackedStatChangedEvent;
	}
	
protected:
	/** The event to execute when the status of specified tracked stat has changed. */
	FTrackedStatChangedEvent TrackedStatChangedEvent;

	/** OnMetaDataUpdated. */

public:
	/**
	 * The event to execute when a new frame has been added to the specified profiler session instance.
	 *
	 * @param const FProfilerSessionPtr& - a shared pointer to the profiler session instance which received a new frame
	 */
	DECLARE_EVENT_OneParam( FProfilerManager, FFrameAddedEvent, const FProfilerSessionPtr& );
	FFrameAddedEvent& OnFrameAdded()
	{
		return FrameAddedEvent;
	}

public:
	/**
	 * The event to be invoked once per second, for example to update the data graph.
	 */
	DECLARE_EVENT( FProfilerManager, FOneSecondPassedEvent );
	FOneSecondPassedEvent& OnOneSecondPassed()
	{
		return OneSecondPassedEvent;
	}
	
protected:
	/** The event to be invoked once per second, for example to update the data graph. */
	FOneSecondPassedEvent OneSecondPassedEvent;
	

public:
	/**
	 * The event to execute when the list of session instances have changed.
	 */
	DECLARE_EVENT( FProfilerManager, FOnSessionsUpdatedEvent );
	FOnSessionsUpdatedEvent& OnSessionInstancesUpdated()
	{
		return SessionInstancesUpdatedEvent;
	}
	
protected:
	/** The event to execute when the list of session instances have changed. */
	FOnSessionsUpdatedEvent SessionInstancesUpdatedEvent;


public:
	/**
	 * The event to execute when the filter and presets widget should be updated with the latest data.
	 */
	DECLARE_EVENT( FProfilerManager, FRequestFilterAndPresetsUpdateEvent );
	FRequestFilterAndPresetsUpdateEvent& OnRequestFilterAndPresetsUpdate()
	{
		return RequestFilterAndPresetsUpdateEvent;
	}
	
protected:
	/** The event to execute when the filter and presets widget should be updated with the latest data. */
	FRequestFilterAndPresetsUpdateEvent RequestFilterAndPresetsUpdateEvent;
	

public:
	/**
	 * Creates a new profiler session instance and loads a saved profiler capture from the specified location.
	 *
	 * @param ProfilerCaptureFilepath	- The path to the file containing a captured session instance
	 * @param bAdd						- if true, it will load a captured session instance and add to the existing ones
	 *
	 */
	void LoadProfilerCapture( const FString& ProfilerCaptureFilepath, const bool bAdd = false );

	/** Creates a new profiler session instance and load a raw stats file from the specified location. */
	void LoadRawStatsFile( const FString& RawStatsFileFileath );

protected:
	void ProfilerClient_OnProfilerData( const FGuid& InstanceID, const FProfilerDataFrame& Content, const float DataLoadingProgress );
	void ProfilerClient_OnClientConnected( const FGuid& SessioID, const FGuid& InstanceID );
	void ProfilerClient_OnClientDisconnected( const FGuid& SessionID, const FGuid& InstanceID );
	void ProfilerClient_OnMetaDataUpdated( const FGuid& InstanceID );
	void ProfilerClient_OnLoadedMetaData( const FGuid& InstanceID );
	void ProfilerClient_OnLoadCompleted( const FGuid& InstanceID );
	void ProfilerClient_OnLoadStarted( const FGuid& InstanceID );

	void ProfilerClient_OnProfilerFileTransfer( const FString& Filename, int64 FileProgress, int64 FileSize );

	void SessionManager_OnCanSelectSession( const ISessionInfoPtr& Session, bool& CanSelect );
	void SessionManager_OnInstanceSelectionChanged();
	void SessionManager_OnSelectedSessionChanged( const ISessionInfoPtr& Session );

public:
	TMap<FGuid,FProfilerSessionRef>::TIterator GetProfilerInstancesIterator()
	{
		return ProfilerSessionInstances.CreateIterator();
	}

	const int32 GetProfilerInstancesNum() const
	{
		return ProfilerSessionInstances.Num();
	}

public:
	const FLinearColor& GetColorForStatID( const uint32 StatID ) const;

protected:
	/** Updates this manager, done through FCoreTicker. */
	bool Tick( float DeltaTime );

public:

	/**
	 * Converts profiler window weak pointer to a shared pointer and returns it.
	 * Make sure the returned pointer is valid before trying to dereference it.
	 */
	TSharedPtr<class SProfilerWindow> GetProfilerWindow() const
	{
		return ProfilerWindow.Pin();
	}

	/** Sets a new view mode for the profiler. */
	void SetViewMode( EProfilerViewMode::Type NewViewMode );

protected:
	/** The delegate to be invoked when this profiler manager ticks. */
	FTickerDelegate OnTick;

	/** A weak pointer to the profiler window. */
	TWeakPtr<class SProfilerWindow> ProfilerWindow;

	/** A shared pointer to the session manager. */
	ISessionManagerPtr SessionManager;

	/** A shared pointer to the currently selected session in the session browser. */
	ISessionInfoPtr ActiveSession;

	/** Session instances currently selected in the session browser. */
	TArray<ISessionInstanceInfoPtr> SelectedSessionInstances;

	/** A shared pointer to the profiler client, which is used to deliver all profiler data from the active session. */
	IProfilerClientPtr ProfilerClient;

	/** List of UI commands for the profiler manager. This will be filled by this and corresponding classes. */
	TSharedRef< FUICommandList > CommandList;

	/** An instance of the profiler action manager. */
	FProfilerActionManager ProfilerActionManager;

	/** An instance of the profiler options. */
	FProfilerSettings Settings;

	/** A shared pointer to the global instance of the profiler manager. */
	static TSharedPtr<FProfilerManager> Instance;

	/*-----------------------------------------------------------------------------
		Events and misc
	-----------------------------------------------------------------------------*/

	/** The event to execute when a new frame has been added to the specified profiler session instance. */
	FFrameAddedEvent FrameAddedEvent;

	/** Contains all currently tracked stats, stored as StatID -> FTrackedStat. */
	TMap<uint32, FTrackedStat> TrackedStats;

	/** Holds all profiler session instances, stored as FGuid -> FProfilerSessionRef. */
	TMap<FGuid,FProfilerSessionRef> ProfilerSessionInstances;

	/*-----------------------------------------------------------------------------
		Profiler manager states
	-----------------------------------------------------------------------------*/

	/** Profiler session type that is currently initialized. */
	EProfilerSessionTypes::Type ProfilerType;

	/** Profiler view mode. */
	EProfilerViewMode::Type ViewMode;

	// TODO: Bool should be replaces with type similar to ECheckBoxState {Checked,Unchecked,Undertermined}

	/** True, if the profiler is currently showing the latest data, only valid if profiler is connected to network based session. */
	bool bLivePreview;

	/** True, if the profiler has at least one fully processed capture file. */
	bool bHasCaptureFileFullyProcessed;
};
