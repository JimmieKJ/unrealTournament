// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;

namespace UnrealBuildTool
{
	public class BuildException : Exception
	{
		public BuildException(string MessageFormat, params Object[] MessageObjects):
			base( "ERROR: " + string.Format(MessageFormat, MessageObjects))
		{
		}

		public BuildException(Exception InnerException, string MessageFormat, params Object[] MessageObjects) :
			base("ERROR: " + string.Format(MessageFormat, MessageObjects), InnerException)
		{
		}

		public override string ToString()
		{
			// Our build exceptions do not show the callstack so they are more friendly to users.
			if (Message != null && Message.Length > 0)
			{
				return Message;
			}
			else
			{
				return base.ToString();
			}
		}
	};

	public class MissingModuleException : BuildException
	{
		public MissingModuleException(string InModuleName):
			base( "Couldn't find module rules file for module '{0}'.", InModuleName )
		{
			ModuleName = InModuleName;
		}

		public string ModuleName;
	};
}
