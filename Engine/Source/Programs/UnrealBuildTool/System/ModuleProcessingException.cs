// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;

namespace UnrealBuildTool
{
	public class ModuleProcessingException : Exception
	{
		public ModuleProcessingException(UEBuildModule Module, Exception InnerException):
			base( string.Format("Exception thrown while processing dependent modules of {0}", Module.Name), InnerException )
		{
		}

		public override string ToString()
		{
			// Our build exceptions do not show the callstack so they are more friendly to users.
			if (!string.IsNullOrEmpty(Message))
			{
				var Result = Message;
				if (InnerException != null)
				{
					Result += '\n' + InnerException.ToString();
				}
				return Result;
			}

			return base.ToString();
		}
	};
}
