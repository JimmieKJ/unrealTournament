// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace APIDocTool
{
	class APIFunctionIndex : APIMemberIndex
	{
		public List<APIFunctionGroup> FunctionGroups = new List<APIFunctionGroup>();

		public APIFunctionIndex(APIPage InParent, IEnumerable<APIFunction> Functions) : base(InParent, "Functions", "Alphabetical list of all functions")
		{
			// Create the operators category
			Category OperatorsCategory = new Category("Operators");
			Categories.Add(OperatorsCategory);

			// Separate out all the functions by their unique names
			Dictionary<APIFunctionKey, List<APIFunction>> KeyedFunctions = new Dictionary<APIFunctionKey, List<APIFunction>>();
			foreach(APIFunction Function in Functions)
			{
				APIFunctionKey Key = new APIFunctionKey(Function.FullName, Function.FunctionType);
				Utility.AddToDictionaryList(Key, Function, KeyedFunctions);
			}

			// Build a list of all the members, creating function groups where necessary
			List<KeyValuePair<APIFunctionKey, APIMember>> KeyedMembers = new List<KeyValuePair<APIFunctionKey, APIMember>>();
			foreach (KeyValuePair<APIFunctionKey, List<APIFunction>> KeyedFunction in KeyedFunctions)
			{
				if (KeyedFunction.Value.Count == 1)
				{
					KeyedMembers.Add(new KeyValuePair<APIFunctionKey,APIMember>(KeyedFunction.Key, KeyedFunction.Value[0]));
				}
				else
				{
					// Check if all the members have the same function group
					APIFunctionGroup FunctionGroup = KeyedFunction.Value[0].Parent as APIFunctionGroup;
					for(int Idx = 1; Idx < KeyedFunction.Value.Count; Idx++)
					{
						if(KeyedFunction.Value[Idx].Parent != FunctionGroup)
						{
							FunctionGroup = null;
							break;
						}
					}

					// If there was no common function group, create a new one
					if (FunctionGroup == null)
					{
						FunctionGroup = new APIFunctionGroup(this, KeyedFunction.Key, KeyedFunction.Value);
						FunctionGroups.Add(FunctionGroup);
					}

					// Add the function group to the member list
					KeyedMembers.Add(new KeyValuePair<APIFunctionKey, APIMember>(KeyedFunction.Key, FunctionGroup));
				}
			}

			// Separate all the functions into different categories
			foreach (KeyValuePair<APIFunctionKey, APIMember> KeyedMember in KeyedMembers)
			{
				if (KeyedMember.Key.Type == APIFunctionType.UnaryOperator || KeyedMember.Key.Type == APIFunctionType.BinaryOperator)
				{
					OperatorsCategory.Entries.Add(new Entry(KeyedMember.Value));
				}
				else
				{
					AddToDefaultCategory(new Entry(KeyedMember.Value));
				}
			}
		}

		public override void GatherReferencedPages(List<APIPage> Pages)
		{
			base.GatherReferencedPages(Pages);
			Pages.AddRange(FunctionGroups);
		}
	}
}
