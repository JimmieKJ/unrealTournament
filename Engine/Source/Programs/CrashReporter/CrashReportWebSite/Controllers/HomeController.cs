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
    public class HomeController : Controller
    {
        public HomeController()
        {
        }

        public ActionResult Index()
        {
            CrashesViewModel Result = new CrashesViewModel();

            return View("Index", Result);
        }
    }
}