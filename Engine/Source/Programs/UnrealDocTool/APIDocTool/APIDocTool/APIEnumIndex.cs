// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace APIDocTool
{
	class APIEnumIndex : APIMemberIndex
	{
		public const string IgnorePrefixLetters = "E";

		public APIEnumIndex(APIPage InParent, IEnumerable<APIMember> Members) : base(InParent, "Enums", "All enums")
		{
			foreach (APIMember Member in Members)
			{
				if (Member is APIEnum)
				{
					Entry NewEntry = new Entry(Member);
					if (NewEntry.SortName.Length >= 2 && IgnorePrefixLetters.Contains(NewEntry.SortName[0]) && Char.IsUpper(NewEntry.SortName[1]))
					{
						NewEntry.SortName = NewEntry.SortName.Substring(1);
					}
					AddToDefaultCategory(NewEntry);
				}
			}
		}
	}
}
