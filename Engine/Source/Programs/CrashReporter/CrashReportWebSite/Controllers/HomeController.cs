// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
using System.Web.Mvc;
using Tools.CrashReporter.CrashReportWebSite.DataModels.Repositories;
using Tools.CrashReporter.CrashReportWebSite.ViewModels;

namespace Tools.CrashReporter.CrashReportWebSite.Controllers
{
	/// <summary>Controls the start page.</summary>
	public class HomeController : Controller
	{
	    private readonly IUnitOfWork _unitOfWork;

        /// <summary>
        /// 
        /// </summary>
	    public HomeController(IUnitOfWork unitOfWork)
	    {
	        _unitOfWork = unitOfWork;
	    }

		/// <summary>
		/// The main view of the home page.
		/// </summary>
		public ActionResult Index()
		{
            using (var logTimer = new FAutoScopedLogTimer( this.GetType().ToString(), bCreateNewLog: true ))
			{
				var result = new CrashesViewModel
				{
				    BranchNames = _unitOfWork.CrashRepository.GetBranchesAsListItems(),
				    VersionNames = _unitOfWork.CrashRepository.GetVersionsAsListItems(),
				    PlatformNames = _unitOfWork.CrashRepository.GetPlatformsAsListItems(),
				    EngineModes = _unitOfWork.CrashRepository.GetEngineModesAsListItems(),
				    EngineVersions = _unitOfWork.CrashRepository.GetEngineVersionsAsListItems(),
				    GenerationTime = logTimer.GetElapsedSeconds().ToString("F2")
				};
			    return View( "Index", result );
			}
		}

        protected override void Dispose(bool disposing)
        {
            _unitOfWork.Dispose();
            base.Dispose(disposing);
        }
	}
}