// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "ModuleManager.h"
#include "ScriptInterface.h"			// Script interface definitions.
#include "Script.h"						// Script constants and forward declarations.
#include "ScriptMacros.h"				// Script macro definitions
#include "ObjectBase.h"					// Object base class.
#include "UObjectAllocator.h"
#include "UObjectGlobals.h"
#include "UObjectMarks.h"
#include "UObjectBase.h"
#include "UObjectBaseUtility.h"
#include "UObjectArray.h"
#include "UObjectHash.h"
#include "WeakObjectPtr.h"
#include "Object.h"
#include "UObjectIterator.h"
#include "CoreNet.h"					// Core networking.
#include "ArchiveUObjectBase.h"			// UObject-related Archive classes.
#include "GarbageCollection.h"			// Realtime garbage collection helpers
#include "TextBuffer.h"					// UObjects for text buffers
#include "Class.h"						// Class definition.
#include "Templates/SubclassOf.h"
#include "StructOnScope.h"
#include "Casts.h"                      // Cast templates
#include "LazyObjectPtr.h"				// Object pointer types
#include "AssetPtr.h"					// Object pointer types
#include "Interface.h"					// Interface base classes.
#include "CoreObject.h"					// Core object class definitions.
#include "Package.h"					// Package class definition
#include "MetaData.h"					// Metadata for packages
#include "UnrealType.h"					// Base property type.
#include "UTextProperty.h"				// FText property type
#include "Stack.h"						// Script stack frame definition.
#include "ObjectRedirector.h"			// Cross-package object redirector
#include "UObjectAnnotation.h"
#include "ObjectMemoryAnalyzer.h"		// Object memory usage analyzer
#include "ReferenceChainSearch.h"		// Search for referencers of an objects
#include "ArchiveUObject.h"				// UObject-related Archive classes.
#include "PackageName.h"
#include "ConstructorHelpers.h"			// Helper templates for object construction.
#include "BulkData.h"					// Bulk data classes
#include "Linker.h"						// Linker.
#include "GCObject.h"			        // non-UObject object referencer
#include "AsyncLoading.h"				// FAsyncPackage definition
#include "StartupPackages.h"
#include "NotifyHook.h"
#include "RedirectCollector.h"
#include "ScriptStackTracker.h"
#include "WorldCompositionUtility.h"
#include "StringClassReference.h"
