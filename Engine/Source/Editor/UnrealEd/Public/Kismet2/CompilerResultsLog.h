// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "BlueprintUtilities.h"
#include "TokenizedMessage.h"
#include "CompilationResult.h"

/** This class maps from final objects to their original source object, across cloning, autoexpansion, etc... */
class UNREALED_API FBacktrackMap
{
protected:
	// Maps from transient object created during compiling to original 'source code' object
	TMap<UObject const*, UObject*> SourceBacktrackMap;
public:
	FBacktrackMap(){}
	virtual ~FBacktrackMap(){}

	/** Update the source backtrack map to note that NewObject was most closely generated/caused by the SourceObject */
	void NotifyIntermediateObjectCreation(UObject* NewObject, UObject* SourceObject);

	/** Returns the true source object for the passed in object */
	UObject* FindSourceObject(UObject* PossiblyDuplicatedObject);
	UObject const* FindSourceObject(UObject const* PossiblyDuplicatedObject);
};

/** This class represents a log of compiler output lines (errors, warnings, and information notes), each of which can be a rich tokenized message */
class UNREALED_API FCompilerResultsLog
{
	// Compiler event
	struct FCompilerEvent
	{
		FString Name;
		uint32 Counter;
		double StartTime;
		double FinishTime;
		TSharedPtr<FCompilerEvent> ParentEventScope;
		TArray< TSharedRef<FCompilerEvent> > ChildEvents;

		FCompilerEvent(TSharedPtr<FCompilerEvent> InParentEventScope = nullptr)
			:Name(TEXT(""))
			,Counter(0)
			,StartTime(0.0)
			,FinishTime(0.0)
			,ParentEventScope(InParentEventScope)
		{
		}

		void Start(const TCHAR* InName)
		{
			Name = InName;
			StartTime = FPlatformTime::Seconds();
		}

		void Finish()
		{
			FinishTime = FPlatformTime::Seconds();
		}
	};

	// Current compiler event scope
	TSharedPtr<FCompilerEvent> CurrentEventScope;

public:
	// List of all tokenized messages
	TArray< TSharedRef<class FTokenizedMessage> > Messages;

	// Number of error messages
	int32 NumErrors;

	// Number of warnings
	int32 NumWarnings;

	// Should we be silent?
	bool bSilentMode;

	// Should we log only Info messages, or all messages?
	bool bLogInfoOnly;

	// Should nodes mentioned in messages be annotated for display with that message?
	bool bAnnotateMentionedNodes;

	// Should detailed results be appended to the final summary log?
	bool bLogDetailedResults;

	// Special maps used for autocreated macros to preserve information about their source
	FBacktrackMap FinalNodeBackToMacroSourceMap;
	TMultiMap<TWeakObjectPtr<UEdGraphNode>, TWeakObjectPtr<UEdGraphNode>> MacroSourceToMacroInstanceNodeMap;

	// Minimum event time (ms) for inclusion into the final summary log
	int EventDisplayThresholdMs;

	/** Tracks nodes that produced errors/warnings */
	TSet< TWeakObjectPtr<UEdGraphNode> > AnnotatedNodes;

protected:
	// Maps from transient object created during compiling to original 'source code' object
	FBacktrackMap SourceBacktrackMap;

	// Name of the source object being compiled
	FString SourceName;

public:
	FCompilerResultsLog();
	virtual ~FCompilerResultsLog();

	/** Register this log with the MessageLog module */
	static void Register();

	/** Unregister this log from the MessageLog module */
	static void Unregister();

	/** Accessor for the LogName, so it can be opened elsewhere */
	static FName GetLogName(){ return Name; }

	/** Set the source name for the final log summary */
	void SetSourceName(const FString& InSourceName)
	{
		SourceName = InSourceName;
	}

	// Note: Message is not a fprintf string!  It should be preformatted, but can contain @@ to indicate object references, which are the varargs
	void Error(const TCHAR* Message, ...);
	void Warning(const TCHAR* Message, ...);
	void Note(const TCHAR* Message, ...);
	void ErrorVA(const TCHAR* Message, va_list ArgPtr);
	void WarningVA(const TCHAR* Message, va_list ArgPtr);
	void NoteVA(const TCHAR* Message, va_list ArgPtr);

	/** Update the source backtrack map to note that NewObject was most closely generated/caused by the SourceObject */
	void NotifyIntermediateObjectCreation(UObject* NewObject, UObject* SourceObject);

	/** Returns the true source object for the passed in object */
	UObject* FindSourceObject(UObject* PossiblyDuplicatedObject);
	UObject const* FindSourceObject(UObject const* PossiblyDuplicatedObject);

	/** Returns the true source object for the passed in object; does type checking on the result */
	template <typename T>
	T* FindSourceObjectTypeChecked(UObject* PossiblyDuplicatedObject)
	{
		return CastChecked<T>(FindSourceObject(PossiblyDuplicatedObject));
	}
	
	template <typename T>
	T const* FindSourceObjectTypeChecked(UObject const* PossiblyDuplicatedObject)
	{
		return CastChecked<T const>(FindSourceObject(PossiblyDuplicatedObject));
	}
	
	void Append(FCompilerResultsLog const& Other);

	/** Begin a new compiler event */
	void BeginEvent(const TCHAR* InName);

	/** End the current compiler event */
	void EndEvent();

	/** Access the current event target log */
	static FCompilerResultsLog* GetEventTarget()
	{
		return CurrentEventTarget;
	}

protected:
	/** Helper method to add a child event to the given parent event scope */
	void AddChildEvent(TSharedPtr<FCompilerEvent>& ParentEventScope, TSharedRef<FCompilerEvent>& ChildEventScope);

	/** Create a tokenized message record from a message containing @@ indicating where each UObject* in the ArgPtr list goes and place it in the MessageLog. */
	void InternalLogMessage(const EMessageSeverity::Type& Severity, const TCHAR* Message, va_list ArgPtr);
	
	/** */
	void AnnotateNode(class UEdGraphNode* Node, TSharedRef<FTokenizedMessage> LogLine);

	/** Internal method to append the final compiler results summary to the MessageLog */
	void InternalLogSummary();

	/** Internal helper method to recursively append event details into the MessageLog */
	void InternalLogEvent(const FCompilerEvent& InEvent, int32 InDepth = 0);

private:
	/** Parses a compiler log dump to generate tokenized output */
	static TArray< TSharedRef<class FTokenizedMessage> > ParseCompilerLogDump(const FString& LogDump);

	/** Goes to an error given a Message Token */
	static void OnGotoError(const class TSharedRef<IMessageToken>& Token);

	/** Callback function for binding the global compiler dump to open the static compiler log */
	static void GetGlobalModuleCompilerDump(const FString& LogDump, ECompilationResult::Type CompilationResult, bool bShowLog);

	/** The log's name, for easy re-use */
	static const FName Name;
	
	/** The log target for compile events */
	static FCompilerResultsLog* CurrentEventTarget;

	/** Handle to the registered GetGlobalModuleCompilerDump delegate. */
	static FDelegateHandle GetGlobalModuleCompilerDumpDelegateHandle;
};

/** This class will begin a new compile event on construction, and automatically end it when the instance goes out of scope */
class UNREALED_API FScopedCompilerEvent
{
public:
	/** Constructor; automatically begins a new event */
	FScopedCompilerEvent(const TCHAR* InName)
	{
		FCompilerResultsLog* ResultsLog = FCompilerResultsLog::GetEventTarget();
		if(ResultsLog != nullptr)
		{
			ResultsLog->BeginEvent(InName);
		}
	}

	/** Destructor; automatically ends the event */
	~FScopedCompilerEvent()
	{
		FCompilerResultsLog* ResultsLog = FCompilerResultsLog::GetEventTarget();
		if(ResultsLog != nullptr)
		{
			ResultsLog->EndEvent();
		}
	}
};

#if STATS
#define BP_SCOPED_COMPILER_EVENT_STAT(Stat) \
	SCOPE_CYCLE_COUNTER(Stat); \
	FScopedCompilerEvent PREPROCESSOR_JOIN(ScopedCompilerEvent,__LINE__)(GET_STATDESCRIPTION(Stat))
#else
#define BP_SCOPED_COMPILER_EVENT_STAT(Stat) \
	FScopedCompilerEvent PREPROCESSOR_JOIN(ScopedCompilerEvent,__LINE__)(ANSI_TO_TCHAR(#Stat))
#endif
#endif	//#if WITH_EDITOR
