// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace APIDocTool
{
	class APIActionPin : APIPage
	{
		public readonly string Tooltip;
		public readonly string TypeText;
		public readonly string DefaultValue;

		public readonly string PinCategory;
		public readonly string PinSubCategory;
		public readonly string PinSubCategoryObject;

		public readonly bool bInputPin;
		public readonly bool bIsConst;
		public readonly bool bIsReference;
		public readonly bool bIsArray;
		public readonly bool bShowAssetPicker;
		public readonly bool bShowPinLabel;

		public static string GetPinName(Dictionary<string, object> PinProperties)
		{
			object Value;
			string Name;

			if (PinProperties.TryGetValue("Name", out Value))
			{
				Name = (string)Value;
			}
			else
			{
				Debug.Assert(((string)PinProperties["PinCategory"] == "exec") || ((string)PinProperties["PinCategory"] == "wildcard"));
				
				Name = ((string)PinProperties["Direction"] == "input" ? "In" : "Out");
			}

			return Name;
		}

		public APIActionPin(APIPage InParent, string InName, Dictionary<string, object> PinProperties)
			: base(InParent, InName)
		{
			object Value;

			Debug.Assert(InName == GetPinName(PinProperties));

			// We have to check this again to set the show label property
			bShowPinLabel = PinProperties.TryGetValue("Name", out Value);

			bInputPin = (string)PinProperties["Direction"] == "input";
			PinCategory = (string)PinProperties["PinCategory"];
			TypeText = (string)PinProperties["TypeText"];

			if (PinProperties.TryGetValue("Tooltip", out Value))
			{
				Tooltip = String.Concat(((string)Value).Split('\n').Select(Line => Line.Trim() + '\n'));
			}
			else
			{
				Tooltip = "";
			}

			if (PinProperties.TryGetValue("PinSubCategory", out Value))
			{
				PinSubCategory = (string)Value;
			}
			else
			{
				PinSubCategory = "";
			}

			if (PinProperties.TryGetValue("PinSubCategoryObject", out Value))
			{
				PinSubCategoryObject = (string)Value;
			}
			else
			{
				PinSubCategoryObject = "";
			}
			
			if (PinProperties.TryGetValue("DefaultValue", out Value))
			{
				DefaultValue = (string)Value;
			}
			else
			{
				DefaultValue = "";
			}
			if (PinProperties.TryGetValue("IsConst", out Value))
			{
				bIsConst = Convert.ToBoolean((string)Value);
			}

			if (PinProperties.TryGetValue("IsReference", out Value))
			{
				bIsReference = Convert.ToBoolean((string)Value);
			}

			if (PinProperties.TryGetValue("IsArray", out Value))
			{
				bIsArray = Convert.ToBoolean((string)Value);
			}

			if (PinProperties.TryGetValue("ShowAssetPicker", out Value))
			{
				bShowAssetPicker = Convert.ToBoolean((string)Value);
			}
			else
			{
				bShowAssetPicker = true;
			}
		}

		public string GetTypeText()
		{
			if (PinCategory == "struct")
			{
				if (PinSubCategoryObject == "Vector")
				{
					return "vector";
				}
				else if (PinSubCategoryObject == "Transform")
				{
					return "transform";
				}
				else if (PinSubCategoryObject == "Rotator")
				{
					return "rotator";
				}
				if (PinSubCategoryObject == "Vector2D")
				{
					return "vector2d struct";
				}
				else if (PinSubCategoryObject == "LinearColor")
				{
					return "linearcolor";
				}
			}
			else if (PinCategory == "bool")
			{
				return "boolean";
			}
			else if (PinCategory == "int")
			{
				return "integer";
			}
			else if (PinCategory == "byte")
			{
				if (PinSubCategoryObject != "")
				{
					return "enum";
				}
			}

			return PinCategory;
		}

		private string GetId()
		{
			return Name.Replace(' ', '_');
		}

		public void WriteObject(UdnWriter Writer, bool bDrawLabels)
		{
			Writer.EnterObject("BlueprintPin");
			Writer.WriteParamLiteral("type", GetTypeText() + (bIsArray ? " array" : ""));
			Writer.WriteParamLiteral("id", GetId());

			if (bShowPinLabel && bDrawLabels)
			{
				Writer.WriteParamLiteral("title",  Name);
			}

			var DefaultValueElements = DefaultValue.Replace('(',' ').Replace(')',' ').Split(',');
			switch (GetTypeText())
			{
				case "byte":
					Writer.WriteParamLiteral("value", (DefaultValueElements[0].Length > 0 ? DefaultValueElements[0] : "0"));
					break;

				case "class":
					if (bShowAssetPicker)
					{
						Writer.WriteParamLiteral("value", "Select Class");
					}
					break;

				case "float":
					Writer.WriteParamLiteral("value", (DefaultValueElements[0].Length > 0 ? DefaultValueElements[0] : "0.0"));
					break;

				case "integer":
					Writer.WriteParamLiteral("value", (DefaultValueElements[0].Length > 0 ? DefaultValueElements[0] : "0"));
					break;

				case "object":
					if (bShowAssetPicker)
					{
						Writer.WriteParamLiteral("value", "Select Asset");
					}
					break;

				case "rotator":
					Writer.WriteParamLiteral("yaw", (DefaultValueElements[0].Length > 0 ? DefaultValueElements[0] : "0.0"));
					Writer.WriteParamLiteral("pitch", (DefaultValueElements.Length > 1 ? DefaultValueElements[1] : "0.0"));
					Writer.WriteParamLiteral("roll", (DefaultValueElements.Length > 2 ? DefaultValueElements[2] : "0.0"));
					break;

				case "linearcolor":
					float r = (DefaultValueElements[0].Length > 0 ? float.Parse(DefaultValueElements[0].Split('=')[1]) : 0) * 255;
					float g = (DefaultValueElements.Length > 1 ? float.Parse(DefaultValueElements[1].Split('=')[1]) : 0) * 255;
					float b = (DefaultValueElements.Length > 2 ? float.Parse(DefaultValueElements[2].Split('=')[1]) : 0) * 255;

					Writer.WriteParamLiteral("r", ((int)r).ToString());
					Writer.WriteParamLiteral("g", ((int)g).ToString());
					Writer.WriteParamLiteral("b", ((int)b).ToString());
					break;

				case "vector2d struct":
					Writer.WriteParamLiteral("x", (DefaultValueElements[0].Length > 0 ? DefaultValueElements[0].Split('=')[1] : "0.0"));
					Writer.WriteParamLiteral("y", (DefaultValueElements.Length > 1 ? DefaultValueElements[1].Split('=')[1] : "0.0"));
					break;

				case "vector":
					Writer.WriteParamLiteral("x", (DefaultValueElements[0].Length > 0 ? DefaultValueElements[0] : "0.0"));
					Writer.WriteParamLiteral("y",(DefaultValueElements.Length > 1 ? DefaultValueElements[1] : "0.0"));
					Writer.WriteParamLiteral("z",(DefaultValueElements.Length > 2 ? DefaultValueElements[2] : "0.0"));
					break;

				default:
					Writer.WriteParamLiteral("value", DefaultValue);
					break;
			}

			Writer.LeaveObject();
		}

		public override void WritePage(UdnManifest Manifest, string OutputPath)
		{
			using (UdnWriter Writer = new UdnWriter(OutputPath))
			{
				Writer.WritePageHeader(Name, PageCrumbs, Name);
			}
		}

		public override void GatherReferencedPages(List<APIPage> Pages)
		{

		}

		public override void AddToManifest(UdnManifest Manifest)
		{
			Manifest.Add(Name, this);
		}

		public void WritePin(UdnWriter Writer)
		{
			Writer.EnterObject("ActionPinListItem");
			
			Writer.EnterParam("icons");

			var PinType = GetTypeText();
			if (PinType == "exec")
			{
				Writer.WriteRegion("input_exec","");
			}
			else if (bIsArray)
			{
				Writer.WriteRegion("input_array " + PinType, "");
			}
			else
			{
				Writer.WriteRegion("input_variable " + PinType, "");
			}
			Writer.LeaveParam();

			Writer.WriteParamLiteral("name", Name);
			Writer.WriteParamLiteral("type", TypeText);
			Writer.WriteParam("tooltip", Tooltip);
			Writer.WriteParamLiteral("id", GetId());
			Writer.LeaveObject();
		}
	}
}
