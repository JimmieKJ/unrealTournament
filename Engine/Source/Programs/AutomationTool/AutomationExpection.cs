// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace AutomationTool
{
	public class AutomationException : System.Exception
	{
		public AutomationException()
		{

		}

		public AutomationException(string Msg)
			:base(Msg)
		{
		}

		public AutomationException(string Msg, System.Exception InnerException)
			:base(Msg, InnerException)
		{
		}

		public AutomationException(string Format, params object[] Args)
			: base(string.Format(Format, Args)) { }
	}
}
