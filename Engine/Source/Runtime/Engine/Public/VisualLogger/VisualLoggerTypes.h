// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "EngineDefines.h"

enum class ECreateIfNeeded : int8
{
	Invalid = -1,
	DontCreate = 0,
	Create = 1,
};

// flags describing VisualLogger device's features
namespace EVisualLoggerDeviceFlags
{
	enum Type
	{
		NoFlags = 0,
		CanSaveToFile = 1,
		StoreLogsLocally = 2,
	};
}

// version for vlog binary file format
namespace EVisualLoggerVersion
{
	enum Type
	{
		Initial = 0,
		HistogramGraphsSerialization = 1,
		AddedOwnerClassName = 2,
		StatusCategoryWithChildren = 3,
		TransformationForShapes = 4,
		// -----<new versions can be added before this line>-------------------------------------------------
		// - this needs to be the last line (see note below)
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	// The GUID for this custom version number
	extern ENGINE_API const FGuid GUID;
}

//types of shape elements
enum class EVisualLoggerShapeElement : uint8
{
	Invalid = 0,
	SinglePoint, // individual points. 
	Segment, // pairs of points 
	Path,	// sequence of point
	Box,
	Cone,
	Cylinder,
	Capsule,
	Polygon,
	// note that in order to remain backward compatibility in terms of log
	// serialization new enum values need to be added at the end
};

#if ENABLE_VISUAL_LOG
struct ENGINE_API FVisualLogEventBase
{
	const FString Name;
	const FString FriendlyDesc;
	const ELogVerbosity::Type Verbosity;

	FVisualLogEventBase(const FString& InName, const FString& InFriendlyDesc, ELogVerbosity::Type InVerbosity)
		: Name(InName), FriendlyDesc(InFriendlyDesc), Verbosity(InVerbosity)
	{
	}
};

struct ENGINE_API FVisualLogEvent
{
	FString Name;
	FString UserFriendlyDesc;
	TEnumAsByte<ELogVerbosity::Type> Verbosity;
	TMap<FName, int32>	 EventTags;
	int32 Counter;
	int64 UserData;
	FName TagName;

	FVisualLogEvent() : Counter(1) { /* Empty */ }
	FVisualLogEvent(const FVisualLogEventBase& Event);
	FVisualLogEvent& operator=(const FVisualLogEventBase& Event);
};

struct ENGINE_API FVisualLogLine
{
	FString Line;
	FName Category;
	TEnumAsByte<ELogVerbosity::Type> Verbosity;
	int32 UniqueId;
	int64 UserData;
	FName TagName;

	FVisualLogLine() { /* Empty */ }
	FVisualLogLine(const FName& InCategory, ELogVerbosity::Type InVerbosity, const FString& InLine);
	FVisualLogLine(const FName& InCategory, ELogVerbosity::Type InVerbosity, const FString& InLine, int64 InUserData);
};

struct ENGINE_API FVisualLogStatusCategory
{
	TArray<FString> Data;
	FString Category;
	int32 UniqueId;
	TArray<FVisualLogStatusCategory> Children;

	void Add(const FString& Key, const FString& Value);
	bool GetDesc(int32 Index, FString& Key, FString& Value) const;
	void AddChild(const FVisualLogStatusCategory& Child);
};

struct ENGINE_API FVisualLogShapeElement
{
	FString Description;
	FName Category;
	TEnumAsByte<ELogVerbosity::Type> Verbosity;
	TArray<FVector> Points;
	FMatrix TransformationMatrix;
	int32 UniqueId;
	EVisualLoggerShapeElement Type;
	uint8 Color;
	union
	{
		uint16 Thicknes;
		uint16 Radius;
	};

	FVisualLogShapeElement(EVisualLoggerShapeElement InType = EVisualLoggerShapeElement::Invalid);
	FVisualLogShapeElement(const FString& InDescription, const FColor& InColor, uint16 InThickness, const FName& InCategory);
	void SetColor(const FColor& InColor);
	EVisualLoggerShapeElement GetType() const;
	void SetType(EVisualLoggerShapeElement InType);
	FColor GetFColor() const;
};

struct ENGINE_API FVisualLogHistogramSample
{
	FName Category;
	TEnumAsByte<ELogVerbosity::Type> Verbosity;
	FName GraphName;
	FName DataName;
	FVector2D SampleValue;
	int32 UniqueId;
};

struct ENGINE_API FVisualLogDataBlock
{
	FName TagName;
	FName Category;
	TEnumAsByte<ELogVerbosity::Type> Verbosity;
	TArray<uint8>	Data;
	int32 UniqueId;
};
#endif  //ENABLE_VISUAL_LOG

struct ENGINE_API FVisualLogEntry
{
#if ENABLE_VISUAL_LOG
	float TimeStamp;
	FVector Location;

	TArray<FVisualLogEvent> Events;
	TArray<FVisualLogLine> LogLines;
	TArray<FVisualLogStatusCategory> Status;
	TArray<FVisualLogShapeElement> ElementsToDraw;
	TArray<FVisualLogHistogramSample>	HistogramSamples;
	TArray<FVisualLogDataBlock>	DataBlocks;

	FVisualLogEntry() { Reset(); }
	FVisualLogEntry(const FVisualLogEntry& Entry);
	FVisualLogEntry(const class AActor* InActor, TArray<TWeakObjectPtr<UObject> >* Children);
	FVisualLogEntry(float InTimeStamp, FVector InLocation, const UObject* Object, TArray<TWeakObjectPtr<UObject> >* Children);

	void Reset();

	void AddText(const FString& TextLine, const FName& CategoryName, ELogVerbosity::Type Verbosity);
	// path
	void AddElement(const TArray<FVector>& Points, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color = FColor::White, const FString& Description = TEXT(""), uint16 Thickness = 0);
	// location
	void AddElement(const FVector& Point, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color = FColor::White, const FString& Description = TEXT(""), uint16 Thickness = 0);
	// segment
	void AddElement(const FVector& Start, const FVector& End, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color = FColor::White, const FString& Description = TEXT(""), uint16 Thickness = 0);
	// box
	void AddElement(const FBox& Box, const FMatrix& Matrix, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color = FColor::White, const FString& Description = TEXT(""), uint16 Thickness = 0);
	// Cone
	void AddElement(const FVector& Orgin, const FVector& Direction, float Length, float AngleWidth, float AngleHeight, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color = FColor::White, const FString& Description = TEXT(""), uint16 Thickness = 0);
	// Cylinder
	void AddElement(const FVector& Start, const FVector& End, float Radius, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color = FColor::White, const FString& Description = TEXT(""), uint16 Thickness = 0);
	// capsule
	void AddElement(const FVector& Center, float HalfHeight, float Radius, const FQuat & Rotation, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color = FColor::White, const FString& Description = TEXT(""));
	// custom element
	void AddElement(const FVisualLogShapeElement& Element);
	// histogram sample
	void AddHistogramData(const FVector2D& DataSample, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FName& GraphName, const FName& DataName);
	// Custom data block
	FVisualLogDataBlock& AddDataBlock(const FString& TagName, const TArray<uint8>& BlobDataArray, const FName& CategoryName, ELogVerbosity::Type Verbosity);
	// Event
	int32 AddEvent(const FVisualLogEventBase& Event);
	// find index of status category
	int32 FindStatusIndex(const FString& CategoryName);


#endif // ENABLE_VISUAL_LOG
};

#if  ENABLE_VISUAL_LOG

class FVisualLogExtensionInterface
{
public:
	virtual void OnTimestampChange(float Timestamp, class UWorld* InWorld, class AActor* HelperActor) = 0;
	virtual void DrawData(class UWorld* InWorld, class UCanvas* Canvas, class AActor* HelperActor, const FName& TagName, const FVisualLogDataBlock& DataBlock, float Timestamp) = 0;
	virtual void DisableDrawingForData(class UWorld* InWorld, class UCanvas* Canvas, class AActor* HelperActor, const FName& TagName, const FVisualLogDataBlock& DataBlock, float Timestamp) = 0;
	virtual void LogEntryLineSelectionChanged(TSharedPtr<struct FLogEntryItem> SelectedItem, int64 UserData, FName TagName) = 0;
};

/**
 * Interface for Visual Logger Device
 */
class ENGINE_API FVisualLogDevice
{
public:
	struct FVisualLogEntryItem
	{
		FVisualLogEntryItem() {}
		FVisualLogEntryItem(FName InOwnerName, FName InOwnerClassName, const FVisualLogEntry& LogEntry) : OwnerName(InOwnerName), OwnerClassName(InOwnerClassName), Entry(LogEntry) { }

		FName OwnerName;
		FName OwnerClassName;
		FVisualLogEntry Entry;
	};


	virtual void Serialize(const class UObject* LogOwner, FName OwnerName, FName InOwnerClassName, const FVisualLogEntry& LogEntry) = 0;
	virtual void Cleanup(bool bReleaseMemory = false) { /* Empty */ }
	virtual void StartRecordingToFile(float TImeStamp) { /* Empty */ }
	virtual void StopRecordingToFile(float TImeStamp) { /* Empty */ }
	virtual void SetFileName(const FString& InFileName) { /* Empty */ }
	virtual void GetRecordedLogs(TArray<FVisualLogDevice::FVisualLogEntryItem>& OutLogs)  const { /* Empty */ }
	virtual bool HasFlags(int32 InFlags) const { return !!(InFlags & EVisualLoggerDeviceFlags::NoFlags); }
};

struct ENGINE_API FVisualLoggerCategoryVerbosityPair
{
	FVisualLoggerCategoryVerbosityPair(FName Category, ELogVerbosity::Type InVerbosity) : CategoryName(Category), Verbosity(InVerbosity) {}

	FName CategoryName;
	ELogVerbosity::Type Verbosity;
};

struct ENGINE_API FVisualLoggerHelpers
{
	static FString GenerateTemporaryFilename(const FString& FileExt);
	static FString GenerateFilename(const FString& TempFileName, const FString& Prefix, float StartRecordingTime, float EndTimeStamp);
	static FArchive& Serialize(FArchive& Ar, FName& Name);
	static FArchive& Serialize(FArchive& Ar, TArray<FVisualLogDevice::FVisualLogEntryItem>& RecordedLogs);
	static void GetCategories(const FVisualLogEntry& RecordedLogs, TArray<FVisualLoggerCategoryVerbosityPair>& OutCategories);
	static void GetHistogramCategories(const FVisualLogEntry& RecordedLogs, TMap<FString, TArray<FString> >& OutCategories);
};

ENGINE_API  FArchive& operator<<(FArchive& Ar, FVisualLogDevice::FVisualLogEntryItem& FrameCacheItem);
ENGINE_API  FArchive& operator<<(FArchive& Ar, FVisualLogDataBlock& Data);
ENGINE_API  FArchive& operator<<(FArchive& Ar, FVisualLogHistogramSample& Sample);
ENGINE_API  FArchive& operator<<(FArchive& Ar, FVisualLogShapeElement& Element);
ENGINE_API  FArchive& operator<<(FArchive& Ar, FVisualLogEvent& Event);
ENGINE_API  FArchive& operator<<(FArchive& Ar, FVisualLogLine& LogLine);
ENGINE_API  FArchive& operator<<(FArchive& Ar, FVisualLogStatusCategory& Status);
ENGINE_API  FArchive& operator<<(FArchive& Ar, FVisualLogEntry& LogEntry);

inline
bool operator==(const FVisualLogEvent& Left, const FVisualLogEvent& Right) 
{ 
	return Left.Name == Right.Name; 
}

inline
FVisualLogEvent::FVisualLogEvent(const FVisualLogEventBase& Event)
: Counter(1)
{
	Name = Event.Name;
	UserFriendlyDesc = Event.FriendlyDesc;
	Verbosity = Event.Verbosity;
}

inline
FVisualLogEvent& FVisualLogEvent::operator= (const FVisualLogEventBase& Event)
{
	Name = Event.Name;
	UserFriendlyDesc = Event.FriendlyDesc;
	Verbosity = Event.Verbosity;
	return *this;
}

inline
FVisualLogLine::FVisualLogLine(const FName& InCategory, ELogVerbosity::Type InVerbosity, const FString& InLine)
: Line(InLine)
, Category(InCategory)
, Verbosity(InVerbosity)
, UserData(0)
{

}

inline
FVisualLogLine::FVisualLogLine(const FName& InCategory, ELogVerbosity::Type InVerbosity, const FString& InLine, int64 InUserData)
: Line(InLine)
, Category(InCategory)
, Verbosity(InVerbosity)
, UserData(InUserData)
{

}

inline
void FVisualLogStatusCategory::Add(const FString& Key, const FString& Value)
{
	Data.Add(FString(Key).AppendChar(TEXT('|')) + Value);
}

inline
void FVisualLogStatusCategory::AddChild(const FVisualLogStatusCategory& Child)
{
	Children.Add(Child);
}

inline
FVisualLogShapeElement::FVisualLogShapeElement(EVisualLoggerShapeElement InType)
: Verbosity(ELogVerbosity::All)
, TransformationMatrix(FMatrix::Identity)
, Type(InType)
, Color(0xff)
, Thicknes(0)
{

}

inline
FVisualLogShapeElement::FVisualLogShapeElement(const FString& InDescription, const FColor& InColor, uint16 InThickness, const FName& InCategory)
: Category(InCategory)
, Verbosity(ELogVerbosity::All)
, TransformationMatrix(FMatrix::Identity)
, Type(EVisualLoggerShapeElement::Invalid)
, Thicknes(InThickness)
{
	if (InDescription.IsEmpty() == false)
	{
		Description = InDescription;
	}
	SetColor(InColor);
}

inline
void FVisualLogShapeElement::SetColor(const FColor& InColor)
{
	Color = ((InColor.DWColor() >> 30) << 6)	| (((InColor.DWColor() & 0x00ff0000) >> 22) << 4)	| (((InColor.DWColor() & 0x0000ff00) >> 14) << 2)	| ((InColor.DWColor() & 0x000000ff) >> 6);
}

inline
EVisualLoggerShapeElement FVisualLogShapeElement::GetType() const
{
	return Type;
}

inline
void FVisualLogShapeElement::SetType(EVisualLoggerShapeElement InType)
{
	Type = InType;
}

inline
FColor FVisualLogShapeElement::GetFColor() const
{
	return FColor(((Color & 0xc0) << 24) | ((Color & 0x30) << 18) | ((Color & 0x0c) << 12) | ((Color & 0x03) << 6));
}


inline
int32 FVisualLogEntry::FindStatusIndex(const FString& CategoryName)
{
	for (int32 TestCategoryIndex = 0; TestCategoryIndex < Status.Num(); TestCategoryIndex++)
	{
		if (Status[TestCategoryIndex].Category == CategoryName)
		{
			return TestCategoryIndex;
		}
	}

	return INDEX_NONE;
}

#endif // ENABLE_VISUAL_LOG
