// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Profiler.h"


/* Private dependencies
 *****************************************************************************/

#include "ISessionInfo.h"
#include "ISessionInstanceInfo.h"
#include "ISessionManager.h"
#include "Messaging.h"
#include "ProfilerClient.h"
#include "StatsData.h"
#include "StatsFile.h"


/* Private includes
 *****************************************************************************/

// TODO: Remove dependencies between generic profiler code and profiler ui code

// Common headers and managers.
#include "ProfilerRawStats.h"
#include "ProfilerStream.h"

#include "ProfilerSample.h"
#include "ProfilerFPSAnalyzer.h"
#include "ProfilerDataProvider.h"
#include "ProfilerDataSource.h"
#include "ProfilerSession.h"
#include "ProfilerCommands.h"
#include "ProfilerManager.h"

// Slate related headers.
#include "SProfilerToolbar.h"
#include "SFiltersAndPresets.h"
#include "SEventGraph.h"
#include "SDataGraph.h"
#include "SProfilerMiniView.h"
#include "SProfilerThreadView.h"
#include "SProfilerGraphPanel.h"
#include "SProfilerFPSChartPanel.h"
#include "SProfilerWindow.h"
#include "SHistogram.h"
#include "StatDragDropOp.h"
