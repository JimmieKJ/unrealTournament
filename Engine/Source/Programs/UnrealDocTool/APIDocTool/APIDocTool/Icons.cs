// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace APIDocTool
{
	public class Icon
	{
		public readonly string Path;
		public readonly string AltText;
		public readonly int Size;

		public Icon(string InPath, string InAltText)
			: this(InPath, InAltText, -1)
		{
		}

		public Icon(string InPath, string InAltText, int InSize)
		{
			Path = InPath;
			AltText = InAltText;
			Size = InSize;
		}

		public override string ToString()
		{
			return AltText;
		}
	};

	public class MetadataIcon
	{
		public readonly Icon Icon;
		public readonly string[] Tags;
		public readonly string[] InvariantTags;

		public MetadataIcon(Icon InIcon, string[] InTags)
		{
			Icon = InIcon;
			Tags = InTags;
			InvariantTags = InTags.Select(x => x.ToLowerInvariant()).ToArray();
		}
	};

	public static class Icons
	{
		//// Class icons /////////////////////////////////////////////////////////////////////////////////////////
		
		public static readonly Icon[] Class =
		{
			new Icon("%ROOT%/api_class_public.png", "Public class"),
			new Icon("%ROOT%/api_class_protected.png", "Protected class"),
			new Icon("%ROOT%/api_class_protected.png", "Private class"),
		};

		public static readonly Icon ReflectedClass = new Icon("%ROOT%/api_class_meta.png", "UClass");

		public static readonly MetadataIcon[] ClassMetadataIcons =
		{
			new MetadataIcon(new Icon("%ROOT%/api_class_meta_declaration.png", "Declaration"), new string[]{ "Abstract", "ConversionRoot", "MinimalAPI", "DependsOn", "HeaderGroup" }),
			new MetadataIcon(new Icon("%ROOT%/api_class_meta_blueprint.png", "Blueprint"), new string[]{ "Blueprintable", "BlueprintType" }),
			new MetadataIcon(new Icon("%ROOT%/api_class_meta_editor.png", "Editor"), new string[]{ "Placeable", "NotPlaceable", "ClassGroup", "AutoExpandCategories", "ShowCategories", "HideCategories", "Meta" }),
			new MetadataIcon(new Icon("%ROOT%/api_class_meta_serialization.png", "Serialization"), new string[]{ "Transient", "Config" }),
			new MetadataIcon(new Icon("%ROOT%/api_class_meta_other.png", "Other"), new string[]{ }),
		};

		//// Struct icons ////////////////////////////////////////////////////////////////////////////////////////

		public static readonly Icon[] Struct =
		{
			new Icon("%ROOT%/api_struct_public.png", "Public struct"),
			new Icon("%ROOT%/api_struct_protected.png", "Protected struct"),
			new Icon("%ROOT%/api_struct_protected.png", "Private struct"),
		};

		public static readonly Icon ReflectedStruct = new Icon("%ROOT%/api_struct_meta.png", "UStruct");

		public static readonly MetadataIcon[] StructMetadataIcons =
		{
			new MetadataIcon(new Icon("%ROOT%/api_struct_meta_declaration.png", "Declaration"), new string[]{ "Atomic", "Immutable", "NoExport" }),
			new MetadataIcon(new Icon("%ROOT%/api_struct_meta_blueprint.png", "Blueprint"), new string[]{ "BlueprintType" }),
			new MetadataIcon(new Icon("%ROOT%/api_struct_meta_serialization.png", "Serialization"), new string[]{ "Transient", "Config" }),
			new MetadataIcon(new Icon("%ROOT%/api_struct_meta_other.png", "Other"), new string[]{ }),
		};

		public static readonly Icon OtherStructIconTag = new Icon("%ROOT%/api_struct_meta_other.png", "Other metadata");

		//// Enum icons ////////////////////////////////////////////////////////////////////////////////////////

		public static readonly Icon[] Enum =
		{
			new Icon("%ROOT%/api_enum_public.png", "Public enum"),
			new Icon("%ROOT%/api_enum_protected.png", "Protected enum"),
			new Icon("%ROOT%/api_enum_protected.png", "Private enum"),
		};

		public static readonly Icon ReflectedEnum = new Icon("%ROOT%/api_enum_meta.png", "UEnum");

		public static readonly MetadataIcon[] EnumMetadataIcons =
		{
			new MetadataIcon(new Icon("%ROOT%/api_enum_meta_blueprint.png", "Blueprint"), new string[]{ "BlueprintType" }),
			new MetadataIcon(new Icon("%ROOT%/api_enum_meta_other.png", "Other"), new string[]{ }),
		};

		//// Function icons ////////////////////////////////////////////////////////////////////////////////////////

		public static readonly Icon[] Function =
		{
			new Icon("%ROOT%/api_function_public.png", "Public function"),
			new Icon("%ROOT%/api_function_protected.png", "Protected function"),
			new Icon("%ROOT%/api_function_protected.png", "Private function"),
		};

		public static readonly Icon ReflectedFunction = new Icon("%ROOT%/api_function_meta.png", "UFunction");
		public static readonly Icon StaticFunction = new Icon("%ROOT%/api_function_static.png", "Static");
		public static readonly Icon VirtualFunction = new Icon("%ROOT%/api_function_virtual.png", "Virtual");

		public static readonly MetadataIcon[] FunctionMetadataIcons =
		{
			new MetadataIcon(new Icon("%ROOT%/api_function_meta_blueprint.png", "Blueprint"), new string[]{ "BlueprintCallable", "BlueprintPure", "BlueprintAuthorityOnly", "Category", "Meta", "BlueprintImplementableEvent" }),
			new MetadataIcon(new Icon("%ROOT%/api_function_meta_serialization.png", "Serialization"), new string[]{ "Server", "Client", "Reliable", "Unreliable", "NetMulticast" }),
			new MetadataIcon(new Icon("%ROOT%/api_function_meta_other.png", "Other"), new string[]{ }),
		};

		//// Variable icons ////////////////////////////////////////////////////////////////////////////////////////

		public static readonly Icon[] Variable =
		{
			new Icon("%ROOT%/api_variable_public.png", "Public variable"),
			new Icon("%ROOT%/api_variable_protected.png", "Protected variable"),
			new Icon("%ROOT%/api_variable_protected.png", "Private variable"),
		};

		public static readonly Icon ReflectedVariable = new Icon("%ROOT%/api_variable_meta.png", "UProperty");
		public static readonly Icon StaticVariable = new Icon("%ROOT%/api_variable_static.png", "Static");

		public static readonly MetadataIcon[] VariableMetadataIcons =
		{
			new MetadataIcon(new Icon("%ROOT%/api_variable_meta_editor.png", "Editor"), new string[]{ "Category", "EditAnywhere", "EditDefaultsOnly", "AdvancedDisplay", "Interp", "Meta" }),
			new MetadataIcon(new Icon("%ROOT%/api_variable_meta_blueprint.png", "Blueprint"), new string[]{ "BlueprintAssignable", "BlueprintReadOnly", "BlueprintReadWrite" }),
			new MetadataIcon(new Icon("%ROOT%/api_variable_meta_serialization.png", "Serialization"), new string[]{ "Transient", "Config", "GlobalConfig", "Replicated", "SaveGame" }),
			new MetadataIcon(new Icon("%ROOT%/api_variable_meta_other.png", "Other"), new string[]{ }),
		};

		//// Blueprint Variable type icons 
		public static readonly Dictionary<string, Icon> VariablePinIcons = new Dictionary<string, Icon>
		{
			{ "bool", new Icon("%ROOT%/bp_api_pin_bool.png", "Boolean") },
			{ "byte", new Icon("%ROOT%/bp_api_pin_byte.png", "Byte") },
			{ "class", new Icon("%ROOT%/bp_api_pin_class.png", "Class") },
			{ "delegate", new Icon("%ROOT%/bp_api_pin_delegate.png", "Delegate") },
			{ "enum", new Icon("%ROOT%/bp_api_pin_byte.png", "Enum") },
			{ "exec", new Icon("%ROOT%/bp_api_pin_exec.png", "Exec") },
			{ "float", new Icon("%ROOT%/bp_api_pin_float.png", "Float") },
			{ "int", new Icon("%ROOT%/bp_api_pin_int.png", "Integer") },
			{ "interface", new Icon("%ROOT%/bp_api_pin_interface.png", "Interface") },
			{ "name", new Icon("%ROOT%/bp_api_pin_name.png", "Name") },
			{ "object", new Icon("%ROOT%/bp_api_pin_object.png", "Object") },
			{ "rotator", new Icon("%ROOT%/bp_api_pin_rotator.png", "Rotator") },
			{ "string", new Icon("%ROOT%/bp_api_pin_string.png", "String") },
			{ "struct", new Icon("%ROOT%/bp_api_pin_struct.png", "Struct") },
			{ "text", new Icon("%ROOT%/bp_api_pin_text.png", "Text") },
			{ "transform", new Icon("%ROOT%/bp_api_pin_transform.png", "Transform") },
			{ "vector", new Icon("%ROOT%/bp_api_pin_vector.png", "Vector") },
			{ "wildcard", new Icon("%ROOT%/bp_api_pin_wildcard.png", "Wildcard") },
		};

		//// Blueprint Array variable type icons 
		public static readonly Dictionary<string, Icon> ArrayVariablePinIcons = new Dictionary<string, Icon>
		{
			{ "bool", new Icon("%ROOT%/bp_api_pin_array_bool.png", "Boolean Array") },
			{ "byte", new Icon("%ROOT%/bp_api_pin_array_byte.png", "Byte Array") },
			{ "class", new Icon("%ROOT%/bp_api_pin_array_class.png", "Class Array") },
			{ "enum", new Icon("%ROOT%/bp_api_pin_array_byte.png", "Enum Array") },
			{ "float", new Icon("%ROOT%/bp_api_pin_array_float.png", "Float Array") },
			{ "int", new Icon("%ROOT%/bp_api_pin_array_int.png", "Integer Array") },
			{ "name", new Icon("%ROOT%/bp_api_pin_array_name.png", "Name Array") },
			{ "object", new Icon("%ROOT%/bp_api_pin_array_object.png", "Object Array") },
			{ "rotator", new Icon("%ROOT%/bp_api_pin_array_rotator.png", "Rotator Array") },
			{ "string", new Icon("%ROOT%/bp_api_pin_array_string.png", "String Array") },
			{ "struct", new Icon("%ROOT%/bp_api_pin_array_struct.png", "Struct Array") },
			{ "text", new Icon("%ROOT%/bp_api_pin_array_text.png", "Text Array") },
			{ "transform", new Icon("%ROOT%/bp_api_pin_array_transform.png", "Transform Array") },
			{ "vector", new Icon("%ROOT%/bp_api_pin_array_vector.png", "Vector Array") },
			{ "wildcard", new Icon("%ROOT%/bp_api_pin_array_wildcard.png", "Wildcard Array") },
		};

		//// Meta icons ///////////////////////////////////////////////////////////////////////////////////////////

		public static readonly MetadataIcon[] MetaMetadataIcons =
		{
			new MetadataIcon(new Icon("%ROOT%/api_variable_meta_other.png", "Other"), new string[]{ }),
		};
	
		//// Other ////////////////////////////////////////////////////////////////////////////////////////

		public static Icon GetMetadataIcon(string Type, string Tag)
		{
			MetadataIcon[] MetadataIcons = GetMetadataIconsForType(Type);
			return GetMetadataIcon(MetadataIcons, Tag);
		}

		public static Icon GetMetadataIcon(MetadataIcon[] MetadataIcons, string Tag)
		{
			int Idx = 0;
			while(Idx + 1 < MetadataIcons.Length && !MetadataIcons[Idx].InvariantTags.Contains(Tag.ToLowerInvariant())) Idx++;

			return MetadataIcons[Idx].Icon;
		}

		public static List<Icon> GetMetadataIcons(string Type, IEnumerable<string> Tags)
		{
			// Build a list of invariant tags
			List<string> RemainingTags = new List<string>(Tags.Select(x => x.ToLowerInvariant()).Distinct());

			// Get the icons for this type
			MetadataIcon[] MetadataIcons = GetMetadataIconsForType(Type);

			// Build a list of tags for each icon
			string[] TagList = new string[MetadataIcons.Length];
			for(int Idx = 0; Idx < MetadataIcons.Length; Idx++)
			{
				MetadataIcon Icon = MetadataIcons[Idx];
				for(int TagIdx = 0; TagIdx < Icon.InvariantTags.Length; TagIdx++)
				{
					int RemainingIdx = RemainingTags.IndexOf(Icon.InvariantTags[TagIdx]);
					if(RemainingIdx != -1)
					{
						TagList[Idx] = ((TagList[Idx] == null)? "" : TagList[Idx] + ", ") + Icon.Tags[TagIdx];
						RemainingTags.RemoveAt(RemainingIdx);
					}
				}
			}

			// Append all the remaining tags to the list
			if(RemainingTags.Count > 0)
			{
				RemainingTags.Sort((x, y) => String.Compare(x, y));

				string OtherTagList = TagList[TagList.Length - 1];
				if(OtherTagList != null) OtherTagList += ", ";
				TagList[TagList.Length - 1] = OtherTagList + String.Join(", ", RemainingTags);
			}

			// Build a list of icons which have tags
			List<Icon> Icons = new List<Icon>();
			for (int Idx = 0; Idx < MetadataIcons.Length; Idx++)
			{
				if (TagList[Idx] != null)
				{
					Icons.Add(new Icon(MetadataIcons[Idx].Icon.Path, TagList[Idx]));
				}
			}
			return Icons;
		}

		public static MetadataIcon[] GetMetadataIconsForType(string Type)
		{
			switch (Type)
			{
				case "UCLASS":
					return ClassMetadataIcons;
				case "USTRUCT":
					return StructMetadataIcons;
				case "UENUM":
					return EnumMetadataIcons;
				case "UFUNCTION":
					return FunctionMetadataIcons;
				case "UPROPERTY":
					return VariableMetadataIcons;
				default:
					throw new NotImplementedException();
			}
		}
	}
}
