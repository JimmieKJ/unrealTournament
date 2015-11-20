// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using System.Threading;
using System.Diagnostics;
using System.Reflection;
// NOTE: This file is shared with AutomationToolLauncher. It can NOT have any references to non-system libraries

namespace AutomationTool
{
	internal class SharedUtils
	{
		/// <summary>
		/// Parses the command line string and returns an array of passed arguments.
		/// Unlike the default parsing algorithm, this will trean \r\n characters
		/// just like spaces.
		/// </summary>
		/// <returns>List of command line arguments.</returns>
		public static string[] ParseCommandLine()
		{
			var CmdLine = Environment.CommandLine;
			var Args = new List<string>();
			StringBuilder Arg = new StringBuilder(CmdLine.Length);
			bool bQuote = false;
			bool bEscape = false;
			for (int Index = 0; Index < CmdLine.Length; ++Index)
			{
				bool bCanAppend = true;
				char C = CmdLine[Index];
				if (!bQuote && Char.IsWhiteSpace(C))
				{
					// There can be an escape character here when passing a Windows-style path with a trailing backslash (eg. "-MyArg1=D:\UE4\ -MyArg2"). We don't want to push it into the next argument.
					if(bEscape)
					{
						Arg.Append('\\');
						bEscape = false;
					}
					if (Arg.Length > 0)
					{
						Args.Add(Arg.ToString());
						Arg.Clear();
					}
					bCanAppend = false;
				}
				else if (C == '\\' && Index < (CmdLine.Length - 1))
				{
					// Escape character
					bEscape = true;
					bCanAppend = false;
				}
				else if (C == '\"')
				{
					bCanAppend = bEscape;
					if (!bEscape)
					{
						bQuote = !bQuote;
					}
					else
					{
						// Consume the scape character
						bEscape = false;
					}
				}
				if (bCanAppend)
				{
					if (bEscape)
					{
						// Unused escape character.
						Arg.Append('\\');
						bEscape = false;
					}
					Arg.Append(C);
				}
			}
			if (Arg.Length > 0)
			{
				Args.Add(Arg.ToString());
			}
			// This code assumes that the first argument is the exe filename. Remove it.
			Args.RemoveAt(0);
			return Args.ToArray();
		}
	}
}
