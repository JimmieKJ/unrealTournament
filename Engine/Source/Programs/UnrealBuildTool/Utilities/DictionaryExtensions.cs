// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;

namespace UnrealBuildTool
{
	public static class DictionaryExtensions
	{
		public static Value GetOrAddDefault<Key, Value>(this IDictionary<Key, Value> Dict, Key InKey)
		{
			Value Result;
			if( !Dict.TryGetValue( InKey, out Result ) )
			{
				Result = default(Value);
				Dict.Add( InKey, Result );
			}
			return Result;
		}

		public static Value GetOrAddNew<Key, Value>(this IDictionary<Key, Value> Dict, Key InKey) where Value : new()
		{
			Value Result;
			if( !Dict.TryGetValue( InKey, out Result ) )
			{
				Result = new Value();
				Dict.Add( InKey, Result );
			}
			return Result;
		}
	}
}