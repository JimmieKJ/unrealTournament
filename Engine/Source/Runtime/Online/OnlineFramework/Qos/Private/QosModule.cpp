// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "QosPrivatePCH.h"
#include "QosModule.h"
#include "QosInterface.h"


IMPLEMENT_MODULE(FQosModule, Qos);

DEFINE_LOG_CATEGORY(LogQos);

void FQosModule::StartupModule()
{
}

void FQosModule::ShutdownModule()
{
}

TSharedRef<FQosInterface> FQosModule::GetQosInterface()
{
	if (!QosInterface.IsValid())
	{
		QosInterface = MakeShareable(new FQosInterface());
	}
	return QosInterface.ToSharedRef();
}

bool FQosModule::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	// Ignore any execs that don't start with Qos
	if (FParse::Command(&Cmd, TEXT("Qos")))
	{
		return false;
	}

	return false;
}


