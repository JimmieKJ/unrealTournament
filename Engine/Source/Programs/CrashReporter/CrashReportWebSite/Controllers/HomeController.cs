// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Diagnostics;
using System.IO;
using System.Web.Mvc;

using Tools.DotNETCommon.XmlHandler;
using Tools.CrashReporter.CrashReportCommon;
using Tools.CrashReporter.CrashReportWebSite.Models;

namespace Tools.CrashReporter.CrashReportWebSite.Controllers
{
	/// <summary>Controls the start page.</summary>
	public class HomeController : Controller
	{
		/// <summary>
		/// The main view of the home page.
		/// </summary>
		public ActionResult Index()
		{
			using (FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer( this.GetType().ToString(), bCreateNewLog: true ))
			{
				CrashesViewModel Result = new CrashesViewModel();
				Result.GenerationTime = LogTimer.GetElapsedSeconds().ToString( "F2" );
				return View( "Index", Result );
			}
		}
	}
}