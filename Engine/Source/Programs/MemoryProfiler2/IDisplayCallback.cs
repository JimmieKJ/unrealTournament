/**
 * Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections.Generic;
using System.Text;

namespace MemoryProfiler2
{
	delegate void AddressDataUpdatedDelegate( object UserObject );

	struct FSymbolInfo
	{
		public ulong Address;
		public string FunctionName;

		public FSymbolInfo( ulong InAddress, string InName )
		{
			Address = InAddress;
			FunctionName = InName;
		}
	}

	interface IDisplayCallback
	{
		void DisplayCallstack( FCallStack Callstack, AddressDataUpdatedDelegate Callback, object UserObject );
		void Shutdown();
	}
}
