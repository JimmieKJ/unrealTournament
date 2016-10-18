// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Linq;
using System.Web.Mvc;
using Tools.CrashReporter.CrashReportWebSite.DataModels;
using Tools.CrashReporter.CrashReportWebSite.DataModels.Repositories;
using Tools.CrashReporter.CrashReportWebSite.Models;

namespace Tools.CrashReporter.CrashReportWebSite.Controllers
{
	/// <summary>
	/// The controller to handle associating users with groups.
	/// </summary>
	public class UsersController : Controller
	{
	    private IUnitOfWork _unitOfWork;

	    public UsersController(IUnitOfWork unitOfWork)
	    {
	        _unitOfWork = unitOfWork;
	    }
        
		/// <summary>
		/// Display the main user groups.
		/// </summary>
		/// <param name="UserForms">A form of user data passed up from the client.</param>
		/// <param name="UserGroup">The name of the current user group to display users for.</param>
		/// <returns>A view to show a list of users in the current user group.</returns>
		public ActionResult Index( FormCollection UserForms, string UserGroup )
		{
			// Examine an incoming form for a new user group
			foreach( var FormInstance in UserForms )
			{
				var UserName = FormInstance.ToString();
				var NewUserGroup = UserForms[UserName];
			}

			UsersViewModel Model = new UsersViewModel();

		    var userGroup = _unitOfWork.UserGroupRepository.First(data => data.Name == UserGroup);

			Model.UserGroup = UserGroup;
		    Model.Users =
		        new HashSet<string>(
		            _unitOfWork.UserRepository.ListAll()
		                .Where(data => data.UserGroupId == userGroup.Id)
		                .Select(data => data.UserName));

		    var groupCounts =
		        _unitOfWork.UserRepository.ListAll()
		            .GroupBy(data => data.UserGroup)
		            .Select(data => new {Key = data.Key, Count = data.Count()}).ToDictionary(groupCount => groupCount.Key.Name, groupCount => groupCount.Count);;
            
            // Add in all groups, even though there are no crashes associated
			//IEnumerable<string> UserGroups = ( from UserGroupDetail in _entities.UserGroups select UserGroupDetail.Name );
            //foreach( string UserGroupName in UserGroups )
            //{
            //    if( !groupCounts.Keys.Contains( UserGroupName ) )
            //    {
            //        groupCounts[UserGroupName] = 0;
            //    }
            //}

            Model.GroupCounts = groupCounts;

			return View( "Index", Model );
		}
	}
}
