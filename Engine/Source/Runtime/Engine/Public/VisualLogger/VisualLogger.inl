// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

// Unfortunately needs to be a #define since it uses GET_VARARGS_RESULT which uses the va_list stuff which operates on the
// current function, so we can't easily call a function
#define COLLAPSED_LOGF(SerializeFunc) \
	int32	BufferSize	= 1024; \
	TCHAR*	Buffer		= NULL; \
	int32	Result		= -1; \
	/* allocate some stack space to use on the first pass, which matches most strings */ \
	TCHAR	StackBuffer[512]; \
	TCHAR*	AllocatedBuffer = NULL; \
	\
	/* first, try using the stack buffer */ \
	Buffer = StackBuffer; \
	GET_VARARGS_RESULT( Buffer, ARRAY_COUNT(StackBuffer), ARRAY_COUNT(StackBuffer) - 1, Fmt, Fmt, Result ); \
	\
	/* if that fails, then use heap allocation to make enough space */ \
	while(Result == -1) \
	{ \
		FMemory::SystemFree(AllocatedBuffer); \
		/* We need to use malloc here directly as GMalloc might not be safe. */ \
		Buffer = AllocatedBuffer = (TCHAR*) FMemory::SystemMalloc( BufferSize * sizeof(TCHAR) ); \
		GET_VARARGS_RESULT( Buffer, BufferSize, BufferSize-1, Fmt, Fmt, Result ); \
		BufferSize *= 2; \
	}; \
	Buffer[Result] = 0; \
	; \
	\
	SerializeFunc; \
	FMemory::SystemFree(AllocatedBuffer);


FORCEINLINE
bool CheckVisualLogInputInternal(const class UObject* Object, const struct FLogCategoryBase& Category, ELogVerbosity::Type Verbosity, UWorld **World, FVisualLogEntry **CurrentEntry)
{
	FVisualLogger& VisualLogger = FVisualLogger::Get();
	if (!Object || (GEngine && GEngine->bDisableAILogging) || VisualLogger.IsRecording() == false || Object->HasAnyFlags(RF_ClassDefaultObject))
	{
		return false; 
	}
	const FName CategoryName = Category.GetCategoryName(); 
	if (VisualLogger.IsBlockedForAllCategories() && VisualLogger.IsWhiteListed(CategoryName) == false)
	{
		return false; 
	}

	*World = GEngine->GetWorldFromContextObject(Object, false); 
	if (ensure(*World != NULL) == false)
	{
		return false; 
	}

	*CurrentEntry = VisualLogger.GetEntryToWrite(Object, (*World)->TimeSeconds);
	if (ensure(CurrentEntry != NULL) == false)
	{
		return false; 
	}

	return true;
}

FORCEINLINE
FVisualLogEntry* FVisualLogger::GetEntryToWrite(const class UObject* Object, float TimeStamp, ECreateIfNeeded ShouldCreate)
{
	FVisualLogEntry* CurrentEntry = NULL;
	UObject * LogOwner = FVisualLogger::FindRedirection(Object);

	bool InitializeNewEntry = false;

	UWorld* World = GetWorld();
	if (CurrentEntryPerObject.Contains(LogOwner))
	{
		CurrentEntry = &CurrentEntryPerObject[LogOwner];
		InitializeNewEntry = TimeStamp > CurrentEntry->TimeStamp && ShouldCreate == ECreateIfNeeded::Create;
		if (World)
		{
			World->GetTimerManager().ClearAllTimersForObject(this);
		}
	}

	if(!CurrentEntry)
	{
		CurrentEntry = &CurrentEntryPerObject.Add(LogOwner);
		ObjectToNameMap.Add(LogOwner, LogOwner->GetFName());
		ObjectToPointerMap.Add(LogOwner, LogOwner);
		InitializeNewEntry = true;
	}

	if (InitializeNewEntry)
	{
		CurrentEntry->Reset();
		CurrentEntry->TimeStamp = TimeStamp;

		if (RedirectionMap.Contains(LogOwner))
		{
			if (ObjectToPointerMap.Contains(LogOwner) && ObjectToPointerMap[LogOwner].IsValid())
			{
				const class AActor* LogOwnerAsActor = Cast<class AActor>(LogOwner);
				if (LogOwnerAsActor)
				{
					LogOwnerAsActor->GrabDebugSnapshot(CurrentEntry);
				}
			}
			for (auto Child : RedirectionMap[LogOwner])
			{
				if (Child.IsValid())
				{
					const class AActor* ChildAsActor = Cast<class AActor>(Child.Get());
					if (ChildAsActor)
					{
						ChildAsActor->GrabDebugSnapshot(CurrentEntry);
					}
				}
			}
		}
		else
		{
			const class AActor* ObjectAsActor = Cast<class AActor>(Object);
			if (ObjectAsActor)
			{
				CurrentEntry->Location = ObjectAsActor->GetActorLocation();
				ObjectAsActor->GrabDebugSnapshot(CurrentEntry);
			}
		}

		if (World)
		{
			//set next tick timer to flush obsolete/old entries
			World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda(
				[this](){
				for (auto& CurrentPair : CurrentEntryPerObject)
				{
					FVisualLogEntry* Entry = &CurrentPair.Value;
					if (Entry->TimeStamp >= 0) // CurrentEntry->TimeStamp == -1 means it's not initialized entry information
					{
						for (auto* Device : OutputDevices)
						{
							Device->Serialize(CurrentPair.Key, ObjectToNameMap[CurrentPair.Key], *Entry);
						}
						Entry->Reset();
					}
				}
			}
			));
		}
	}

	return CurrentEntry;
}

FORCEINLINE
void FVisualLogger::Flush()
{
	for (auto &CurrentEntry : CurrentEntryPerObject)
	{
		if (CurrentEntry.Value.TimeStamp >= 0)
		{
			for (auto* Device : OutputDevices)
			{
				Device->Serialize(CurrentEntry.Key, ObjectToNameMap[CurrentEntry.Key], CurrentEntry.Value);
			}
			CurrentEntry.Value.Reset();
		}
	}
}


FORCEINLINE
VARARG_BODY(void, FVisualLogger::CategorizedLogf, const TCHAR*, VARARG_EXTRA(const class UObject* Object) VARARG_EXTRA(const struct FLogCategoryBase& Category) VARARG_EXTRA(ELogVerbosity::Type Verbosity) VARARG_EXTRA(int32 UniqueLogId))
{
	SCOPE_CYCLE_COUNTER(STAT_VisualLog);
	UWorld *World = NULL;
	FVisualLogEntry *CurrentEntry = NULL;
	if (CheckVisualLogInputInternal(Object, Category, Verbosity, &World, &CurrentEntry) == false)
	{
		return;
	}

	COLLAPSED_LOGF(
		CurrentEntry->AddText(Buffer, Category.GetCategoryName(), Verbosity);
	);
}

FORCEINLINE
VARARG_BODY(void, FVisualLogger::GeometryShapeLogf, const TCHAR*, VARARG_EXTRA(const class UObject* Object) VARARG_EXTRA(const struct FLogCategoryBase& Category) VARARG_EXTRA(ELogVerbosity::Type Verbosity) VARARG_EXTRA(int32 UniqueLogId) VARARG_EXTRA(const FVector& Start) VARARG_EXTRA(const FVector& End) VARARG_EXTRA(const FColor& Color))
{
	SCOPE_CYCLE_COUNTER(STAT_VisualLog);
	UWorld *World = NULL;
	FVisualLogEntry *CurrentEntry = NULL;
	if (CheckVisualLogInputInternal(Object, Category, Verbosity, &World, &CurrentEntry) == false)
	{
		return;
	}

	COLLAPSED_LOGF(
		CurrentEntry->AddElement(Start, End, Category.GetCategoryName(), Verbosity, Color, Buffer);
	);
}

FORCEINLINE
VARARG_BODY(void, FVisualLogger::GeometryShapeLogf, const TCHAR*, VARARG_EXTRA(const class UObject* Object) VARARG_EXTRA(const struct FLogCategoryBase& Category) VARARG_EXTRA(ELogVerbosity::Type Verbosity) VARARG_EXTRA(int32 UniqueLogId) VARARG_EXTRA(const FVector& Location) VARARG_EXTRA(float Radius) VARARG_EXTRA(const FColor& Color))
{
	SCOPE_CYCLE_COUNTER(STAT_VisualLog);
	UWorld *World = NULL;
	FVisualLogEntry *CurrentEntry = NULL;
	if (CheckVisualLogInputInternal(Object, Category, Verbosity, &World, &CurrentEntry) == false)
	{
		return;
	}

	COLLAPSED_LOGF(
		CurrentEntry->AddElement(Location, Category.GetCategoryName(), Verbosity, Color, Buffer, Radius);
	);
}

FORCEINLINE
VARARG_BODY(void, FVisualLogger::GeometryShapeLogf, const TCHAR*, VARARG_EXTRA(const class UObject* Object) VARARG_EXTRA(const struct FLogCategoryBase& Category) VARARG_EXTRA(ELogVerbosity::Type Verbosity) VARARG_EXTRA(int32 UniqueLogId) VARARG_EXTRA(const FBox& Box) VARARG_EXTRA(const FColor& Color))
{
	SCOPE_CYCLE_COUNTER(STAT_VisualLog);
	UWorld *World = NULL;
	FVisualLogEntry *CurrentEntry = NULL;
	if (CheckVisualLogInputInternal(Object, Category, Verbosity, &World, &CurrentEntry) == false)
	{
		return;
	}

	COLLAPSED_LOGF(
		CurrentEntry->AddElement(Box, Category.GetCategoryName(), Verbosity, Color, Buffer);
	);
}

FORCEINLINE
VARARG_BODY(void, FVisualLogger::GeometryShapeLogf, const TCHAR*, VARARG_EXTRA(const class UObject* Object) VARARG_EXTRA(const struct FLogCategoryBase& Category) VARARG_EXTRA(ELogVerbosity::Type Verbosity) VARARG_EXTRA(int32 UniqueLogId) VARARG_EXTRA(const FVector& Orgin) VARARG_EXTRA(const FVector& Direction) VARARG_EXTRA(const float Length) VARARG_EXTRA(const float Angle)  VARARG_EXTRA(const FColor& Color))
{
	SCOPE_CYCLE_COUNTER(STAT_VisualLog);
	UWorld *World = NULL;
	FVisualLogEntry *CurrentEntry = NULL;
	if (CheckVisualLogInputInternal(Object, Category, Verbosity, &World, &CurrentEntry) == false)
	{
		return;
	}

	COLLAPSED_LOGF(
		CurrentEntry->AddElement(Orgin, Direction, Length, Angle, Angle, Category.GetCategoryName(), Verbosity, Color, Buffer);
	);
}

FORCEINLINE
VARARG_BODY(void, FVisualLogger::GeometryShapeLogf, const TCHAR*, VARARG_EXTRA(const class UObject* Object) VARARG_EXTRA(const struct FLogCategoryBase& Category) VARARG_EXTRA(ELogVerbosity::Type Verbosity) VARARG_EXTRA(int32 UniqueLogId) VARARG_EXTRA(const FVector& Start) VARARG_EXTRA(const FVector& End) VARARG_EXTRA(const float Radius) VARARG_EXTRA(const FColor& Color))
{
	SCOPE_CYCLE_COUNTER(STAT_VisualLog);
	UWorld *World = NULL;
	FVisualLogEntry *CurrentEntry = NULL;
	if (CheckVisualLogInputInternal(Object, Category, Verbosity, &World, &CurrentEntry) == false)
	{
		return;
	}

	COLLAPSED_LOGF(
		CurrentEntry->AddElement(Start, End, Radius, Category.GetCategoryName(), Verbosity, Color, Buffer);
	);
}

FORCEINLINE
VARARG_BODY(void, FVisualLogger::GeometryShapeLogf, const TCHAR*, VARARG_EXTRA(const class UObject* Object) VARARG_EXTRA(const struct FLogCategoryBase& Category) VARARG_EXTRA(ELogVerbosity::Type Verbosity) VARARG_EXTRA(int32 UniqueLogId) VARARG_EXTRA(const FVector& Center) VARARG_EXTRA(float HalfHeight) VARARG_EXTRA(float Radius) VARARG_EXTRA(const FQuat& Rotation) VARARG_EXTRA(const FColor& Color))
{
	SCOPE_CYCLE_COUNTER(STAT_VisualLog);
	UWorld *World = NULL;
	FVisualLogEntry *CurrentEntry = NULL;
	if (CheckVisualLogInputInternal(Object, Category, Verbosity, &World, &CurrentEntry) == false)
	{
		return;
	}

	COLLAPSED_LOGF(
		CurrentEntry->AddElement(Center, HalfHeight, Radius, Rotation, Category.GetCategoryName(), Verbosity, Color, Buffer);
	);
}

FORCEINLINE
VARARG_BODY(void, FVisualLogger::HistogramDataLogf, const TCHAR*, VARARG_EXTRA(const class UObject* Object) VARARG_EXTRA(const struct FLogCategoryBase& Category) VARARG_EXTRA(ELogVerbosity::Type Verbosity) VARARG_EXTRA(int32 UniqueLogId) VARARG_EXTRA(FName GraphName) VARARG_EXTRA(FName DataName) VARARG_EXTRA(const FVector2D& Data) VARARG_EXTRA(const FColor& Color))
{
	SCOPE_CYCLE_COUNTER(STAT_VisualLog);
	UWorld *World = NULL;
	FVisualLogEntry *CurrentEntry = NULL;
	if (CheckVisualLogInputInternal(Object, Category, Verbosity, &World, &CurrentEntry) == false)
	{
		return;
	}

	COLLAPSED_LOGF(
		CurrentEntry->AddHistogramData(Data, Category.GetCategoryName(), Verbosity, GraphName, DataName);
	);
}

#undef COLLAPSED_LOGF

FORCEINLINE
void FVisualLogger::EventLog(const class UObject* Object, const FName EventTag1, const FVisualLogEventBase& Event1, const FVisualLogEventBase& Event2, const FVisualLogEventBase& Event3, const FVisualLogEventBase& Event4, const FVisualLogEventBase& Event5, const FVisualLogEventBase& Event6)
{
	EventLog(Object, EventTag1, Event1, Event2, Event3, Event4, Event5);
	EventLog(Object, EventTag1, Event6);
}

FORCEINLINE
void FVisualLogger::EventLog(const class UObject* Object, const FName EventTag1, const FVisualLogEventBase& Event1, const FVisualLogEventBase& Event2, const FVisualLogEventBase& Event3, const FVisualLogEventBase& Event4, const FVisualLogEventBase& Event5)
{
	EventLog(Object, EventTag1, Event1, Event2, Event3, Event4);
	EventLog(Object, EventTag1, Event5);
}

FORCEINLINE
void FVisualLogger::EventLog(const class UObject* Object, const FName EventTag1, const FVisualLogEventBase& Event1, const FVisualLogEventBase& Event2, const FVisualLogEventBase& Event3, const FVisualLogEventBase& Event4)
{
	EventLog(Object, EventTag1, Event1, Event2, Event3);
	EventLog(Object, EventTag1, Event4);
}

FORCEINLINE
void FVisualLogger::EventLog(const class UObject* Object, const FName EventTag1, const FVisualLogEventBase& Event1, const FVisualLogEventBase& Event2, const FVisualLogEventBase& Event3)
{
	EventLog(Object, EventTag1, Event1, Event2);
	EventLog(Object, EventTag1, Event3);
}

FORCEINLINE
void FVisualLogger::EventLog(const class UObject* Object, const FName EventTag1, const FVisualLogEventBase& Event1, const FVisualLogEventBase& Event2)
{
	EventLog(Object, EventTag1, Event1);
	EventLog(Object, EventTag1, Event2);
}

FORCEINLINE
void FVisualLogger::EventLog(const class UObject* Object, const FName EventTag1, const FVisualLogEventBase& Event, const FName EventTag2, const FName EventTag3, const FName EventTag4, const FName EventTag5, const FName EventTag6)
{
	SCOPE_CYCLE_COUNTER(STAT_VisualLog);
	UWorld *World = NULL;
	FVisualLogEntry *CurrentEntry = NULL;
	const FLogCategory<ELogVerbosity::Log, ELogVerbosity::Log> Category(*Event.Name);
	if (CheckVisualLogInputInternal(Object, Category, ELogVerbosity::Log, &World, &CurrentEntry) == false)
	{
		return;
	}

	int32 Index = CurrentEntry->Events.Find(FVisualLogEvent(Event));
	if (Index != INDEX_NONE)
	{
		CurrentEntry->Events[Index].Counter++;
	}
	else
	{
		Index = CurrentEntry->AddEvent(Event);
	}

	CurrentEntry->Events[Index].EventTags.FindOrAdd(EventTag1)++;
	CurrentEntry->Events[Index].EventTags.FindOrAdd(EventTag2)++;
	CurrentEntry->Events[Index].EventTags.FindOrAdd(EventTag3)++;
	CurrentEntry->Events[Index].EventTags.FindOrAdd(EventTag4)++;
	CurrentEntry->Events[Index].EventTags.FindOrAdd(EventTag5)++;
	CurrentEntry->Events[Index].EventTags.FindOrAdd(EventTag6)++;
	CurrentEntry->Events[Index].EventTags.Remove(NAME_None);
}
