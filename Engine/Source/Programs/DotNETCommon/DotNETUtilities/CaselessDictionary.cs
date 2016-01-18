// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Runtime.Serialization;

namespace Tools.DotNETCommon.CaselessDictionary
{
	/// <summary>
	/// Equivalent of case insensitive Dictionary<string, T>
	/// </summary>
	/// <typeparam name="T"></typeparam>
	[Serializable]
	public class CaselessDictionary<T> : Dictionary<string, T>
	{
		public CaselessDictionary()
			: base(StringComparer.InvariantCultureIgnoreCase)
		{
		}

		public CaselessDictionary(int Capacity)
			: base(Capacity, StringComparer.InvariantCultureIgnoreCase)
		{
		}

		public CaselessDictionary(IDictionary<string, T> Dict)
			: base(Dict, StringComparer.InvariantCultureIgnoreCase)
		{
		}

		protected CaselessDictionary(SerializationInfo Info, StreamingContext Context)
			: base(Info, Context)
		{
		}
	}
}
