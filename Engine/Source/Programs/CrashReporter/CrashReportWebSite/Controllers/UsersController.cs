// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Web;
using System.Web.Mvc;

using Tools.CrashReporter.CrashReportWebSite.Models;

namespace Tools.CrashReporter.CrashReportWebSite.Controllers
{
	/// <summary>
	/// The controller to handle associating users with groups.
	/// </summary>
	public class UsersController : Controller
	{
		/// <summary>
		/// Display the main user groups.
		/// </summary>
		/// <param name="UserForms">A form of user data passed up from the client.</param>
		/// <param name="UserGroup">The name of the current user group to display users for.</param>
		/// <returns>A view to show a list of users in the current user group.</returns>
		public ActionResult Index( FormCollection UserForms, string UserGroup )
		{
			CrashRepository Crashes = new CrashRepository();

			// Examine an incoming form for a new user group
			foreach( var FormInstance in UserForms )
			{
				string UserName = FormInstance.ToString();
				string NewUserGroup = UserForms[UserName];
				FRepository.Get( Crashes ).SetUserGroup( UserName, NewUserGroup );
			}

			UsersViewModel Model = new UsersViewModel();

			Model.UserGroup = UserGroup;
			Model.Users = FRepository.Get( Crashes ).GetUserNamesFromGroupName( UserGroup );
			Model.GroupCounts = FRepository.Get( Crashes ).GetCountsByGroup();

			return View( "Index", Model );
		}
	}
}
