// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Diagnostics;
using System.Linq;

namespace UnrealBuildTool
{
	public enum LocalizationTargetDescriptorLoadingPolicy
	{
		Never,
		Always,
		Editor,
		Game,
		PropertyNames,
		ToolTips,
	};

	[DebuggerDisplay("Name={Name}")]
	public class LocalizationTargetDescriptor
	{
		// Name of this target
		public readonly string Name;

		// When should the localization data associated with a target should be loaded?
		public LocalizationTargetDescriptorLoadingPolicy LoadingPolicy;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="InName">Name of the target</param>
		/// <param name="InLoadingPolicy">When should the localization data associated with a target should be loaded?</param>
		public LocalizationTargetDescriptor(string InName, LocalizationTargetDescriptorLoadingPolicy InLoadingPolicy)
		{
			Name = InName;
			LoadingPolicy = InLoadingPolicy;
		}

		/// <summary>
		/// Constructs a LocalizationTargetDescriptor from a Json object
		/// </summary>
		/// <param name="InObject"></param>
		/// <returns>The new localization target descriptor</returns>
		public static LocalizationTargetDescriptor FromJsonObject(JsonObject InObject)
		{
			return new LocalizationTargetDescriptor(InObject.GetStringField("Name"), InObject.GetEnumField<LocalizationTargetDescriptorLoadingPolicy>("LoadingPolicy"));
		}

		/// <summary>
		/// Write this target to a JsonWriter
		/// </summary>
		/// <param name="Writer">Writer to output to</param>
		void Write(JsonWriter Writer)
		{
			Writer.WriteObjectStart();
			Writer.WriteValue("Name", Name);
			Writer.WriteValue("LoadingPolicy", LoadingPolicy.ToString());
			Writer.WriteObjectEnd();
		}

		/// <summary>
		/// Write an array of target descriptors
		/// </summary>
		/// <param name="Writer">The Json writer to output to</param>
		/// <param name="Name">Name of the array</param>
		/// <param name="Targets">Array of targets</param>
		public static void WriteArray(JsonWriter Writer, string Name, LocalizationTargetDescriptor[] Targets)
		{
			if (Targets.Length > 0)
			{
				Writer.WriteArrayStart(Name);
				foreach (LocalizationTargetDescriptor Target in Targets)
				{
					Target.Write(Writer);
				}
				Writer.WriteArrayEnd();
			}
		}
	}
}
