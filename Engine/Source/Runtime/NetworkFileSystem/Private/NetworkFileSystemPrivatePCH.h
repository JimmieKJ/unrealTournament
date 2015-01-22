// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NetworkFileSystem.h"


/* Private dependencies
 *****************************************************************************/

#include "Developer/DirectoryWatcher/Public/DirectoryWatcherModule.h"
#include "IPlatformFileSandboxWrapper.h"
#include "MultichannelTCP.h"
#include "Projects.h"
#include "Sockets.h"
#include "SocketSubsystem.h"


/* Private includes
 *****************************************************************************/

DECLARE_LOG_CATEGORY_EXTERN(LogFileServer, Log, All);

#include "NetworkFileServerConnection.h"
#include "NetworkFileServer.h"
#include "NetworkFileServerHttp.h"
