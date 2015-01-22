// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once


#ifdef TRUE
	#undef TRUE
#endif

#ifdef FALSE
	#undef FALSE
#endif


/// Trick to prevent people from using old UE4 types (which are still defined in Windef.h on PC).
namespace DoNotUseOldUE4Type
{
	/// Used to cause compile errors through typedefs of unportable types. If unportable types 
	/// are really necessary please use the global scope operator to access it (i.e. ::&nbsp;INT). 
	/// Windows header file includes can be wrapped in #include "AllowWindowsPlatformTypes.h" 
	/// and #include "HideWindowsPlatformTypes.h"
	class FUnusableType
	{
		FUnusableType();
		~FUnusableType();
	};

	/// This type is aliased to FUnusableType - use ::int32 instead.
	typedef FUnusableType INT;

	/// This type is aliased to FUnusableType - use ::uint32 instead.
	typedef FUnusableType UINT;

	/// This type is aliased to FUnusableType - use ::uint32 instead.
	typedef FUnusableType DWORD;

	/// This type is aliased to FUnusableType - use float instead.
	typedef FUnusableType FLOAT;

	/// This value is aliased to FUnusableType - use true instead.
	typedef FUnusableType TRUE;

	/// This value is aliased to FUnusableType - use false instead.
	typedef FUnusableType FALSE;
}



using namespace DoNotUseOldUE4Type;


