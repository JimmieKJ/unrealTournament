// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml.Serialization;

namespace AutomationTool
{
	public class MsBuildTaskDefinition : BuildTaskDefinition
	{
		[XmlAttribute]
		public string Project;

		[XmlAttribute]
		public string Arguments;

		[XmlAttribute]
		public string Output;

		/// <summary>
		/// Creates a task from this definition
		/// </summary>
		/// <returns>New instance of a build task</returns>
		public override BuildTask CreateTask()
		{
			return new MsBuildTask(Project, Arguments, Output);
		}
	}

	public class MsBuildTask : BuildTask
	{
		public string Project;
		public string Arguments;
		public string Output;

		public MsBuildTask(string InProject, string InArguments, string InOutput)
		{
			Project = InProject;
			Arguments = InArguments;
			Output = InOutput;
		}

		public override bool Execute(List<string> BuildProducts)
		{
			throw new NotImplementedException();
		}
	}
}
