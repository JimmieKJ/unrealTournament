// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	CoreUObjectPrivate.h: Unreal core private header file.
=============================================================================*/

#ifndef COREUOBJECT_PRIVATE_H
#define COREUOBJECT_PRIVATE_H

#include "Core.h"

#include "ModuleManager.h"
#include "ScriptInterface.h"			// Script interface definitions.
#include "Script.h"						// Script constants and forward declarations.
#include "ObjectBase.h"					// Object base class.
#include "CoreNet.h"					// Core networking.
#include "ArchiveUObjectBase.h"			// UObject-related Archive classes.
#include "GarbageCollection.h"			// Realtime garbage collection helpers
#include "Class.h"						// Class definition.
#include "Casts.h"                      // Cast templates
#include "LazyObjectPtr.h"				// Object pointer types
#include "AssetPtr.h"					// Object pointer types
#include "CoreObject.h"					// Core object class definitions.
#include "UnrealType.h"					// Base property type.
#include "Stack.h"						// Script stack frame definition.
#include "ObjectRedirector.h"			// Cross-package object redirector
#include "UObjectAnnotation.h"
#include "ReferenceChainSearch.h"		// Search for referencers of an objects
#include "ArchiveUObject.h"				// UObject-related Archive classes.
#include "PackageName.h"
#include "BulkData.h"					// Bulk data classes
#include "Linker.h"						// Linker.
#include "GCObject.h"			        // non-UObject object referencer
#include "RedirectCollector.h"
#include "WorldCompositionUtility.h"
#include "StringClassReference.h"
#include "Blueprint/BlueprintSupport.h"

#endif // COREUOBJECT_PRIVATE_H