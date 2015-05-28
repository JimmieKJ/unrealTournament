// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UObjectThreadContext.cpp: Unreal object globals
=============================================================================*/

#include "CoreUObjectPrivate.h"
#include "UObject/UObjectThreadContext.h"

FUObjectThreadContext::FUObjectThreadContext()
: ImportCount(0)
, ForcedExportCount(0)
, ObjBeginLoadCount(0)
, IsRoutingPostLoad(false)
, IsInConstructor(0)
, ConstructedObject(nullptr)
, SerializedObject(nullptr)
, SerializedPackageLinker(nullptr)
, SerializedImportIndex(INDEX_NONE)
, SerializedImportLinker(nullptr)
, SerializedExportIndex(INDEX_NONE)
, SerializedExportLinker(nullptr)
{}