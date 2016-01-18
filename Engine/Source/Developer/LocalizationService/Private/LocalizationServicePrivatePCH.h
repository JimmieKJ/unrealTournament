// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "ILocalizationServiceProvider.h"	// for TRANSLATION_SERVICES_WITH_SLATE

#ifndef LOCALIZATION_SERVICES_WITH_SLATE
	#error LOCALIZATION_SERVICES_WITH_SLATE not defined
#endif // LOCALIZATION_SERVICES_WITH_SLATE

#if LOCALIZATION_SERVICES_WITH_SLATE
	#include "SlateBasics.h"
	#include "EditorStyle.h"
#endif // LOCALIZATION_SERVICES_WITH_SLATE

#if WITH_EDITOR
#include "Engine.h"
#include "PackageTools.h"
#include "MessageLog.h"
#endif

#if WITH_UNREAL_DEVELOPER_TOOLS
#include "MessageLogModule.h"
#endif
 
#include "ILocalizationServiceModule.h"
#include "LocalizationServiceModule.h"
#include "LocalizationServiceSettings.h"
#include "ILocalizationServiceProvider.h"
#include "ILocalizationServiceRevision.h"
#include "ILocalizationServiceOperation.h" 
#include "ILocalizationServiceState.h"
#include "LocalizationServiceOperations.h" 