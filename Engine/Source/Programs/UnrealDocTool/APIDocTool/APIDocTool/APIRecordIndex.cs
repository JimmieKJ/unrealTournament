// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace APIDocTool
{
	class APIRecordIndex : APIMemberIndex
	{
		public const string IgnorePrefixLetters = "UAFTS"; 

		public APIRecordIndex(APIPage InParent, IEnumerable<APIMember> Members) : base(InParent, "Classes", "All classes")
		{
			foreach (APIMember Member in Members)
			{
				if ((Member is APIRecord && (!((APIRecord)Member).bIsTemplateSpecialization)) || (Member is APITypeDef) || (Member is APIEventParameters))
				{
					if (Member.FindParent<APIRecord>() == null)
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
}
