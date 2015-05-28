// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "GameplayTagsEditorModule.h"

#if WITH_EDITOR
#include "UnrealEd.h"
#endif


// You should place include statements to your module's private header files here.  You only need to
// add includes for headers that are used in most of your module's source files though.

#include "Core.h"
#include "Engine.h"
#include "BlueprintGraphDefinitions.h"
#include "GameplayTagsEditorClasses.h"

#include "GraphEditor.h"
#include "SNodePanel.h"
#include "SGraphNode.h"
#include "Editor/UnrealEd/Public/Kismet2/BlueprintEditorUtils.h"

#include "AssetTypeActions_GameplayTagAssetBase.h"
