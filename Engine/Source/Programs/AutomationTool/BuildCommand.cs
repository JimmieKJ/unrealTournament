// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using AutomationTool;

/// <summary>
/// Base class for buildcommands.
/// </summary>
public abstract class BuildCommand : CommandUtils
{

	#region Interface

	/// <summary>
	/// Build command entry point.  Throws AutomationExceptions on failure.
	/// </summary>
	public abstract void ExecuteBuild();

	#endregion

	/// <summary>
	/// Command entry point.
	/// </summary>
	public void Execute()
	{
		try
		{
			ExecuteBuild();
		}
		catch
		{
			LogError("BUILD FAILED");
			throw;
		}
		Log("BUILD SUCCESSFUL");
	}

	/// <summary>
	/// Executes a new command as a child of another command.
	/// </summary>
	/// <typeparam name="T"></typeparam>
	/// <param name="ParentCommand"></param>
	public static void Execute<T>(BuildCommand ParentCommand) where T : BuildCommand, new()
	{
		T Command = new T();
		if (ParentCommand != null)
		{
			Command.Params = ParentCommand.Params;
		}
		Command.Execute();
	}
}
