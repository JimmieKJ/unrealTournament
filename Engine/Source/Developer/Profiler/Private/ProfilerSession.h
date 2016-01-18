// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/*-----------------------------------------------------------------------------
	Enumerators
-----------------------------------------------------------------------------*/

/** Enumerates profiler session type. */
namespace EProfilerSessionTypes
{
	enum Type
	{
		/** Based on the live connection. */
		Live,
		LiveRaw,

		/** Based on the regular stats file. */
		StatsFile,

		/** Based on the raw stats file. */
		StatsFileRaw,

		Combined,
		Summary,

		/** Invalid enum type, may be used as a number of enumerations. */
		InvalidOrMax,
	};

	/**
	 * Returns the string representation of the specified EProfilerSessionType value.
	 *
	 * @param ProfilerSessionType - The value to get the string for.
	 *
	 * @return A string value.
	 */
	inline FString ToString( EProfilerSessionTypes::Type ProfilerSessionType )
	{
		// @TODO: Localize
		switch( ProfilerSessionType )
		{
			case Live: return FString("Live");
			case StatsFile: return FString("Offline");

			default: return FString("InvalidOrMax");
		}
	}
}

/** Enumerates loading a capture file progress states. */
namespace ELoadingProgressStates
{
	enum Type
	{
		Started,
		InProgress,
		Loaded,
		Failed, 

		/** Invalid enum type, may be used as a number of enumerations. */
		InvalidOrMax,
	};
}

namespace EProfilerNotificationTypes
{
	enum Type
	{
		LoadingOfflineCapture,
		SendingServiceSideCapture,
	};
}

/*-----------------------------------------------------------------------------
	Declarations
-----------------------------------------------------------------------------*/

/**	Contains information about stat like name, ID and type, and information about the owning stat group. */
class FProfilerStat : public FNoncopyable
{
	friend class FProfilerStatMetaData;

public:
	/**
	 * Default constructor.
	 *
	 * @param InID		- the unique ID for this stat
	 *
	 */
	FProfilerStat( const uint32 InStatID = 0 );

protected:
	/**
	 * Sets the new description for this stat.
	 *
	 * @param InName	- the name of this stat
	 * @param InGroupID - the pointer to the group that this stat belongs to
	 * @param InType	- the type of this stat
	 *
	 */
	void Initialize( const FString& InName, FProfilerGroup* InOwningGroupPtr, const EStatType InType )	
	{
		// Skip leading spaces in the name of the stat.
		int32 Index = 0;
		while( FChar::IsWhitespace( InName[Index] ) )
		{
			Index++;
		}

		const FString CleanedName = *InName+Index;
		_Name = *CleanedName;
		_OwningGroupPtr = InOwningGroupPtr;
		_Type = ConvertStatTypeToProfilerSampleType(InType);
	}

	/**
	 * @return a profiler sample type from the specified generic stat type
	 */
	static const EProfilerSampleTypes::Type ConvertStatTypeToProfilerSampleType( const EStatType StatType )
	{
		EProfilerSampleTypes::Type ProfilerUnit = EProfilerSampleTypes::InvalidOrMax;

		switch( StatType )
		{	
		case STATTYPE_MemoryCounter:
			{
				ProfilerUnit = EProfilerSampleTypes::Memory;
				break;
			}

		case STATTYPE_AccumulatorFLOAT:
		case STATTYPE_CounterFLOAT:
			{
				ProfilerUnit = EProfilerSampleTypes::NumberFloat;
				break;
			}

		case STATTYPE_AccumulatorDWORD:
		case STATTYPE_CounterDWORD:
			{
				ProfilerUnit = EProfilerSampleTypes::NumberInt;
				break;
			}

		case STATTYPE_CycleCounter:
			{
				ProfilerUnit = EProfilerSampleTypes::HierarchicalTime;
				break;
			}
		case STATTYPE_Error:
			break;
		}

		return ProfilerUnit;
	}

public:
	/**
	 * @return the display name for this stat.
	 */
	const FName& Name() const
	{
		return _Name;
	}

	/**
	 * @return a reference to the group that this stat belongs to.
	 */
	const FProfilerGroup& OwningGroup() const
	{
		check( _OwningGroupPtr != NULL );
		return *_OwningGroupPtr;
	}

	/**
	 * @return the unique ID for this stat.
	 */
	const uint32 ID() const
	{
		return _ID;
	}

	/**
	 * @return the type of this stat.
	 */
	const EProfilerSampleTypes::Type Type() const
	{
		return _Type;
	}

	/**
	 * @return number of bytes allocated by this stat.
	 */
	const SIZE_T GetMemoryUsage() const
	{
		return sizeof(_Name) + sizeof(*this);
	}

	/**
	 * @return a pointer to the default profiler stat.
	 */
	static FProfilerStat* GetDefaultPtr()
	{
		return &Default;
	}

protected:
	/**	The display name for this stat. */
	FName _Name;

	/**	The pointer to the group that this stat belongs to. */
	FProfilerGroup* _OwningGroupPtr;

	/** The unique ID for this stat. */
	const uint32 _ID;

	/**	Holds the type of this stat. */
	EProfilerSampleTypes::Type _Type;

	/**	Default profiler stat, used for uninitialized stats. */
	static FProfilerStat Default;
};
template <> struct TIsPODType<FProfilerStat> { enum { Value = true }; };

/**
 *	Contains information about stat group and stats associated with the specified stat group.
 */
class FProfilerGroup : public FNoncopyable
{
	friend class FProfilerStatMetaData;

public:
	/**
	 * Default constructor.
	 */
	FProfilerGroup()
		: _Name( TEXT("(Group-Default)") )
		, _ID( 0 )
	{}

protected:
	/**
	 * Initialization constructor.
	 *
	 * @param InID	- the unique ID for this stat group
	 *
	 */
	FProfilerGroup( const uint32 InID )
		: _Name( *FString::Printf( TEXT("(Group-%04u)"), InID ) )
		, _ID( InID )
	{}

	/**
	 * Sets the new name for this stat group.
	 *
	 * @param InName - the name of this stat group 
	 *
	 */
	void Initialize( const FString& InName )
	{
		_Name = *InName;
	}

	/**
	 * Adds a stat to this group.
	 *
	 * @param ProfilerStat - a pointer to the existing stat object
	 * 
	 */
	void AddStat( FProfilerStat* ProfilerStat )
	{
		OwnedStats.Add( ProfilerStat );
	}

public:
	/**
	 * @return a reference to the list of stats that are in this stat group.
	 */
	const TArray<FProfilerStat*>& GetStats() const
	{
		return OwnedStats;
	}

	/**
	 * @return the display name for this stat group.
	 */
	const FName& Name() const
	{
		return _Name;
	}

	/**
	 * @return the unique ID for this stat group.
	 */
	const uint32 ID() const
	{
		return _ID;
	}

	/**
	 * @return number of bytes allocated by this group.
	 */
	const SIZE_T GetMemoryUsage() const
	{
		return OwnedStats.GetAllocatedSize() + sizeof(_Name) + sizeof(*this);
	}

	/**
	 * @return a pointer to the default profiler group.
	 */
	static FProfilerGroup* GetDefaultPtr()
	{
		return &Default;
	}

protected:
	/**	Contains a list of stats that are in this stat group. */
	TArray<FProfilerStat*> OwnedStats;

	/** The display name for this stat group. */
	FName _Name;
	
	/** The unique ID for this stat group. */
	const uint32 _ID;

	/**	Default profiler group, used for uninitialized stats. */
	static FProfilerGroup Default;
};

/**
 * Structure holding the metadata describing the various stats and data associated with them.
 * Critical data like stat and group description are stored in arrays of predefined size to allow constant time access.
 */
class FProfilerStatMetaData : public FNoncopyable, public TSharedFromThis<FProfilerStatMetaData>
{
	friend class FProfilerSession;
	friend class FRawProfilerSession;

protected:
	/** Constructor. */
	FProfilerStatMetaData()
		: SecondsPerCycle( FPlatformTime::GetSecondsPerCycle() )
		, GameThreadID( 0 )
	{}

public:
	/** Destructor. */
	~FProfilerStatMetaData()
	{
		// Delete all allocated descriptions.
		for( auto It = StatDescriptions.CreateConstIterator(); It; ++It )
		{
			delete It.Value();
		}
		StatDescriptions.Empty();

		for( auto It = GroupDescriptions.CreateConstIterator(); It; ++It )
		{
			delete It.Value();
		}
		GroupDescriptions.Empty();
	}

	/**
	 * @return number of bytes allocated by this instance of stat metadata.
	 */
	const SIZE_T GetMemoryUsage() const
	{
		SIZE_T MemorySize = 0;

		MemorySize += sizeof(*this);
		MemorySize += StatDescriptions.GetAllocatedSize();
		for( auto It = StatDescriptions.CreateConstIterator(); It; ++It )
		{
			MemorySize += It.Value()->GetMemoryUsage();
		}

		MemorySize += GroupDescriptions.GetAllocatedSize();
		for( auto It = GroupDescriptions.CreateConstIterator(); It; ++It )
		{
			MemorySize += It.Value()->GetMemoryUsage();
		}

		MemorySize += GetThreadDescriptions().GetAllocatedSize();

		return MemorySize;
	}

protected:
	/**
	 * Updates this instance of stat metadata.
	 *
	 * @param StatMetaData - the stat metadata received from the connected profiler client
	 *
	 */
	void Update( const FStatMetaData& ClientStatMetaData );

public:
	/**
	 *	Updates this stats metadata based on the stats thread state.
	 *	This is a temporary solution to make it working with current implementation of FProfilerSample.
	 */
	void UpdateFromStatsState( const FStatsThreadState& StatsThreadStats );

private:

	/**
	 * Initialized the specified stat group.
	 * If specified stat group doesn't exist, adds it to the list of stat group descriptions.
	 *
	 * @param GroupID	- the ID of the stat group that needs to be initialized
	 * @param GroupName	- the name of the stat group that needs to be initialized
	 *
	 */
	void InitializeGroup( const uint32 GroupID, const FString& GroupName ) 
	{
		FProfilerGroup* GroupPtr = GroupDescriptions.FindRef( GroupID );
		if( !GroupPtr )
		{
			GroupPtr = GroupDescriptions.Add( GroupID, new FProfilerGroup( GroupID ) );
		}
		GroupPtr->Initialize( GroupName );
	}

	/**
	 * Initialized the specified stat.
	 *
	 * @param StatID	- the id of the stat that needs to be initialized
	 * @param GroupID	- the id of the stat group that will be assigned to the specified stat
	 * @param StatName  - the name of the stat that needs to be initialized
	 * @param InType	- the type of the stat that needs to be initialized
	 * @param StatName	- the unique stat name
	 *
	 */
	void InitializeStat( const uint32 StatID, const uint32 GroupID, const FString& StatName, const EStatType InType, FName StatFName = NAME_None )	
	{
		FProfilerStat* StatPtr = StatDescriptions.FindRef( StatID );
		if( !StatPtr )
		{
			StatPtr = StatDescriptions.Add( StatID, new FProfilerStat( StatID ) );

			if( StatFName != NAME_None )
			{
				StatFNameDescriptions.Add( StatFName, StatPtr );
			}

			// @TODO yrx 2014-03-24 FindRef!
			FProfilerGroup* GroupPtr = GroupDescriptions.FindRef( GroupID );

			StatPtr->Initialize( StatName, GroupPtr, InType );

			static const FName NAME_Threads(TEXT("Threads"));
			if( StatFName == NAME_None && GroupPtr->Name() == NAME_Threads )
			{
				// Check if this stat is a thread stat.
				const uint32 ThreadID = FStatsUtils::ParseThreadID( StatPtr->Name().ToString() );
				if( ThreadID != 0 )
				{
					const FString* ThreadDesc = GetThreadDescriptions().Find( ThreadID );
					if( ThreadDesc )
					{
						// Replace the stat name with a thread name.
						const FString UniqueThreadName = FString::Printf( TEXT( "%s [0x%x]" ), **ThreadDesc, ThreadID );
						StatPtr->_Name = *UniqueThreadName;
						ThreadIDtoStatID.Add( ThreadID, StatID );

						// Game thread is always NAME_GameThread
						if( **ThreadDesc == FName( NAME_GameThread ) )
						{
							GameThreadID = ThreadID;
						}
						// Rendering thread may be "Rendering thread" or NAME_RenderThread with an index
						else if( ThreadDesc->Contains( FName( NAME_RenderThread ).GetPlainNameString() ) )
						{
							RenderThreadIDs.AddUnique( ThreadID );
						}
						else if( ThreadDesc->Contains( TEXT( "RenderingThread" ) ) )
						{
							RenderThreadIDs.AddUnique( ThreadID );
						}
					}			
				}
			}

			if( GroupPtr != FProfilerGroup::GetDefaultPtr() )
			{
				GroupPtr->AddStat( StatPtr );
			}
		}
	}

public:
	/**
	 * @return a reference to the stat description specified by the stat ID
	 */
	const FProfilerStat& GetStatByID( const uint32 StatID ) const
	{
		return *StatDescriptions.FindChecked( StatID );
	}

	/**
	* @return a reference to the stat description specified by the unique stat name.
	*/
	const FProfilerStat& GetStatByFName( FName StatName ) const
	{
		return *StatFNameDescriptions.FindChecked( StatName );
	}

	/**
	 * @return an instance of a const iterator for the stat descriptions.
	 */
	const TMap<uint32,FProfilerStat*>::TConstIterator GetStatIterator() const
	{
		return StatDescriptions.CreateConstIterator();
	}

	/**
	 * @return a reference to the group description specified by the group ID
	 */
	const FProfilerGroup& GetGroup( const uint32 GroupID ) const
	{
		return *GroupDescriptions.FindChecked( GroupID );
	}

	/**
	 * @return an instance of a const iterator for the stat group descriptions.
	 */
	const TMap<uint32,FProfilerGroup*>::TConstIterator GetGroupIterator() const
	{
		return GroupDescriptions.CreateConstIterator();
	}

	/**
	 * @return a reference to the thread descriptions.
	 */
	const TMap<uint32, FString>& GetThreadDescriptions() const
	{
		return ThreadDescriptions;
	}

	/**
	 * @return seconds per CPU cycle
	 */
	const double GetSecondsPerCycle() const
	{
		return SecondsPerCycle;
	}

	/**
	 * @return the specified number of cycles converted to milliseconds.
	 */
	const double ConvertCyclesToMS( const uint32 Cycles ) const
	{
		return SecondsPerCycle * 1000 * Cycles;
	}

	/**
	 * @return the profiler sample type for the specified stat ID.
	 */
	const EProfilerSampleTypes::Type GetSampleTypeForStatID( const uint32 StatID ) const
	{
		return GetStatByID( StatID ).Type();
	}

	/**
	 * @return true, if the stat for the specified stat ID has been initialized.
	 */
	const bool IsStatInitialized( const uint32 StatID )
	{
		return StatDescriptions.Contains( StatID );
	}

	const uint32 GetGameThreadID() const
	{
		return GameThreadID;
	}

	const TArray<uint32>& GetRenderThreadID() const
	{
		return RenderThreadIDs;
	}

	bool IsReady() const
	{
		return GameThreadID != 0;
	}

	const uint32 GetGameThreadStatID() const
	{
		return ThreadIDtoStatID.FindChecked( GameThreadID );
	}

protected:
	/** All initialized stat descriptions, stored as StatID -> FProfilerStat. */
	TMap<uint32,FProfilerStat*> StatDescriptions;

	/** All initialized stat descriptions, stored as unique stat name -> FProfilerStat. */
	TMap<FName, FProfilerStat*> StatFNameDescriptions;

	/** All initialized stat group descriptions, stored as StatID -> FProfilerGroup. */
	TMap<uint32,FProfilerGroup*> GroupDescriptions;

	/** Thread descriptions, stored as ThreadID -> ThreadDesc/Name. */
	TMap<uint32, FString> ThreadDescriptions;

	/**
	 *	Helper map, as a part of migration into stats2, stored as ThreadID -> StatID.
	 *	Used during creating a thread sample, so we can use the real stat id and get data graph for this thread 
	 */
	TMap<uint32,uint32> ThreadIDtoStatID;

	/** Seconds per CPU cycle. */
	double SecondsPerCycle;

	/** Game thread id. */
	uint32 GameThreadID;

	/** Array of all render thread ids. */
	TArray<uint32> RenderThreadIDs;
};

/** Holds the aggregated information for the specific stat across all frames that have been captured. */
class FProfilerAggregatedStat
{
	friend class FProfilerSession;

public:
	enum Type
	{
		EAvgValue,
		EMinValue,
		EMaxValue,

		EAvgNumCalls,
		EMinNumCalls,
		EMaxNumCalls,

		EFramesWithCallPct,

		EInvalidOrMax
	};

	/** Default constructor. */
	FProfilerAggregatedStat( const FName& InStatName, const FName& InGroupName, EProfilerSampleTypes::Type InStatType )
		: _StatName( InStatName )
		, _GroupName( InGroupName )
		, _ValueOneFrame( 0.0f )
		, _ValueAllFrames( 0.0f )
		, _MinValueAllFrames( FLT_MAX )
		, _MaxValueAllFrames( FLT_MIN )
	
		, _NumCallsAllFrames( 0 )
		, _NumCallsOneFrame( 0 )
		, _MinNumCallsAllFrames( MAX_uint32 )
		, _MaxNumCallsAllFrames( 0 )

		, _NumFrames( 0 )
		, _NumFramesWithCall( 0 )

		, StatType( InStatType )
	{}

public:
	/** @return The average value of all combined instances. */
	const double AvgValue() const
	{
		return _ValueAllFrames / (double)_NumFrames;
	}

	/** @return The min value of all combined instances. */
	const double MinValue() const
	{
		return _MinValueAllFrames;
	}

	/** @return The max value of all combined instances. */
	const double MaxValue() const
	{
		return _MaxValueAllFrames;
	}


	/** @return The average number of calls of all combined instances. */
	const float AvgNumCalls() const 
	{
		return (double)_NumCallsAllFrames / (double)_NumFrames;
	}

	/** @return The min number of calls of all combined instances. */
	const float MinNumCalls() const
	{
		return (float)_MinNumCallsAllFrames;
	}

	/** @return The max number of calls of all combined instances. */
	const float MaxNumCalls() const
	{
		return (float)_MaxNumCallsAllFrames;
	}

	/**
	 * @return the percentage of how often the stat is called, only matters for the hierarchical stat
	 */
	const float FramesWithCallPct() const
	{
		return (double)_NumFramesWithCall / (double)_NumFrames * 100.0f;
	}

	/**
	 * @return true, if this aggregated has a valid calls stats, only matters for the hierarchical stat.
	 */
	const bool HasCalls() const
	{
		return StatType == EProfilerSampleTypes::HierarchicalTime;
	}

	/**
	 * @return a string representation of this aggregated stat.
	 */
	FString ToString() const
	{
		FString FormattedValueStr;
		if( StatType == EProfilerSampleTypes::HierarchicalTime )
		{
			FormattedValueStr = FString::Printf( TEXT("{Value Min:%.3f Avg:%.3f Max:%.3f (MS) / Calls (%.1f%%) Min:%.1f Avg:%.1f Max:%.1f}"), 
				MinValue(), AvgValue(), MaxValue(),
				FramesWithCallPct(), MinNumCalls(), AvgNumCalls(), MaxNumCalls() );
		}
		else if( StatType == EProfilerSampleTypes::Memory )
		{
			FormattedValueStr = FString::Printf( TEXT("{Min:%.2f Avg:%.2f Max:%.2f (KB)}"), MinValue(), AvgValue(), MaxValue() );
		}
		else if( StatType == EProfilerSampleTypes::NumberInt )
		{
			FormattedValueStr = FString::Printf( TEXT("{Min:%.2f Avg:%.2f Max:%.2f}"), MinValue(), AvgValue(), MaxValue() );
		}
		else if( StatType == EProfilerSampleTypes::NumberFloat )
		{
			FormattedValueStr = FString::Printf( TEXT("{Min:%.2f Avg:%.2f Max:%.2f}"), MinValue(), AvgValue(), MaxValue() );
		}
		else
		{
			check(0);
		}
		return FormattedValueStr;
	}

	
	/**
	 * @return a string representation of the specified value type.
	 */
	FString GetFormattedValue( const Type ValueType ) const
	{
		check( ValueType < Type::EInvalidOrMax );
		const double ValueArray[Type::EInvalidOrMax] =
		{
			AvgValue(),
			MinValue(),
			MaxValue(),

			AvgNumCalls(),
			MinNumCalls(),
			MaxNumCalls(),
		
			FramesWithCallPct(),
		};

		FString FormatValueStr;
		if( StatType == EProfilerSampleTypes::HierarchicalTime )
		{
			if( ValueType == EMinValue || ValueType == EAvgValue || ValueType == EMaxValue )
			{
				FormatValueStr = FString::Printf( TEXT("%.3f (MS)"), ValueArray[ValueType] );
			}
			else if( ValueType == EFramesWithCallPct )
			{
				FormatValueStr = FString::Printf( TEXT("%.1f%%"), ValueArray[EFramesWithCallPct] );
			}
			else
			{
				FormatValueStr = FString::Printf( TEXT("%.1f"), ValueArray[ValueType] );
			}
		}
		else if( StatType == EProfilerSampleTypes::Memory )
		{
			FormatValueStr = FString::Printf( TEXT("%.2f (KB)"), ValueArray[ValueType] );
		}
		else if( StatType == EProfilerSampleTypes::NumberInt )
		{
			FormatValueStr = FString::Printf( TEXT("%.2f"), ValueArray[ValueType] );
		}
		else if( StatType == EProfilerSampleTypes::NumberFloat )
		{
			FormatValueStr = FString::Printf( TEXT("%.2f"), ValueArray[ValueType] );
		}
		else
		{
			check(0);
		}
		return FormatValueStr;
	}

protected:
	/** Called once a frame to update aggregates. */
	void Advance()
	{
		_NumFrames++;

		_NumCallsAllFrames += _NumCallsOneFrame;
		_ValueAllFrames += _ValueOneFrame;

		// Calculate new extreme values.
		if( StatType == EProfilerSampleTypes::HierarchicalTime )
		{
			// Update the extreme values only if this stat has been called at least once.
			if( _NumCallsOneFrame > 0 )
			{
				_NumFramesWithCall ++;
			}

			{
				_MinValueAllFrames = FMath::Min<float>( _MinValueAllFrames, _ValueOneFrame );
				_MaxValueAllFrames = FMath::Max<float>( _MaxValueAllFrames, _ValueOneFrame );

				_MinNumCallsAllFrames = FMath::Min<float>( _MinNumCallsAllFrames, _NumCallsOneFrame );
				_MaxNumCallsAllFrames = FMath::Max<float>( _MaxNumCallsAllFrames, _NumCallsOneFrame );
			}
		}
		else
		{
			_MinValueAllFrames = FMath::Min<float>( _MinValueAllFrames, _ValueOneFrame );
			_MaxValueAllFrames = FMath::Max<float>( _MaxValueAllFrames, _ValueOneFrame );
		}

		_ValueOneFrame = 0.0f;
		_NumCallsOneFrame = 0;
	}

	/**
	 * Adds a profiler sample to our aggregated data
	 *
	 * @param Sample - a reference to the profiler sample that will be aggregated
	 *
	 */
	FProfilerAggregatedStat& operator+=( const FProfilerSample& Sample )
	{
		double TypedValue = 0.0;

		// Determine whether to we are reading a time hierarchical sample or not.
		if( Sample.Type() == EProfilerSampleTypes::HierarchicalTime )
		{
			TypedValue = Sample.DurationMS();
			_NumCallsOneFrame += (uint32)Sample.CounterAsInt();
		}
		else
		{
			TypedValue = Sample.CounterAsFloat();

			if( Sample.Type() == EProfilerSampleTypes::Memory )
			{
				// @TODO: Remove later
				TypedValue *= 1.0f / 1024.0f;
			}
		}

		_ValueOneFrame += TypedValue;
		
		return *this;
	}

protected:
	FName _StatName;
	FName _GroupName;

	/** The accumulated value of all instances for this stat for one frame. */
	double _ValueOneFrame;

	/** The accumulated value of all instances for this stat across all frames. */
	double _ValueAllFrames;

	/** The minimum value of all instances for this stat across all frames. */
	double _MinValueAllFrames;

	/** The maximum value of all instances for this stat across all frames. */
	double _MaxValueAllFrames;


	/** The number of times this stat has been called on all frames. */
	uint64 _NumCallsAllFrames;

	/** The number of times this stat has been called on one frame. */
	uint32 _NumCallsOneFrame;

	/** The minimum number of times this stat has been called on all frames. */
	uint32 _MinNumCallsAllFrames;

	/** The maximum number of times this stat has been called on all frames. */
	uint32 _MaxNumCallsAllFrames;


	/** Number of frames. */
	uint32 _NumFrames;

	/** Number of frames with a least one call to this stat. */
	uint32 _NumFramesWithCall;

	/** Stat type. */
	const EProfilerSampleTypes::Type StatType;
};

// @TODO: Standardize naming used in the profiler session/ session / session instance.

/** Class that holds the profiler session information. */
class FProfilerSession : public TSharedFromThis<FProfilerSession>
{
	friend class FProfilerManager;
	friend class FProfilerActionManager;

protected:
	/** Default constructor. */
	FProfilerSession
	( 
		EProfilerSessionTypes::Type InSessionType, 
		const ISessionInstanceInfoPtr InSessionInstanceInfo,
		FGuid InSessionInstanceID,
		FString InDataFilepath
	);

public:
	/**
	 * Initialization constructor, creates a profiler session from a capture file.
	 */
	FProfilerSession( const FString& InDataFilepath );

	/**
	 * Initialization constructor, creates a live profiler session.
	 */
	FProfilerSession( const ISessionInstanceInfoPtr InSessionInstanceInfo );

	/** Destructor. */
	~FProfilerSession();

	/** Updates this profiler session. */
	bool HandleTicker( float DeltaTime );

public:
	typedef TMap<uint32, float> FThreadTimesMap;
	DECLARE_DELEGATE_ThreeParams( FAddThreadTimeDelegate, int32 /*FrameIndex*/, const FThreadTimesMap& /*ThreadMS*/, const FProfilerStatMetaDataRef& /*StatMetaData*/ );

	FProfilerSession& SetOnAddThreadTime( const FAddThreadTimeDelegate& InOnAddThreadTime )
	{
		OnAddThreadTime = InOnAddThreadTime;
		return *this;
	}

protected:
	FAddThreadTimeDelegate OnAddThreadTime;

public:
	/**
	 * The delegate to execute when this profiler session has fully processed a capture file and profiler manager can update all widgets.
	 *
	 * @param SessionInstanceID - the session instance ID of this session, used to determine of which session this method should be called
	 *
	 */
	DECLARE_DELEGATE_OneParam( FCaptureFileProcessedDelegate, const FGuid /*SessionInstanceID*/ );

	/**
	 * Sets the handler that will be invoked when this profiler session has fully processed a capture file
	 * 
	 * @return This instance (for method chaining).
	 */
	FProfilerSession& SetOnCaptureFileProcessed( const FCaptureFileProcessedDelegate& InOnCaptureFileProcessed )
	{
		OnCaptureFileProcessed = InOnCaptureFileProcessed;
		return *this;
	}
	
protected:
	/** The delegate to execute when this profiler session has fully processed a capture file and profiler manager can update all widgets. */
	FCaptureFileProcessedDelegate OnCaptureFileProcessed;
	
public:
	
	/**
	 * @return the name of this profiler session
	 */
	const FString GetName() const
	{
		FString SessionName;
		if( SessionType == EProfilerSessionTypes::Live )
		{
			SessionName = FString::Printf( TEXT("%s"), *SessionInstanceInfo->GetInstanceName() );
		}
		else if( SessionType == EProfilerSessionTypes::StatsFile )
		{
			SessionName = FString::Printf( TEXT("%s"), *DataFilepath );
		}
		else if( SessionType == EProfilerSessionTypes::StatsFileRaw )
		{
			SessionName = FString::Printf( TEXT( "%s" ), *DataFilepath );
		}
		
		return SessionName;
	}

	/**
	 * @return the short name of this profiler session
	 */
	const FString GetShortName() const
	{
		return FProfilerHelper::ShortenName( GetName(), 12 );
	}

	/**
	 * @return session type for this profiler session
	 */
	const EProfilerSessionTypes::Type GetSessionType() const
	{
		return SessionType;
	}

	/**
	 * @return an unique session instance ID.
	 */
	const FGuid GetInstanceID() const
	{
		return SessionInstanceID;
	}

	/**
	 * @return the time when this profiler session was created ( time of the connection to the client, time when a profiler capture was created ).
	 */
	const FDateTime& GetCreationTime() const
	{
		return CreationTime;
	}

	/**
	 * @return a shared reference to the the data provider which holds all the collected profiler samples.
	 */
	const IDataProviderRef& GetDataProvider() const
	{
		return DataProvider;
	}

	/**
	 * @return a shared reference to the stat metadata which holds all collected stats descriptions.
	 */
	const FProfilerStatMetaDataRef& GetMetaData() const
	{
		return StatMetaData;
	}

	/**
	 * @return a const pointer to the aggregated stat for the specified stat ID or null if not found.
	 */
	FORCEINLINE_DEBUGGABLE const FProfilerAggregatedStat* GetAggregatedStat( const uint32 StatID ) const
	{
		return AggregatedStats.Find( StatID );
	}

	const FEventGraphDataRef& GetEventGraphDataMaximum() const
	{
		return EventGraphDataMaximum;
	}

	const FEventGraphDataRef GetEventGraphDataAverage() const
	{
		const uint32 NumFrames = EventGraphDataTotal->GetNumFrames();
		FEventGraphDataRef EventGraphDataAverage = EventGraphDataTotal->DuplicateAsRef();
		EventGraphDataAverage->Divide( (double)NumFrames );
		EventGraphDataAverage->Advance( 0, NumFrames );

		return EventGraphDataAverage;
	}

	/**
	 * @return number of bytes allocated by this profiler session.
	 */
	const SIZE_T GetMemoryUsage() const
	{
		SIZE_T MemoryUsage = 0;
		MemoryUsage += DataProvider->GetMemoryUsage();
		MemoryUsage += StatMetaData->GetMemoryUsage();
		return MemoryUsage;
	}

	/**
	 * @return a new instance of the graph data source for the specified stat ID.
	 */
	FGraphDataSourceRefConst CreateGraphDataSource( const uint32 InStatID );

	/**
	 * @return a new instance of the event graph data for the specified frame range and event graph type.
	 */
	FEventGraphDataRef CreateEventGraphData( const uint32 FrameStartIndex, const uint32 FrameEndIndex, const EEventGraphTypes::Type EventGraphType );

protected:

	/**
	 * Recursively populates hierarchy based on the specified profiler cycle graph.
	 *
	 * @param ParentGraph - <describe ParentGraph>
	 * @param ParentStartTimeMS - <describe ParentStartTimeMS>
	 * @param ParentDurationMS - <describe ParentDurationMS>
	 * @param ParentSampleIndex - <describe ParentSampleIndex>
	 *
	 * @return <describe return value>
	 */
	void PopulateHierarchy_Recurrent
	( 
		const FProfilerCycleGraph& ParentGraph, 
		const double ParentStartTimeMS, 
		const double ParentDurationMS, 
		const uint32 ParentSampleIndex 
	);

	/** Called when this profiler session receives a new profiler data. */
	void UpdateProfilerData( const FProfilerDataFrame& Content );

	/** Called when this profiler session receives information that the meta data has been updated. @see FProfilerMetaDataUpdateDelegate and IProfilerClient */
	void UpdateMetadata( const FStatMetaData& InClientStatMetaData );

	/**
	 * Updates aggregated stats.
	 *
	 * @param FrameIndex - the index of the profiler samples that will be used to update the aggregated stats.
	 *
	 */
	void UpdateAggregatedStats( const uint32 FrameIndex );

	/**
	 * Updates aggregated event graphs.
	 *
	 * @param FrameIndex - the index of the profiler samples that will be used to update the aggregated stats.
	 *
	 */
	void UpdateAggregatedEventGraphData( const uint32 FrameIndex );

	void EventGraphCombineAndMax( const FEventGraphDataRef Current, const uint32 NumFrames );

	void EventGraphCombineAndAdd( const FEventGraphDataRef Current, const uint32 NumFrames );

	/** Called when the capture file has been fully loaded. */
	void LoadComplete();

protected:
	/** All aggregated stats, stored as StatID -> FProfilerAggregatedStat. */
	TMap<uint32, FProfilerAggregatedStat > AggregatedStats;

	TMap<uint32, FProfilerAggregatedStat > LastFrameStats;

	// TODO: This part will go away once we fully switch over to the stats2.
	//{
	/** Profiler data collected for the previous frames. */
	TMap<uint32, FProfilerDataFrame> FrameToProfilerDataMapping;
	/** Frame indices that should be processed by the profiler manager. */
	TArray<uint32> FrameToProcess;
	/** Reference to the client's stat metadata. */
	const FStatMetaData* ClientStatMetadata;
	/** If true, we need to update the metadata before we update the data provider. */
	bool bRequestStatMetadataUpdate;
	bool bLastPacket;
	uint32 StatMetaDataSize;
	//}

	/** The delegate to be invoked when this profiler session instance ticks. */
	FTickerDelegate OnTick;

	/** Handle to the registered OnTick. */
	FDelegateHandle OnTickHandle;

	/** The data provider which holds all the collected profiler samples. */
	IDataProviderRef DataProvider;

	/** The stat metadata which holds all collected stats descriptions. */
	FProfilerStatMetaDataRef StatMetaData;

	// TODO: Make this more universal ? FEventGraphDataRef<AggregateFunc,FinalizerFunc,bRequiredCopy>

	/** Aggregated event graph data for all collected frames, used for generating average values. */
	FEventGraphDataRef EventGraphDataTotal;

	/** Aggregated event graph data for all collected frames, contains only maximum inclusive times. */
	FEventGraphDataRef EventGraphDataMaximum;

	/** Temporary event graph data for the specified frame. */
	FEventGraphDataRef EventGraphDataCurrent;

	/** Event graph completion sync ( combine for max, combine for add ) that can be done in parallel, but need to wait before combining the next frame. */
	FGraphEventRef CompletionSync;

	// TODO: Temporary, need to be stored in metadata or send via UMB.
	/** The time when this profiler session was created ( time of the connection to the client, time when a capture file was created ). */
	/*const*/ FDateTime CreationTime;

	/** Session type for this profiler session. */
	/*const*/ EProfilerSessionTypes::Type SessionType;

	/** Shared pointer to the session instance info. */
	const ISessionInstanceInfoPtr SessionInstanceInfo;

	/** An unique session instance ID. */
	/*const*/ FGuid SessionInstanceID;

	/** Data filepath. */
	/*const*/ FString DataFilepath;

	/** True, if this profiler session instance is currently previewing data, only valid if profiler is connected to network based session. */
	bool bDataPreviewing;

	/** True, if this profiler session instance is currently capturing data to a file, only valid if profiler is connected to network based session. */
	bool bDataCapturing;

	/** True, if this profiler session instance has the whole profiler data, but it still needs to be processed by the tick method. */
	bool bHasAllProfilerData;

public:
	/** Provides analysis of the frame rate */
	TSharedRef<FFPSAnalyzer> FPSAnalyzer;
};

/*-----------------------------------------------------------------------------
	Profiler session for the raw stats files
-----------------------------------------------------------------------------*/

struct FAllocationInfo;

class FRawProfilerSession : public FProfilerSession
{
	friend class FProfilerManager;
	friend class FProfilerActionManager;

	/** Profiler stream that contains all read raw profiler frames. */
	FProfilerStream ProfilerStream;

	/** Stats thread state, mostly used to manage the stats metadata. */
	FStatsThreadState StatsThreadStats;
	FStatsReadStream Stream;

	/** Index of the last processed data for the mini-view. */
	int32 CurrentMiniViewFrame;

public:
	/**
	* Default constructor, creates a profiler session from a capture file.
	*/
	FRawProfilerSession( const FString& InRawStatsFileFileath );

	/** Destructor. */
	~FRawProfilerSession();

	/** Updates this profiler session. */
	bool HandleTicker( float DeltaTime );

	/** Starts a process of loading the raw stats file. */
	void PrepareLoading();

	const FProfilerStream& GetStream() const
	{
		return ProfilerStream;
	}

	/**
	 *	Process all stats packets and convert them to data accessible by the profiler.
	 *	Temporary version, will be optimized later.
	 */
	void ProcessStatPacketArray( const FStatPacketArray& PacketArray, FProfilerFrame& out_ProfilerFrame, int32 FrameIndex );
};