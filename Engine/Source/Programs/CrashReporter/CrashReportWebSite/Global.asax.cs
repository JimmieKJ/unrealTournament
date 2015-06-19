// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;
using System.Web.Mvc;
using System.Web.Routing;

namespace Tools.CrashReporter.CrashReportWebSite
{
	/// <summary>
	/// The main control flow of this MVC web site
	/// </summary>
    public class MvcApplication : System.Web.HttpApplication
    {
		/// <summary>
		/// Register all the routes from URLs to controllers
		/// </summary>
		/// <param name="Routes">The mapping of web requests to controllers</param>
        public static void RegisterRoutes( RouteCollection Routes )
        {
			Routes.IgnoreRoute( "{resource}.axd/{*pathInfo}" );

			// Map a route for errors
			Routes.MapRoute(
				"Error",
				"Error/{status}",
				new { controller = "Error", action = "Index", status = UrlParameter.Optional }
			);

			// Map Crashes/Show/Id to the show crash detail page
			Routes.MapRoute(
                "Crashes",														// Route name
                "Crashes/Show/{id}",											// URL with parameters
                new { controller = "Crashes", action = "Show", id = 0 }			// Parameter defaults
            );

			// Map Buggs/Show/Id to the show crash detail page
			Routes.MapRoute(
				"Buggs",														// Route name
				"Buggs/Show/{id}",												// URL with parameters
                new { controller = "Buggs", action = "Show", id = 0 }			// Parameter defaults
			);

			// Edit users and user groups
			Routes.MapRoute(
				"EditUsers",															// Route name
				"Users/Index/{UserGroup}",												// URL with parameters
                new { controller = "Users", action = "Index", UserGroup = "General" }	// Parameter defaults
			);

			// Default mapping for all other requests
			Routes.MapRoute(
                "Default",														// Route name
                "{controller}/{action}/{id}",									// URL with parameters
                new { controller = "Home", action = "Index", id = UrlParameter.Optional } // Parameter defaults
            );
		}

		/// <summary>
		/// Boilerplate web site initialisation
		/// </summary>
        protected void Application_Start()
        {
            AreaRegistration.RegisterAllAreas();

			RegisterRoutes( RouteTable.Routes );
		}
    }
}