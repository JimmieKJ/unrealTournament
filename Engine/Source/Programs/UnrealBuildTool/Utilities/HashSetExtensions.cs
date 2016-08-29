// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;

namespace UnrealBuildTool
{
	public static class HashSetExtensions
	{
		public static HashSet<T> ToHashSet<T>(this IEnumerable<T> Sequence)
		{
			return new HashSet<T>(Sequence);
		}
	}
}