// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace APIDocTool
{
	class APIConstantIndex : APIMemberIndex
	{
		public APIConstantIndex(APIPage InParent, IEnumerable<APIMember> Members) 
			: base(InParent, "Constants", "Alphabetical list of all constants")
		{
			foreach (APIMember Member in Members)
			{
				if (Member.FullName == Member.Name)
				{
					if (Member is APIConstant)
					{
						// Add the constant as-is
						AddToDefaultCategory(new Entry(Member));
					}
					else if (Member is APIEnum)
					{
						// Get the enum
						APIEnum Enum = (APIEnum)Member;

						// Get the scope prefix for all enum members
						string EnumPrefix = Enum.FullName;
						int LastScope = EnumPrefix.LastIndexOf("::");
						EnumPrefix = (LastScope == -1) ? "" : EnumPrefix.Substring(0, LastScope + 2);

						// Add all the values
						foreach (APIEnumValue EnumValue in Enum.Values)
						{
							AddToDefaultCategory(new Entry(EnumPrefix + EnumValue.Name, Enum.LinkPath));
						}
					}
				}
			}
		}
	}
}
