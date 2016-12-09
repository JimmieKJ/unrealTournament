// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System.Linq;
using System.Web.Mvc;
using Tools.CrashReporter.CrashReportWebSite.DataModels.Repositories;
using Tools.CrashReporter.CrashReportWebSite.ViewModels;

namespace Tools.CrashReporter.CrashReportWebSite.Controllers
{
	/// <summary>
	/// The controller to handle associating users with groups.
	/// </summary>
	public class UsersController : Controller
	{
	    private IUnitOfWork _unitOfWork;

        /// <summary>
        /// 
        /// </summary>
        /// <param name="unitOfWork"></param>
	    public UsersController(IUnitOfWork unitOfWork)
	    {
	        _unitOfWork = unitOfWork;
	    }

	    /// <summary>
	    /// Display the main user groups.
	    /// </summary>
	    /// <param name="formData"></param>
	    /// <param name="userName">A form of user data passed up from the client.</param>
	    /// <param name="userGroup">The name of the current user group to display users for.</param>
	    /// <returns>A view to show a list of users in the current user group.</returns>
	    public ActionResult Index( FormCollection formData, string userName, string userGroup = "General" )
        {
            var group = _unitOfWork.UserGroupRepository.First(data => data.Name == userGroup);

            if (!string.IsNullOrWhiteSpace(userName))
            {
                var user = _unitOfWork.UserRepository.GetByUserName(userName);
                if (user != null && group != null)
                {
                    user.UserGroup = group;
                    _unitOfWork.UserRepository.Update(user);
                    _unitOfWork.Save();
                }
            }

	        var formhelper = new FormHelper(Request, formData, "");
	        var skip = (formhelper.Page -1) * formhelper.PageSize;
	        var take = formhelper.PageSize;

			var model = new UsersViewModel
			{
			    UserGroup = userGroup,
			    User = userName,
			    Users = _unitOfWork.UserRepository.ListAll().Where(data => data.UserGroupId == group.Id).OrderBy(data => data.UserName)
                    .Skip(skip)
                    .Take(take)
			        .Select(data => new UserViewModel() {Name = data.UserName, UserGroup = data.UserGroup.Name})
			        .ToList()
			};

		    var groupCounts =
		        _unitOfWork.UserRepository.ListAll()
		            .GroupBy(data => data.UserGroup)
		            .Select(data => new { Key = data.Key, Count = data.Count() }).ToDictionary( groupCount => groupCount.Key.Name, groupCount => groupCount.Count );
            
		    model.GroupSelectList = groupCounts.Select(listItem => new SelectListItem{Selected = listItem.Key == userGroup, Text = listItem.Key, Value = listItem.Key}).ToList();
            model.GroupCounts = groupCounts;
	        model.PagingInfo = new PagingInfo
	        {
	            CurrentPage = formhelper.Page,
	            PageSize = formhelper.PageSize,
	            TotalResults = _unitOfWork.UserRepository.ListAll().Count(data => data.UserGroupId == group.Id)
	        };

			return View( "Index", model );
		}

        /// <summary>
        /// Returns a list of user names and ids as a json for the auto-complete jQuery to parse
        /// </summary>
        /// <param name="userName"></param>
        /// <returns></returns>
	    public JsonResult AutocompleteUser(string userName)
        {
            var users = _unitOfWork.UserRepository.Get(data => data.UserName.Contains(userName)).Take(10).Select(data => new { UserName = data.UserName, Group = data.UserGroup.Name}).ToList();
            return Json(users, JsonRequestBehavior.AllowGet);
        }

        protected override void Dispose(bool disposing)
        {
            _unitOfWork.Dispose();
            base.Dispose(disposing);
        }
	}
}
