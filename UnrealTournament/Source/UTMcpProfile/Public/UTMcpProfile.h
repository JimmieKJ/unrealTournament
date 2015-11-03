// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once 

#include "EngineDefines.h"
#include "EngineSettings.h"
#include "EngineStats.h"
#include "EngineLogs.h"
#include "EngineGlobals.h"
#include "Engine/EngineBaseTypes.h"

#include "McpProfile.h"

#include "UTMcpProfile.generated.h"

UCLASS()
class UTMCPPROFILE_API UUTMcpProfile : public UMcpProfile
{
	GENERATED_UCLASS_BODY()

public:
	UFUNCTION(ServiceRequest(MCP))
	void OpenCase(const FString& CaseItemId, UPARAM(NotReplicated) FClientUrlContext& Context = FClientUrlContext::Default);
};
