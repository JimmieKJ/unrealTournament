// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SwarmDefines.h"
#include "SwarmMessages.generated.h"


USTRUCT()
struct FSwarmPingMessage
{
	GENERATED_USTRUCT_BODY()
};

template<>
struct TStructOpsTypeTraits<FSwarmPingMessage> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithMessageHandling = true
	};
};


USTRUCT()
struct FSwarmPongMessage
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	bool			bIsEditor;

	UPROPERTY()
	FString			ComputerName;

	FSwarmPongMessage() {}

	FSwarmPongMessage(bool bInIsEditor, const TCHAR* InComputerName)
	:	bIsEditor(bInIsEditor)
	,	ComputerName(InComputerName)
	{
	}
};

template<>
struct TStructOpsTypeTraits<FSwarmPongMessage> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithMessageHandling = true
	};
};


USTRUCT()
struct FSwarmInfoMessage
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString TextMessage;

	FSwarmInfoMessage() {}

	FSwarmInfoMessage(const FString& InTextMessage)
		:	TextMessage(InTextMessage)
	{
	}
};

template<>
struct TStructOpsTypeTraits<FSwarmInfoMessage> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithMessageHandling = true
	};
};


USTRUCT()
struct FSwarmAlertMessage
{
	GENERATED_USTRUCT_BODY()

	/** The Job Guid */
	UPROPERTY()
	FGuid			JobGuid;
	/** The type of alert */
	UPROPERTY()
	uint8			AlertLevel;
	/** The identifier for the object that is associated with the issue */
	UPROPERTY()
	FGuid			ObjectGuid;
	/** App-specific identifier for the type of the object */
	UPROPERTY()
	int32			TypeId;
	/** Generic text message for informational purposes */
	UPROPERTY()
	FString			TextMessage;

	FSwarmAlertMessage() {}

	FSwarmAlertMessage(const FGuid& InJobGuid, NSwarm::TAlertLevel InAlertLevel, const FGuid& InObjectGuid, int32 InTypeId, const FString& InTextMessage)
		:	JobGuid(InJobGuid)
		,	AlertLevel(InAlertLevel)
		,	ObjectGuid(InObjectGuid)
		,	TypeId(InTypeId)
		,	TextMessage(InTextMessage)
	{
	}
};

template<>
struct TStructOpsTypeTraits<FSwarmAlertMessage> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithMessageHandling = true
	};
};


USTRUCT()
struct FSwarmTimingMessage
{
	GENERATED_USTRUCT_BODY()

	/** State that the distributed job is transitioning to */
	UPROPERTY()
	uint8			State;
	/** The thread this state is referring to */
	UPROPERTY()
	int32			ThreadNum;

	FSwarmTimingMessage() {}

	FSwarmTimingMessage(NSwarm::TProgressionState InState, int32 InThreadNum)
		:	State(InState)
		,	ThreadNum(InThreadNum)
	{
	}
};

template<>
struct TStructOpsTypeTraits<FSwarmTimingMessage> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithMessageHandling = true
	};
};


USTRUCT()
struct FSwarmTaskRequestMessage
{
	GENERATED_USTRUCT_BODY()

	FSwarmTaskRequestMessage() {}
};

template<>
struct TStructOpsTypeTraits<FSwarmTaskRequestMessage> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithMessageHandling = true
	};
};


USTRUCT()
struct FSwarmTaskRequestReleaseMessage
{
	GENERATED_USTRUCT_BODY()

	FSwarmTaskRequestReleaseMessage() {}
};

template<>
struct TStructOpsTypeTraits<FSwarmTaskRequestReleaseMessage> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithMessageHandling = true
	};
};


USTRUCT()
struct FSwarmTaskRequestReservationMessage
{
	GENERATED_USTRUCT_BODY()

	FSwarmTaskRequestReservationMessage() {}
};

template<>
struct TStructOpsTypeTraits<FSwarmTaskRequestReservationMessage> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithMessageHandling = true
	};
};


USTRUCT()
struct FSwarmTaskRequestSpecificationMessage
{
	GENERATED_USTRUCT_BODY()

	/** The GUID used for identifying the Task being referred to */
	UPROPERTY()
	FGuid					TaskGuid;
	/** The Task's parameter string specified with AddTask */
	UPROPERTY()
	FString					Parameters;
	/** Flags used to control the behavior of the Task, subject to overrides from the containing Job */
	UPROPERTY()
	uint8					Flags;
	/** The Task's cost, relative to all other Tasks in the same Job, used for even distribution and scheduling */
	UPROPERTY()
	uint32					Cost;
	/** Any additional Task dependencies */
	UPROPERTY()
	TArray<FString>			Dependencies;

	FSwarmTaskRequestSpecificationMessage() {}

	FSwarmTaskRequestSpecificationMessage(FGuid InTaskGuid, const FString& InParameters, NSwarm::TJobTaskFlags InFlags, const TArray<FString>& InDependencies)
		:	TaskGuid(InTaskGuid)
		,	Parameters(InParameters)
		,	Flags(InFlags)
		,	Dependencies(InDependencies)
	{ }
};

template<>
struct TStructOpsTypeTraits<FSwarmTaskRequestSpecificationMessage> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithMessageHandling = true
	};
};


USTRUCT()
struct FSwarmJobStateMessage
{
	GENERATED_USTRUCT_BODY()

	/** The Job GUID used for identifying the Job */
	UPROPERTY()
	FGuid			Guid;
	/** The current state and arbitrary message */
	UPROPERTY()
	uint8			State;
	UPROPERTY()
	FString			Message;
	/** Various stats, including run time, exit codes, etc. */
	UPROPERTY()
	int32			ExitCode;
	UPROPERTY()
	double			RunningTime;

	FSwarmJobStateMessage() {}

	FSwarmJobStateMessage(FGuid InGuid, NSwarm::TJobTaskState InState, const FString& InMessage, int32 InExitCode, double InRunningTime)
		:	Guid(InGuid)
		,	State(InState)
		,	Message(InMessage)
		,	ExitCode(InExitCode)
		,	RunningTime(InRunningTime)
	{
	}
};

template<>
struct TStructOpsTypeTraits<FSwarmJobStateMessage> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithMessageHandling = true
	};
};


USTRUCT()
struct FSwarmTaskStateMessage
{
	GENERATED_USTRUCT_BODY()

	/** The Task GUID used for identifying the Task */
	UPROPERTY()
	FGuid			Guid;
	/** The current state and arbitrary message */
	UPROPERTY()
	uint8			State;
	UPROPERTY()
	FString			Message;
	/** Various stats, including run time, exit codes, etc. */
	UPROPERTY()
	int32			ExitCode;
	UPROPERTY()
	double			RunningTime;

	FSwarmTaskStateMessage() {}

	FSwarmTaskStateMessage(FGuid InGuid, NSwarm::TJobTaskState InState, const FString& InMessage, int32 InExitCode, double InRunningTime)
		:	Guid(InGuid)
		,	State(InState)
		,	Message(InMessage)
		,	ExitCode(InExitCode)
		,	RunningTime(InRunningTime)
	{
	}
};

template<>
struct TStructOpsTypeTraits<FSwarmTaskStateMessage> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithMessageHandling = true
	};
};


USTRUCT()
struct FSwarmQuitMessage
{
	GENERATED_USTRUCT_BODY()

	FSwarmQuitMessage() {}
};

template<>
struct TStructOpsTypeTraits<FSwarmQuitMessage> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithMessageHandling = true
	};
};
