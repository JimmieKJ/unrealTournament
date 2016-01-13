// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Commandlets/Commandlet.h"

#include "UpdateBackendContentCommandlet.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogTemplates, Log, All);

UCLASS(CustomConstructor)
class UUpdateBackendContentCommandlet : public UCommandlet
{
	GENERATED_UCLASS_BODY()

public:
	UUpdateBackendContentCommandlet(const FObjectInitializer& ObjectInitializer)
		: Super(ObjectInitializer)
	{
		LogToConsole = false;
	}

	// Begin UCommandlet Interface
	virtual int32 Main(const FString& Params) override;
	// End UCommandlet Interface

	// do the export without creating a commandlet
	static bool ExportTemplates(const FString& ExportDir);
};

