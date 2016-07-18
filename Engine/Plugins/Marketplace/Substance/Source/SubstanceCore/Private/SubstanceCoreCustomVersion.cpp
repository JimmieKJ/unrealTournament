// Copyright 2015 Allegorithmic All Rights Reserved.

#include "SubstanceCorePrivatePCH.h"
#include "SubstanceCoreCustomVersion.h"

const FGuid FSubstanceCoreCustomVersion::GUID(0xC52D92BE, 0xD4A6A49C, 0x11E59324, 0x9A6C11E5);

// Register the custom version with core
FCustomVersionRegistration GRegisterSubstanceCoreCustomVersion(FSubstanceCoreCustomVersion::GUID, FSubstanceCoreCustomVersion::LatestVersion, TEXT("SubstanceCoreVer"));
