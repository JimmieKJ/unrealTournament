// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace AutomationTool
{
	/// <summary>
	/// Base class for buildcommands.
	/// </summary>
	public abstract class BuildCommand : CommandUtils
	{
		/// <summary>
		/// Build command entry point.  Throws AutomationExceptions on failure.
		/// </summary>
		public virtual void ExecuteBuild()
		{
			throw new AutomationException("Either Execute() or ExecuteBuild() should be implemented for {0}", GetType().Name);
		}

		/// <summary>
		/// Command entry point.
		/// </summary>
		public virtual ExitCode Execute()
		{
			ExecuteBuild();
			return ExitCode.Success;
		}

		/// <summary>
		/// Executes a new command as a child of another command.
		/// </summary>
		/// <typeparam name="T"></typeparam>
		/// <param name="ParentCommand"></param>
		public static ExitCode Execute<T>(BuildCommand ParentCommand) where T : BuildCommand, new()
		{
			T Command = new T();
			if (ParentCommand != null)
			{
				Command.Params = ParentCommand.Params;
			}
			return Command.Execute();
		}
	}
}