// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ProfilerPrivatePCH.h"
#define LOCTEXT_NAMESPACE "ProfilerDataSource"

/*-----------------------------------------------------------------------------
	FGraphDataSource
-----------------------------------------------------------------------------*/

const int32 FTimeAccuracy::_FPS008 = 8;
const int32 FTimeAccuracy::_FPS015 = 15;
const int32 FTimeAccuracy::_FPS030 = 30;
const int32 FTimeAccuracy::_FPS060 = 60;
const int32 FTimeAccuracy::_FPS120 = 120;

/*-----------------------------------------------------------------------------
	FGraphDataSource
-----------------------------------------------------------------------------*/

FGraphDataSource::FGraphDataSource( const FProfilerSessionRef& InProfilerSession, const uint32 InStatID ) 
	: FGraphDataSourceDescription( InStatID )
	, ThisCachedDataByIndex()
	, ThisCachedDataByTime( FTimeAccuracy::FPS060 )
	, ProfilerSession( InProfilerSession ) 
{
	const FProfilerStatMetaDataRef MetaData = ProfilerSession->GetMetaData();
	const FProfilerStat& Stat = MetaData->GetStatByID(InStatID);
	const FProfilerGroup& Group = Stat.OwningGroup();

	Initialize( Stat.Name().GetPlainNameString(), Group.ID(), Group.Name().GetPlainNameString(), Stat.Type(), ProfilerSession->GetCreationTime() );

	switch( GetSampleType() )
	{
	case EProfilerSampleTypes::Memory:
		{
			// By default we show memory data as KBs.
			Scale = 1.0f / 1024.0f;
			break;
		}

	default:
		{
			Scale = 1.0f;
		}
	}
}

const TGraphDataType FGraphDataSource::GetUncachedValueFromIndex( const uint32 Index ) const
{
	check( Index < ProfilerSession->GetDataProvider()->GetNumFrames() );
	double Result = 0.0;

	const IDataProviderRef DataProvider = ProfilerSession->GetDataProvider();

	const FIntPoint& IndicesForFrame = DataProvider->GetSamplesIndicesForFrame( Index );
	const uint32 SampleStartIndex = IndicesForFrame.X;
	const uint32 SampleEndIndex = IndicesForFrame.Y;

	const FProfilerSampleArray& Collection = DataProvider->GetCollection();

	for( uint32 SampleIndex = SampleStartIndex; SampleIndex < SampleEndIndex; SampleIndex++ )
	{
		const FProfilerSample& ProfilerSample = Collection[ SampleIndex ];
		const bool bValidStat = ProfilerSample.StatID() == GetStatID();

		if( bValidStat )
		{
			if( GetSampleType() == EProfilerSampleTypes::HierarchicalTime )
			{
				Result += ProfilerSample.DurationMS() * Scale;
			}
			else 
			{
				Result += ProfilerSample.CounterAsFloat() * Scale;
			}
		}
	}	

	return (TGraphDataType)Result;
}


const TGraphDataType FGraphDataSource::GetUncachedValueFromTimeRange( const float StartTimeMS, const float EndTimeMS ) const
{
	const IDataProviderRef DataProvider = ProfilerSession->GetDataProvider();
	const FIntPoint IndicesForFrame = DataProvider->GetClosestSamplesIndicesForTime( StartTimeMS, EndTimeMS );

	const uint32 StartFrameIndex = IndicesForFrame.X;
	const uint32 EndFrameIndex = IndicesForFrame.Y;

	TGraphDataType ResultMax = (TGraphDataType)MIN_int32;

	// Iterate through all frames and calculate the maximum value.
	for( uint32 FrameIndex = StartFrameIndex; FrameIndex < EndFrameIndex; ++FrameIndex )
	{
		const TGraphDataType DataSourceValue = const_cast<FGraphDataSource*>(this)->GetValueFromIndex( FrameIndex );
		ResultMax = FMath::Max( ResultMax, DataSourceValue );
	}

	return (TGraphDataType)ResultMax;
}

const uint32 FGraphDataSource::GetNumFrames() const
{
	return ProfilerSession->GetDataProvider()->GetNumFrames();
}

const float FGraphDataSource::GetTotalTimeMS() const
{
	return ProfilerSession->GetDataProvider()->GetTotalTimeMS();
}

const IDataProviderRef FGraphDataSource::GetDataProvider() const
{
	return ProfilerSession->GetDataProvider();
}

const FProfilerAggregatedStat* FGraphDataSource::GetAggregatedStat() const
{
	return ProfilerSession->GetAggregatedStat( StatID );
}

const FGuid FGraphDataSource::GetSessionInstanceID() const
{
	return ProfilerSession->GetInstanceID();
}

/*-----------------------------------------------------------------------------
	FCombinedGraphDataSource
-----------------------------------------------------------------------------*/


FCombinedGraphDataSource::FCombinedGraphDataSource( const uint32 InStatID, const FTimeAccuracy::Type InTimeAccuracy ) 
	: FGraphDataSourceDescription( InStatID )
	, ThisCachedDataByTime( InTimeAccuracy )
{
}

const FVector FCombinedGraphDataSource::GetUncachedValueFromTimeRange( const float StartTimeMS, const float EndTimeMS ) const
{
	// X=Min, Y=Max, Z=Avg
	FVector AggregatedValue( (TGraphDataType)MAX_int32, (TGraphDataType)MIN_int32, 0.0f );

	const uint32 NumSources = GraphDataSources.Num();
	const float InvNumSources = 1.0f / (float)NumSources;

	for( auto It = GetSourcesIterator(); It; ++It )
	{
		const FGraphDataSourceRefConst& GraphDataSource = It.Value();
		const float DataSourceValue = GraphDataSource->GetValueFromTimeRange( StartTimeMS, EndTimeMS );

		AggregatedValue.X = FMath::Min( AggregatedValue.X, DataSourceValue );
		AggregatedValue.Y = FMath::Max( AggregatedValue.Y, DataSourceValue );
		AggregatedValue.Z += DataSourceValue;
	}
	AggregatedValue.Z *= InvNumSources;

	return AggregatedValue;
}

void FCombinedGraphDataSource::GetStartIndicesFromTimeRange( const float StartTimeMS, const float EndTimeMS, TMap<FGuid,uint32>& out_StartIndices ) const
{
	for( auto It = GetSourcesIterator(); It; ++It )
	{
		const FGraphDataSourceRefConst& GraphDataSource = It.Value();
		const FIntPoint IndicesForFrame = GraphDataSource->GetDataProvider()->GetClosestSamplesIndicesForTime( StartTimeMS, EndTimeMS );

		const uint32 StartFrameIndex = IndicesForFrame.X;
		const uint32 EndFrameIndex = IndicesForFrame.Y;

		uint32 FrameIndex = 0;
		float MaxFrameTime = 0.0f;

		// Iterate through all frames and find the highest frame time.
		for( uint32 InnerFrameIndex = StartFrameIndex; InnerFrameIndex < EndFrameIndex; ++InnerFrameIndex )
		{
			const float InnerFrameTime = GraphDataSource->GetDataProvider()->GetFrameTimeMS( InnerFrameIndex );
			if( InnerFrameTime > MaxFrameTime )
			{
				MaxFrameTime = InnerFrameTime;
				FrameIndex = InnerFrameIndex;
			}
		}

		if( MaxFrameTime > 0.0f )
		{
			out_StartIndices.Add( GraphDataSource->GetSessionInstanceID(), FrameIndex );
		}
	}
}

/*-----------------------------------------------------------------------------
	FEventGraphData
-----------------------------------------------------------------------------*/

//namespace NEventGraphSample {

FName FEventGraphConsts::RootEvent = TEXT("RootEvent");
FName FEventGraphConsts::Self = TEXT("Self");
FName FEventGraphConsts::FakeRoot = TEXT("FakeRoot");

/*-----------------------------------------------------------------------------
	FEventGraphSample
-----------------------------------------------------------------------------*/

FEventProperty FEventGraphSample::Properties[ EEventPropertyIndex::InvalidOrMax ] =
{
	// Properties
	FEventProperty(EEventPropertyIndex::StatName,TEXT("StatName"),STRUCT_OFFSET(FEventGraphSample,_StatName),EEventPropertyFormatters::Name),
	FEventProperty(EEventPropertyIndex::InclusiveTimeMS,TEXT("InclusiveTimeMS"),STRUCT_OFFSET(FEventGraphSample,_InclusiveTimeMS),EEventPropertyFormatters::TimeMS),
	FEventProperty(EEventPropertyIndex::InclusiveTimePct,TEXT("InclusiveTimePct"),STRUCT_OFFSET(FEventGraphSample,_InclusiveTimePct),EEventPropertyFormatters::TimePct),
	FEventProperty(EEventPropertyIndex::MinInclusiveTimeMS,TEXT("MinInclusiveTimeMS"),STRUCT_OFFSET(FEventGraphSample,_MinInclusiveTimeMS),EEventPropertyFormatters::TimeMS),
	FEventProperty(EEventPropertyIndex::MaxInclusiveTimeMS,TEXT("MaxInclusiveTimeMS"),STRUCT_OFFSET(FEventGraphSample,_MaxInclusiveTimeMS),EEventPropertyFormatters::TimeMS),
	FEventProperty(EEventPropertyIndex::AvgInclusiveTimeMS,TEXT("AvgInclusiveTimeMS"),STRUCT_OFFSET(FEventGraphSample,_AvgInclusiveTimeMS),EEventPropertyFormatters::TimeMS),
	FEventProperty(EEventPropertyIndex::ExclusiveTimeMS,TEXT("ExclusiveTimeMS"),STRUCT_OFFSET(FEventGraphSample,_ExclusiveTimeMS),EEventPropertyFormatters::TimeMS),
	FEventProperty(EEventPropertyIndex::ExclusiveTimePct,TEXT("ExclusiveTimePct"),STRUCT_OFFSET(FEventGraphSample,_ExclusiveTimePct),EEventPropertyFormatters::TimePct),
	FEventProperty(EEventPropertyIndex::AvgInclusiveTimePerCallMS,TEXT("AvgInclusiveTimePerCallMS"),STRUCT_OFFSET(FEventGraphSample,_AvgInclusiveTimePerCallMS),EEventPropertyFormatters::TimeMS),
	FEventProperty(EEventPropertyIndex::NumCallsPerFrame,TEXT("NumCallsPerFrame"),STRUCT_OFFSET(FEventGraphSample,_NumCallsPerFrame),EEventPropertyFormatters::Number),
	FEventProperty(EEventPropertyIndex::AvgNumCallsPerFrame,TEXT("AvgNumCallsPerFrame"),STRUCT_OFFSET(FEventGraphSample,_AvgNumCallsPerFrame),EEventPropertyFormatters::Number),
	FEventProperty(EEventPropertyIndex::ThreadName,TEXT("ThreadName"),STRUCT_OFFSET(FEventGraphSample,_ThreadName),EEventPropertyFormatters::Name),
	FEventProperty(EEventPropertyIndex::ThreadDurationMS,TEXT("ThreadDurationMS"),STRUCT_OFFSET(FEventGraphSample,_ThreadDurationMS),EEventPropertyFormatters::TimeMS),
	FEventProperty(EEventPropertyIndex::FrameDurationMS,TEXT("FrameDurationMS"),STRUCT_OFFSET(FEventGraphSample,_FrameDurationMS),EEventPropertyFormatters::TimeMS),
	FEventProperty(EEventPropertyIndex::ThreadPct,TEXT("ThreadPct"),STRUCT_OFFSET(FEventGraphSample,_ThreadPct),EEventPropertyFormatters::TimePct),
	FEventProperty(EEventPropertyIndex::FramePct,TEXT("FramePct"),STRUCT_OFFSET(FEventGraphSample,_FramePct),EEventPropertyFormatters::TimePct),
	FEventProperty(EEventPropertyIndex::ThreadToFramePct,TEXT("ThreadToFramePct"),STRUCT_OFFSET(FEventGraphSample,_ThreadToFramePct),EEventPropertyFormatters::TimePct),
	FEventProperty(EEventPropertyIndex::StartTimeMS,TEXT("StartTimeMS"),STRUCT_OFFSET(FEventGraphSample,_StartTimeMS),EEventPropertyFormatters::TimeMS),
	FEventProperty(EEventPropertyIndex::GroupName,TEXT("GroupName"),STRUCT_OFFSET(FEventGraphSample,_GroupName),EEventPropertyFormatters::Name),

	// Special none property
	FEventProperty(EEventPropertyIndex::None,NAME_None,INDEX_NONE,EEventPropertyFormatters::Name),

	// Booleans
	FEventProperty(EEventPropertyIndex::bIsHotPath,TEXT("bIsHotPath"),STRUCT_OFFSET(FEventGraphSample,_bIsHotPath),EEventPropertyFormatters::Bool),
	FEventProperty(EEventPropertyIndex::bIsFiltered,TEXT("bIsFiltered"),STRUCT_OFFSET(FEventGraphSample,_bIsFiltered),EEventPropertyFormatters::Bool),
	FEventProperty(EEventPropertyIndex::bIsCulled,TEXT("bIsCulled"),STRUCT_OFFSET(FEventGraphSample,_bIsCulled),EEventPropertyFormatters::Bool),

	// Booleans internal
	FEventProperty(EEventPropertyIndex::bNeedNotCulledChildrenUpdate,TEXT("bNeedNotCulledChildrenUpdate"),STRUCT_OFFSET(FEventGraphSample,bNeedNotCulledChildrenUpdate),EEventPropertyFormatters::Bool),
};

TMap<FName,const FEventProperty*> FEventGraphSample::NamedProperties;

void FEventGraphSample::InitializePropertyManagement()
{
	static bool bInitialized = false;
	if( !bInitialized )
	{
		NamedProperties = TMapBuilder<FName,const FEventProperty*>()
			// Properties
			.Add( TEXT("StatName"), &Properties[EEventPropertyIndex::StatName] )
			.Add( TEXT("InclusiveTimeMS"), &Properties[EEventPropertyIndex::InclusiveTimeMS] )
			.Add( TEXT("InclusiveTimePct"), &Properties[EEventPropertyIndex::InclusiveTimePct] )
			.Add( TEXT("MinInclusiveTimeMS"), &Properties[EEventPropertyIndex::MinInclusiveTimeMS] )
			.Add( TEXT("MaxInclusiveTimeMS"), &Properties[EEventPropertyIndex::MaxInclusiveTimeMS] )
			.Add( TEXT("AvgInclusiveTimeMS"), &Properties[EEventPropertyIndex::AvgInclusiveTimeMS] )
			.Add( TEXT("ExclusiveTimeMS"), &Properties[EEventPropertyIndex::ExclusiveTimeMS] )
			.Add( TEXT("ExclusiveTimePct"), &Properties[EEventPropertyIndex::ExclusiveTimePct] )
			.Add( TEXT("AvgInclusiveTimePerCallMS"), &Properties[EEventPropertyIndex::AvgInclusiveTimePerCallMS] )
			.Add( TEXT("NumCallsPerFrame"), &Properties[EEventPropertyIndex::NumCallsPerFrame] )
			.Add( TEXT("AvgNumCallsPerFrame"), &Properties[EEventPropertyIndex::AvgNumCallsPerFrame] )
			.Add( TEXT("ThreadName"), &Properties[EEventPropertyIndex::ThreadName] )
			.Add( TEXT("ThreadDurationMS"), &Properties[EEventPropertyIndex::ThreadDurationMS] )
			.Add( TEXT("FrameDurationMS"), &Properties[EEventPropertyIndex::FrameDurationMS] )
			.Add( TEXT("ThreadPct"), &Properties[EEventPropertyIndex::ThreadPct] )
			.Add( TEXT("FramePct"), &Properties[EEventPropertyIndex::FramePct] )
			.Add( TEXT("ThreadToFramePct"), &Properties[EEventPropertyIndex::ThreadToFramePct] )
			.Add( TEXT("StartTimeMS"), &Properties[EEventPropertyIndex::StartTimeMS] )
			.Add( TEXT("GroupName"), &Properties[EEventPropertyIndex::GroupName] )

			// Special none property
			.Add( NAME_None, &Properties[EEventPropertyIndex::None] )

			// Booleans
			.Add( TEXT("bIsHotPath"), &Properties[EEventPropertyIndex::bIsHotPath] )
			.Add( TEXT("bIsFiltered"), &Properties[EEventPropertyIndex::bIsFiltered] )
			.Add( TEXT("bIsCulled"), &Properties[EEventPropertyIndex::bIsCulled] )

			// Booleans internal
			.Add( TEXT("bNeedNotCulledChildrenUpdate"), &Properties[EEventPropertyIndex::bNeedNotCulledChildrenUpdate] )
			;
	
		// Make sure that the minimal property manager has been initialized.
		bInitialized = true;

		// Sanity checks.
		check( NamedProperties.FindChecked( NAME_None )->Name == NAME_None );
		check( NamedProperties.FindChecked( NAME_None )->Offset == INDEX_NONE );

		check( FEventGraphSample::Properties[ EEventPropertyIndex::None ].Name == NAME_None );
		check( FEventGraphSample::Properties[ EEventPropertyIndex::None ].Offset == INDEX_NONE );
	}
}

FEventGraphSamplePtr FEventGraphSample::FindChildPtr( const FEventGraphSamplePtr& OtherChild )
{
	FEventGraphSamplePtr FoundChildPtr;
	for( int32 ChildIndex = 0; ChildIndex < _ChildrenPtr.Num(); ++ChildIndex )
	{
		const FEventGraphSamplePtr& ThisChild = _ChildrenPtr[ChildIndex];

		const bool bTheSame = OtherChild->AreTheSamePtr( ThisChild );
		if( bTheSame )
		{
			FoundChildPtr = ThisChild;
			break;
		}
	}
	return FoundChildPtr;
}


void FEventGraphSample::CombineAndAddPtr_Recurrent( const FEventGraphSamplePtr& Other )
{
	AddSamplePtr( Other );

	// Check other children.
	for( int32 ChildIndex = 0; ChildIndex < Other->_ChildrenPtr.Num(); ++ChildIndex )
	{
		const FEventGraphSamplePtr& OtherChild = Other->_ChildrenPtr[ChildIndex];
		FEventGraphSamplePtr ThisChild = FindChildPtr( OtherChild );

		if( ThisChild.IsValid() )
		{
			ThisChild->CombineAndAddPtr_Recurrent( OtherChild );
		}
		else
		{
			AddChildAndSetParentPtr( OtherChild->DuplicateWithHierarchyPtr() );
		}
	}
}

void FEventGraphSample::CombineAndFindMaxPtr_Recurrent( const FEventGraphSamplePtr& Other )
{
#if	_DEBUG
	const FName Some0 = TEXT("FGCTask");
	const FName Some1 = TEXT("PoolThread [0x21e8]");

	if( Other->_StatName == Some0 || Other->_StatName == Some1 )
	{
		int k=0;k++;

		if( Other->_InclusiveTimeMS > 0.4f )
		{
			int j=0;j++;
		}
	}
#endif // _DEBUG

	MaxSamplePtr( Other );

	// Check other children.
	for( int32 ChildIndex = 0; ChildIndex < Other->_ChildrenPtr.Num(); ++ChildIndex )
	{
		const FEventGraphSamplePtr& OtherChild = Other->_ChildrenPtr[ChildIndex];
		FEventGraphSamplePtr ThisChild = FindChildPtr( OtherChild );

		if( ThisChild.IsValid() )
		{
			ThisChild->CombineAndFindMaxPtr_Recurrent( OtherChild );
		}
		else
		{
			AddChildAndSetParentPtr( OtherChild->DuplicateWithHierarchyPtr() );
		}
	}
}

void FEventGraphSample::Divide_Recurrent( const double Divisor )
{
	DivideSamplePtr( Divisor );

	// Check other children.
	for( int32 ChildIndex = 0; ChildIndex < _ChildrenPtr.Num(); ++ChildIndex )
	{
		FEventGraphSamplePtr ThisChild = _ChildrenPtr[ChildIndex];
		ThisChild->Divide_Recurrent( Divisor );
}
}

/*-----------------------------------------------------------------------------
	FEventGraphData
-----------------------------------------------------------------------------*/

FEventGraphData::FEventGraphData() 
	: RootEvent( FEventGraphSample::CreateNamedEvent( FEventGraphConsts::RootEvent ) )
	, FrameStartIndex( 0 )
	, FrameEndIndex( 0 )
{}

FEventGraphData::FEventGraphData( const FProfilerSessionRef& InProfilerSession, const uint32 InFrameIndex )
	: FrameStartIndex( InFrameIndex )
	, FrameEndIndex( InFrameIndex+1 )
	, ProfilerSessionPtr( InProfilerSession )
{
	static FTotalTimeAndCount Current(0.0f, 0);
	PROFILER_SCOPE_LOG_TIME( TEXT( "FEventGraphData::FEventGraphData" ), &Current );

	Description = FString::Printf( TEXT("%s: %i"), *InProfilerSession->GetShortName(), InFrameIndex );

	// @TODO: Duplicate is not needed, remove it later.
	const IDataProviderRef DataProvider = InProfilerSession->GetDataProvider()->Duplicate<FArrayDataProvider>( FrameStartIndex );

	const double FrameDurationMS = DataProvider->GetFrameTimeMS( 0 ); 
	const FProfilerSample& RootProfilerSample = DataProvider->GetCollection()[0];

	RootEvent = FEventGraphSample::CreateNamedEvent( FEventGraphConsts::RootEvent );

	PopulateHierarchy_Recurrent( InProfilerSession, RootEvent, RootProfilerSample, DataProvider );

	// Root sample contains FrameDurationMS
	RootEvent->_InclusiveTimeMS = RootProfilerSample.DurationMS();
	RootEvent->_InclusiveTimePct = 100.0f;

	// Set root and thread event.
	RootEvent->SetRootAndThreadForAllChildren();
	// Fix all children. 
	RootEvent->ExecuteOperationForAllChildren( FEventGraphSample::FFixChildrenTimes() );
}

void FEventGraphData::PopulateHierarchy_Recurrent
( 
	const FProfilerSessionRef& ProfilerSession,
	const FEventGraphSamplePtr ParentEvent, 
	const FProfilerSample& ParentSample, 
	const IDataProviderRef DataProvider
)
{
	const FProfilerStatMetaDataRef MetaData = ProfilerSession->GetMetaData();

	for( int32 ChildIndex = 0; ChildIndex < ParentSample.ChildrenIndices().Num(); ChildIndex++ )
	{
		const FProfilerSample& ChildSample = DataProvider->GetCollection()[ ParentSample.ChildrenIndices()[ChildIndex] ];

		const FProfilerStat& ProfilerThread = MetaData->GetStatByID( ChildSample.ThreadID() );
		const FName& ThreadName = ProfilerThread.Name();

		const FProfilerStat& ProfilerStat = MetaData->GetStatByID( ChildSample.StatID() );
		const FName& StatName = ProfilerStat.Name();
		const FName& GroupName = ProfilerStat.OwningGroup().Name();

		FEventGraphSample* ChildEvent = new FEventGraphSample
		(
			ThreadName,	GroupName, ChildSample.StatID(), StatName, 
			ChildSample.StartMS(), ChildSample.DurationMS(),
			0.0f, 0.0f, 0.0f, 0.00f, ChildSample.CounterAsFloat(), 0.0f,
			ParentEvent
		);

		FEventGraphSamplePtr ChildEventPtr = MakeShareable( ChildEvent );
		ParentEvent->AddChildPtr( ChildEventPtr );

		PopulateHierarchy_Recurrent( ProfilerSession, ChildEventPtr, ChildSample, DataProvider );	
	}
}

FEventGraphData::FEventGraphData( const FEventGraphData& Source )
{
	RootEvent = Source.GetRoot()->DuplicateWithHierarchyPtr();
	RootEvent->SetRootAndThreadForAllChildren();
	Advance( Source.FrameStartIndex, Source.FrameEndIndex );
	}

FEventGraphDataRef FEventGraphData::DuplicateAsRef()
{
	FEventGraphDataRef EventGraphRef = MakeShareable( new FEventGraphData(*this) );
	return EventGraphRef;
}

void FEventGraphData::CombineAndAdd( const FEventGraphData& Other )
{
	RootEvent->CombineAndAddPtr_Recurrent( Other.GetRoot() );
	Description = FString::Printf( TEXT("CombineAndAdd: %i"), GetNumFrames() );
}

void FEventGraphData::CombineAndFindMax( const FEventGraphData& Other )
{
	RootEvent->CombineAndFindMaxPtr_Recurrent( Other.GetRoot() );
	Description = FString::Printf( TEXT("CombineAndFindMax: %i"), GetNumFrames() );
}

void FEventGraphData::Divide( const int32 InNumFrames )
{
	const double NumFrames = (double)InNumFrames;
	RootEvent->ExecuteOperationForAllChildren( FEventGraphSample::FDivide(), NumFrames );
	Description = FString::Printf( TEXT("Divide: %i"), GetNumFrames() );
}

void FEventGraphData::Advance( const uint32 InFrameStartIndex, const uint32 InFrameEndIndex )
{
	// Set root and thread event.
	RootEvent->SetRootAndThreadForAllChildren();
	// Fix all children. 
	RootEvent->ExecuteOperationForAllChildren( FEventGraphSample::FFixChildrenTimes() );

	FrameStartIndex = InFrameStartIndex;
	FrameEndIndex = InFrameEndIndex;
}

//}//namespace NEventGraphSample

FString EEventGraphTypes::ToName( const EEventGraphTypes::Type EventGraphType )
{
	switch( EventGraphType )
	{
		case Average: return LOCTEXT("EventGraphType_Name_Average", "Average").ToString();
		case Maximum: return LOCTEXT("EventGraphType_Name_Maximum", "Maximum").ToString();
		case OneFrame: return LOCTEXT("EventGraphType_Name_OneFrame", "OneFrame").ToString();

		default: return LOCTEXT("InvalidOrMax", "InvalidOrMax").ToString();
	}
}

FString EEventGraphTypes::ToDescription( const EEventGraphTypes::Type EventGraphType )
{
	switch( EventGraphType )
{
		case Average: return LOCTEXT("EventGraphType_Desc_Average", "Per-frame average event graph").ToString();
		case Maximum: return LOCTEXT("EventGraphType_Desc_Maximum", "Highest \"per-frame\" event graph").ToString();
		case OneFrame: return LOCTEXT("EventGraphType_Desc_OneFrame", "Event graph for one frame").ToString();

		default: return LOCTEXT("InvalidOrMax", "InvalidOrMax").ToString();
	}
}

#undef LOCTEXT_NAMESPACE