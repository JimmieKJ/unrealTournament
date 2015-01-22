// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;
using System.Web.Mvc;

namespace Tools.CrashReporter.CrashReportWebSite.Controllers
{
	/// <summary>
	/// A simple controller to return an error view.
	/// </summary>
    public class ErrorController : Controller
    {
		/// <summary>
		/// A simple error page.
		/// </summary>
		/// <returns>A view showing an error.</returns>
        public ActionResult Index()
        {
            return View( "Error" );
        }
    }
}
