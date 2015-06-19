// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "ObjectBase.h"
#include "VisualLogger/VisualLogger.h"

#if ENABLE_VISUAL_LOG

#define DEPRECATED_VISUAL_LOGGER_MAGIC_NUMBER 0xFAFAAFAF
#define VISUAL_LOGGER_MAGIC_NUMBER 0xAFAFFAFA

bool FVisualLogStatusCategory::GetDesc(int32 Index, FString& Key, FString& Value) const
{
	if (Data.IsValidIndex(Index))
	{
		int32 SplitIdx = INDEX_NONE;
		if (Data[Index].FindChar(TEXT('|'), SplitIdx))
		{
			Key = Data[Index].Left(SplitIdx);
			Value = Data[Index].Mid(SplitIdx + 1);
			return true;
		}
	}

	return false;
}

FVisualLogEntry::FVisualLogEntry(const FVisualLogEntry& Entry)
{
	TimeStamp = Entry.TimeStamp;
	Location = Entry.Location;

	Events = Entry.Events;
	LogLines = Entry.LogLines;
	Status = Entry.Status;
	ElementsToDraw = Entry.ElementsToDraw;
	HistogramSamples = Entry.HistogramSamples;
	DataBlocks = Entry.DataBlocks;
}

FVisualLogEntry::FVisualLogEntry(const class AActor* InActor, TArray<TWeakObjectPtr<UObject> >* Children)
{
	if (InActor && InActor->IsPendingKill() == false)
	{
		TimeStamp = InActor->GetWorld()->TimeSeconds;
		Location = InActor->GetActorLocation();
		InActor->GrabDebugSnapshot(this);
		if (Children != nullptr)
		{
			TWeakObjectPtr<UObject>* WeakActorPtr = Children->GetData();
			for (int32 Index = 0; Index < Children->Num(); ++Index, ++WeakActorPtr)
			{
				if (WeakActorPtr->IsValid())
				{
					const AActor* ChildActor = Cast<AActor>(WeakActorPtr->Get());
					if (ChildActor)
					{
						ChildActor->GrabDebugSnapshot(this);
					}
				}
			}
		}
	}
}

FVisualLogEntry::FVisualLogEntry(float InTimeStamp, FVector InLocation, const UObject* Object, TArray<TWeakObjectPtr<UObject> >* Children)
{
	TimeStamp = InTimeStamp;
	Location = InLocation;
	const AActor* AsActor = Cast<AActor>(Object);
	if (AsActor)
	{
		AsActor->GrabDebugSnapshot(this);
	}
	if (Children != nullptr)
	{
		TWeakObjectPtr<UObject>* WeakActorPtr = Children->GetData();
		for (int32 Index = 0; Index < Children->Num(); ++Index, ++WeakActorPtr)
		{
			if (WeakActorPtr->IsValid())
			{
				const AActor* ChildActor = Cast<AActor>(WeakActorPtr->Get());
				if (ChildActor)
				{
					ChildActor->GrabDebugSnapshot(this);
				}
			}
		}
	}
}

void FVisualLogEntry::Reset()
{
	TimeStamp = -1;
	Location = FVector::ZeroVector;
	Events.Reset();
	LogLines.Reset();
	Status.Reset();
	ElementsToDraw.Reset();
	HistogramSamples.Reset();
	DataBlocks.Reset();
}

int32 FVisualLogEntry::AddEvent(const FVisualLogEventBase& Event)
{
	return Events.Add(Event);
}

void FVisualLogEntry::AddText(const FString& TextLine, const FName& CategoryName, ELogVerbosity::Type Verbosity)
{
	LogLines.Add(FVisualLogLine(CategoryName, Verbosity, TextLine));
}

void FVisualLogEntry::AddElement(const FVisualLogShapeElement& Element)
{
	ElementsToDraw.Add(Element);
}

void FVisualLogEntry::AddElement(const TArray<FVector>& Points, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color, const FString& Description, uint16 Thickness)
{
	FVisualLogShapeElement Element(Description, Color, Thickness, CategoryName);
	Element.Points = Points;
	Element.Type = EVisualLoggerShapeElement::Path;
	Element.Verbosity = Verbosity;
	ElementsToDraw.Add(Element);
}

void FVisualLogEntry::AddElement(const FVector& Point, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color, const FString& Description, uint16 Thickness)
{
	FVisualLogShapeElement Element(Description, Color, Thickness, CategoryName);
	Element.Points.Add(Point);
	Element.Type = EVisualLoggerShapeElement::SinglePoint;
	Element.Verbosity = Verbosity;
	ElementsToDraw.Add(Element);
}

void FVisualLogEntry::AddElement(const FVector& Start, const FVector& End, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color, const FString& Description, uint16 Thickness)
{
	FVisualLogShapeElement Element(Description, Color, Thickness, CategoryName);
	Element.Points.Reserve(2);
	Element.Points.Add(Start);
	Element.Points.Add(End);
	Element.Type = EVisualLoggerShapeElement::Segment;
	Element.Verbosity = Verbosity;
	ElementsToDraw.Add(Element);
}

void FVisualLogEntry::AddElement(const FBox& Box, const FMatrix& Matrix, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color, const FString& Description, uint16 Thickness)
{
	FVisualLogShapeElement Element(Description, Color, Thickness, CategoryName);
	Element.Points.Reserve(2);
	Element.Points.Add(Box.Min);
	Element.Points.Add(Box.Max);
	Element.Type = EVisualLoggerShapeElement::Box;
	Element.Verbosity = Verbosity;
	Element.TransformationMatrix = Matrix;
	ElementsToDraw.Add(Element);
}

void FVisualLogEntry::AddElement(const FVector& Orgin, const FVector& Direction, float Length, float AngleWidth, float AngleHeight, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color, const FString& Description, uint16 Thickness)
{
	FVisualLogShapeElement Element(Description, Color, Thickness, CategoryName);
	Element.Points.Reserve(3);
	Element.Points.Add(Orgin);
	Element.Points.Add(Direction);
	Element.Points.Add(FVector(Length, AngleWidth, AngleHeight));
	Element.Type = EVisualLoggerShapeElement::Cone;
	Element.Verbosity = Verbosity;
	ElementsToDraw.Add(Element);
}

void FVisualLogEntry::AddElement(const FVector& Start, const FVector& End, float Radius, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color, const FString& Description, uint16 Thickness)
{
	FVisualLogShapeElement Element(Description, Color, Thickness, CategoryName);
	Element.Points.Reserve(3);
	Element.Points.Add(Start);
	Element.Points.Add(End);
	Element.Points.Add(FVector(Radius, Thickness, 0));
	Element.Type = EVisualLoggerShapeElement::Cylinder;
	Element.Verbosity = Verbosity;
	ElementsToDraw.Add(Element);
}

void FVisualLogEntry::AddElement(const FVector& Center, float HalfHeight, float Radius, const FQuat & Rotation, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color, const FString& Description)
{
	FVisualLogShapeElement Element(Description, Color, 0, CategoryName);
	Element.Points.Reserve(3);
	Element.Points.Add(Center);
	Element.Points.Add(FVector(HalfHeight, Radius, Rotation.X));
	Element.Points.Add(FVector(Rotation.Y, Rotation.Z, Rotation.W));
	Element.Type = EVisualLoggerShapeElement::Capsule;
	Element.Verbosity = Verbosity;
	ElementsToDraw.Add(Element);
}

void FVisualLogEntry::AddHistogramData(const FVector2D& DataSample, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FName& GraphName, const FName& DataName)
{
	FVisualLogHistogramSample Sample;
	Sample.Category = CategoryName;
	Sample.GraphName = GraphName;
	Sample.DataName = DataName;
	Sample.SampleValue = DataSample;
	Sample.Verbosity = Verbosity;

	HistogramSamples.Add(Sample);
}

FVisualLogDataBlock& FVisualLogEntry::AddDataBlock(const FString& TagName, const TArray<uint8>& BlobDataArray, const FName& CategoryName, ELogVerbosity::Type Verbosity)
{
	FVisualLogDataBlock DataBlock;
	DataBlock.Category = CategoryName;
	DataBlock.TagName = *TagName;
	DataBlock.Data = BlobDataArray;
	DataBlock.Verbosity = Verbosity;

	const int32 Index = DataBlocks.Add(DataBlock);
	return DataBlocks[Index];
}

FArchive& operator<<(FArchive& Ar, FVisualLogDataBlock& Data)
{
	FVisualLoggerHelpers::Serialize(Ar, Data.TagName);
	FVisualLoggerHelpers::Serialize(Ar, Data.Category);
	Ar << Data.Verbosity;
	Ar << Data.Data;
	Ar << Data.UniqueId;

	return Ar;
}

FArchive& operator<<(FArchive& Ar, FVisualLogHistogramSample& Sample)
{
	FVisualLoggerHelpers::Serialize(Ar, Sample.Category);
	FVisualLoggerHelpers::Serialize(Ar, Sample.GraphName);
	FVisualLoggerHelpers::Serialize(Ar, Sample.DataName);
	Ar << Sample.Verbosity;
	Ar << Sample.SampleValue;
	Ar << Sample.UniqueId;

	return Ar;
}

FArchive& operator<<(FArchive& Ar, EVisualLoggerShapeElement& Shape)
{
	uint8 ShapeAsInt = (uint8)Shape;
	Ar << ShapeAsInt;

	if (Ar.IsLoading())
	{
		Shape = (EVisualLoggerShapeElement)ShapeAsInt;
	}

	return Ar;
}

FArchive& operator<<(FArchive& Ar, FVisualLogShapeElement& Element)
{
	FVisualLoggerHelpers::Serialize(Ar, Element.Category);
	Ar << Element.Description;
	Ar << Element.Verbosity;
	const int32 VLogsVer = Ar.CustomVer(EVisualLoggerVersion::GUID);

	if (VLogsVer >= EVisualLoggerVersion::TransformationForShapes)
	{
		Ar << Element.TransformationMatrix;
	}

	Ar << Element.Points;
	Ar << Element.UniqueId;
	Ar << Element.Type;
	Ar << Element.Color;
	Ar << Element.Thicknes;

	return Ar;
}

FArchive& operator<<(FArchive& Ar, FVisualLogEvent& Event)
{
	Ar << Event.Name;
	Ar << Event.UserFriendlyDesc;
	Ar << Event.Verbosity;

	int32 NumberOfTags = Event.EventTags.Num();
	Ar << NumberOfTags;
	if (Ar.IsLoading())
	{
		for (int32 Index = 0; Index < NumberOfTags; ++Index)
		{
			FName Key = NAME_None;
			int32 Value = 0;
			FVisualLoggerHelpers::Serialize(Ar, Key);
			Ar << Value;
			Event.EventTags.Add(Key, Value);
		}
	}
	else
	{
		for (auto& CurrentTag : Event.EventTags)
		{
			FVisualLoggerHelpers::Serialize(Ar, CurrentTag.Key);
			Ar << CurrentTag.Value;
		}
	}

	Ar << Event.Counter;
	Ar << Event.UserData;
	FVisualLoggerHelpers::Serialize(Ar, Event.TagName);

	return Ar;
}

FArchive& operator<<(FArchive& Ar, FVisualLogLine& LogLine)
{
	FVisualLoggerHelpers::Serialize(Ar, LogLine.Category);
	FVisualLoggerHelpers::Serialize(Ar, LogLine.TagName);
	Ar << LogLine.Verbosity;
	Ar << LogLine.UniqueId;
	Ar << LogLine.UserData;
	Ar << LogLine.Line;
	return Ar;
}

FArchive& operator<<(FArchive& Ar, FVisualLogStatusCategory& Status)
{
	Ar << Status.Category;
	Ar << Status.Data;

	const int32 VLogsVer = Ar.CustomVer(EVisualLoggerVersion::GUID);
	if (VLogsVer >= EVisualLoggerVersion::StatusCategoryWithChildren)
	{
		int32 NumChildren = Status.Children.Num();
		Ar << NumChildren;
		if (Ar.IsLoading())
		{
			for (int32 Index = 0; Index < NumChildren; ++Index)
			{
				FVisualLogStatusCategory CurrentChild;
				Ar << CurrentChild;
				Status.Children.Add(CurrentChild);
			}
		}
		else
		{
			for (auto& CurrentChild : Status.Children)
			{
				Ar << CurrentChild;
			}
		}
	}
	return Ar;
}

FArchive& operator<<(FArchive& Ar, FVisualLogEntry& LogEntry)
{
	Ar << LogEntry.TimeStamp;
	Ar << LogEntry.Location;
	Ar << LogEntry.LogLines;
	Ar << LogEntry.Status;
	Ar << LogEntry.Events;
	Ar << LogEntry.ElementsToDraw;
	Ar << LogEntry.DataBlocks;

	const int32 VLogsVer = Ar.CustomVer(EVisualLoggerVersion::GUID);
	if (VLogsVer > EVisualLoggerVersion::Initial)
	{
		Ar << LogEntry.HistogramSamples;
	}

	return Ar;
}

FArchive& operator<<(FArchive& Ar, FVisualLogDevice::FVisualLogEntryItem& FrameCacheItem)
{
	FVisualLoggerHelpers::Serialize(Ar, FrameCacheItem.OwnerName);
	const int32 VLogsVer = Ar.CustomVer(EVisualLoggerVersion::GUID);
	if (VLogsVer >= EVisualLoggerVersion::AddedOwnerClassName)
	{
		FVisualLoggerHelpers::Serialize(Ar, FrameCacheItem.OwnerClassName);
	}
	Ar << FrameCacheItem.Entry;
	return Ar;
}

FString FVisualLoggerHelpers::GenerateTemporaryFilename(const FString& FileExt)
{
	return FString::Printf(TEXT("VTEMP_%s.%s"), *FDateTime::Now().ToString(), *FileExt);
}

FString FVisualLoggerHelpers::GenerateFilename(const FString& TempFileName, const FString& Prefix, float StartRecordingTime, float EndTimeStamp)
{
	const FString TempFullFilename = FString::Printf(TEXT("%slogs/%s"), *FPaths::GameSavedDir(), *TempFileName);
	const FString FullFilename = FString::Printf(TEXT("%slogs/%s_%s"), *FPaths::GameSavedDir(), *Prefix, *TempFileName);
	const FString TimeFrameString = FString::Printf(TEXT("%d-%d_"), FMath::TruncToInt(StartRecordingTime), FMath::TruncToInt(EndTimeStamp));
	return FullFilename.Replace(TEXT("VTEMP_"), *TimeFrameString, ESearchCase::CaseSensitive);
}

FArchive& FVisualLoggerHelpers::Serialize(FArchive& Ar, FName& Name)
{
	// Serialize the FName as a string
	if (Ar.IsLoading())
	{
		FString StringName;
		Ar << StringName;
		Name = FName(*StringName);
	}
	else
	{
		FString StringName = Name.ToString();
		Ar << StringName;
	}
	return Ar;
}

FArchive& FVisualLoggerHelpers::Serialize(FArchive& Ar, TArray<FVisualLogDevice::FVisualLogEntryItem>& RecordedLogs)
{
	Ar.UsingCustomVersion(EVisualLoggerVersion::GUID);

	if (Ar.IsLoading())
	{
		TArray<FVisualLogDevice::FVisualLogEntryItem> CurrentFrame;
		while (Ar.AtEnd() == false)
		{
			int32 FrameTag = VISUAL_LOGGER_MAGIC_NUMBER;
			Ar << FrameTag;
			if (FrameTag != DEPRECATED_VISUAL_LOGGER_MAGIC_NUMBER && FrameTag != VISUAL_LOGGER_MAGIC_NUMBER)
			{
				break;
			}

			if (FrameTag == DEPRECATED_VISUAL_LOGGER_MAGIC_NUMBER)
			{
				Ar.SetCustomVersion(EVisualLoggerVersion::GUID, EVisualLoggerVersion::Initial, TEXT("VisualLogger"));
			}
			else
			{
				int32 ArchiveVer = -1;
				Ar << ArchiveVer;
				check(ArchiveVer >= EVisualLoggerVersion::Initial);

				Ar.SetCustomVersion(EVisualLoggerVersion::GUID, ArchiveVer, TEXT("VisualLogger"));
			}

			Ar << CurrentFrame;
			RecordedLogs.Append(CurrentFrame);
			CurrentFrame.Reset();
		}
	}
	else
	{
		int32 FrameTag = VISUAL_LOGGER_MAGIC_NUMBER;
		Ar << FrameTag;

		int32 ArchiveVer = Ar.CustomVer(EVisualLoggerVersion::GUID);
		Ar << ArchiveVer;
		Ar << RecordedLogs;
	}

	int32 CustomVer = Ar.CustomVer(EVisualLoggerVersion::GUID);

	return Ar;
}

void FVisualLoggerHelpers::GetCategories(const FVisualLogEntry& EntryItem, TArray<FVisualLoggerCategoryVerbosityPair>& OutCategories)
{
	for (const auto& CurrentEvent : EntryItem.Events)
	{
		OutCategories.Add(FVisualLoggerCategoryVerbosityPair(*CurrentEvent.Name, CurrentEvent.Verbosity));
	}

	for (const auto& CurrentLine : EntryItem.LogLines)
	{
		OutCategories.Add(FVisualLoggerCategoryVerbosityPair(CurrentLine.Category, CurrentLine.Verbosity));
	}

	for (const auto& CurrentElement : EntryItem.ElementsToDraw)
	{
		OutCategories.Add(FVisualLoggerCategoryVerbosityPair(CurrentElement.Category, CurrentElement.Verbosity));
	}

	for (const auto& CurrentSample : EntryItem.HistogramSamples)
	{
		OutCategories.Add(FVisualLoggerCategoryVerbosityPair(CurrentSample.Category, CurrentSample.Verbosity));
	}

	for (const auto& CurrentBlock : EntryItem.DataBlocks)
	{
		OutCategories.Add(FVisualLoggerCategoryVerbosityPair(CurrentBlock.Category, CurrentBlock.Verbosity));
	}
}

void FVisualLoggerHelpers::GetHistogramCategories(const FVisualLogEntry& EntryItem, TMap<FString, TArray<FString> >& OutCategories)
{
	for (const auto& CurrentSample : EntryItem.HistogramSamples)
	{
		auto& DataNames = OutCategories.FindOrAdd(CurrentSample.GraphName.ToString());
		if (DataNames.Find(CurrentSample.DataName.ToString()) == INDEX_NONE)
		{
			DataNames.Add(CurrentSample.DataName.ToString());
		}
	}
}

#endif //ENABLE_VISUAL_LOG

