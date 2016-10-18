<%-- // Copyright 1998-2016 Epic Games, Inc. All Rights Reserved. --%>

<%@ Page Title="" Language="C#" MasterPageFile="~/Views/Shared/Site.Master" Inherits="System.Web.Mvc.ViewPage<BuggsViewModel>" %>
<%@ Import Namespace="Tools.CrashReporter.CrashReportWebSite.ViewModels" %>

<asp:Content ID="StyleSheet" ContentPlaceHolderID="CssContent" runat="server">
	<link href="../../Content/Site.css" rel="stylesheet" type="text/css" />
</asp:Content>

<asp:Content ID="TitleContent" ContentPlaceHolderID="TitleContent" runat="server">
[CR] Buggs
</asp:Content>

<asp:Content ID="ScriptContent" ContentPlaceHolderID="ScriptContent" runat="server">
	<script src="../../Scripts/searchCollapse.js"></script>
	<script type="text/javascript">
		$(function ()
		{
			$("#dateFromVisible")
				.datepicker({ maxDate: '+0D' })
				.datepicker('setDate', new Date(parseInt($('#dateFrom').val())));

			$("#dateToVisible")
				.datepicker({ maxDate: '+0D' })
				.datepicker('setDate', new Date(parseInt($('#dateTo').val())));

		});

		$.datepicker.setDefaults({
			onSelect: function ()
			{
				$("#dateFrom").val($("#dateFromVisible").datepicker('getDate').getTime());
				$("#dateTo").val($("#dateToVisible").datepicker('getDate').getTime());
			}
		});

		$(document).ready(function ()
		{
			//Shift Check box
			$(":checkbox").shiftcheckbox();

			//Zebrastripes
			$("#CrashesTable tr:nth-child(even)").css("background-color", "#C3CAD0");
			$("#CrashesTable tr:nth-child(odd)").css("background-color", "#eeeeee");

		});
	</script>
</asp:Content>

<asp:Content ID="Content"  ContentPlaceHolderID="AboveMainContent" runat="server" >
	<div style="clear: both;">
		<small style="color: lightgray;">Generated in <%=Model.GenerationTime%> second(s)</small>
		<br />

		<div id='SearchForm' style="clear: both">
			<div id="TitleBar" class="TitleBar">
				SEARCH OPTIONS
			</div>
			<% using (Html.BeginForm( "", "Buggs", FormMethod.Get, new { id = "FilterBuggsForm" } ))
	  { %>
			<%=Html.HiddenFor( u => u.UserGroup )%>
			<%=Html.Hidden( "SortTerm", Model.SortTerm )%>
			<%=Html.Hidden( "SortOrder", Model.SortOrder )%>

			<div id="Container">
				<table>
					<tr>
						<td rowspan="3">
							<div id="CrashTypeList">
								<p class="SearchTextTitle" title="Press to hide">Show buggs of type</p>
								<ul>
									<li>
										<input type="radio" name="CrashType" value="CrashesAsserts" <%=( Model.CrashType == "CrashesAsserts" ) ? "checked='checked'" : "" %> />
										<span title='All Crashes Except Ensures'>Crashes+Asserts</span>
                                    </li>
									<li>
										<input type="radio" name="CrashType" value="Ensure" <%=( Model.CrashType == "Ensure" ) ? "checked='checked'" : "" %> />
										<span title='Only Ensures'>Ensures</span>
                                    </li>
									<li>
										<input type="radio" name="CrashType" value="Assert" <%=( Model.CrashType == "Assert" ) ? "checked='checked'" : "" %> />
										<span title='Only Asserts'>Asserts</span>
									</li>
									<li>
										<input type="radio" name="CrashType" value="Crashes" <%=( Model.CrashType == "Crashes" ) ? "checked='checked'" : "" %> />
										<span title='Crashes Except Ensures and Asserts'>Crashes</span>
                                    </li>
									<li>
										<input type="radio" name="CrashType" value="All" <%=( Model.CrashType == "All" ) ? "checked='checked'" : "" %> />
										<span title='All Crashes'>All</span>
                                    </li>
								</ul>
							</div>
						</td>
						<td>
							<p class="SearchTextTitle">Call Stack</p>
						</td>
						<td>
							<input name="SearchQuery" type="text" value="<%=Model.SearchQuery%>" title=""/>
						</td>
                        <td>
                            <p class="SearchTextTitle">Jira</p>
                        </td>
                        <td>
                            <input name="JiraId" type="text" value="<%=Model.Jira %>" title=""/>
                        </td>
                         <td>
                            <p class="SearchTextTitle">Bugg Id</p>
                        </td>
                        <td>
                            <input name="Bugg Id" type="text" value="<%=Model.BuggId %>" title=""/>
                        </td>
						<td>
							<p class="SearchTextTitle">Filter by Date</p>
						</td>
						<td>
							<script>$.datepicker.setDefaults($.datepicker.regional['']);</script>
							<span class="SearchTextTitle">From:</span>
							<input id="dateFromVisible" type="text" class="date" autocomplete="OFF" style="width:80px" />
							<input id="dateFrom" name="dateFrom" type="hidden" value="<%=Model.DateFrom %>" autocomplete="OFF" />
							<span class="SearchTextTitle">To:</span>
							<input id="dateToVisible" type="text" class="date" autocomplete="OFF" style="width:80px" />
							<input id="dateTo" name="dateTo" type="hidden" value="<%=Model.DateTo %>"autocomplete="OFF" />
						</td>
					</tr>
                    <tr>
						<td>
							<p class="SearchTextTitle">Filter by Version</p>
						</td>
						<td>
							<%=Html.DropDownListFor( m=>m.VersionName, Model.VersionNames )%>
						</td>
                        <td>
							<p class="SearchTextTitle">Filter by Platform</p>
						</td>
						<td>
							<%=Html.DropDownListFor( m=>m.PlatformName, Model.PlatformNames )%>
						</td>
                        <td>
							<p class="SearchTextTitle">Filter by Branch</p>
						</td>
						<td>
							<%=Html.DropDownListFor( m=>m.BranchName, Model.BranchNames )%>
						</td>
					</tr>
					<tr>
						<td colspan="7" >
							<input type="submit" value="Search" class='SearchButton' />
						</td>
					</tr>
				</table>
			</div>
			<% } %>
		</div>
	</div>
</asp:Content>
<asp:Content ID="MainContent" ContentPlaceHolderID="MainContent" runat="server">
	<div id='CrashesTableContainer'>
		<div id='UserGroupBar'>
			<%foreach (var GroupCount in Model.GroupCounts)
	 {%>
				<span class=<%if (Model.UserGroup == GroupCount.Key)
				  { %> "UserGroupTabSelected" <%}
				  else
				  {%> "UserGroupTab"<%} %> id="<%=GroupCount.Key%>Tab">
					<%=Url.UserGroupLink( GroupCount.Key, Model )%> 
				<span class="UserGroupResults">
					(<%=GroupCount.Value%>)
				</span>
			</span>
			<%} %>
		</div>

		<div id='CrashesForm'>
			<form action="/" method="POST">
				<div style="background-color: #E8EEF4; margin-bottom: -5px; width: 19.7em;">
					<span style="background-color: #C3CAD0; font-size: medium; padding: 0 1em;">
						<%=Html.ActionLink( "Crashes", "Index", "Crashes", 		
							new 
							{ 
								SortTerm = Model.SortTerm,
								SortOrder = Model.SortOrder,
								CrashType = Model.CrashType,
								UserGroup = Model.UserGroup,
								SearchQuery = Model.SearchQuery,
								DateFrom = Model.DateFrom,
								DateTo = Model.DateTo,
								VersionName = Model.VersionName,
							}, 
							new { style = "color:black; text-decoration:none;" } )%></span>
					<span style="background-color: #E8EEF4; font-size: medium; padding:0 1em;"
						  title="<%= BuggsViewModel.Tooltip %>">
						<%=Html.ActionLink( "CrashGroups", "Index", "Buggs", 
							new 
							{ 
								SortTerm = Model.SortTerm,
								SortOrder = Model.SortOrder,
								CrashType = Model.CrashType,
								UserGroup = Model.UserGroup,
								SearchQuery = Model.SearchQuery,
								DateFrom = Model.DateFrom,
								DateTo = Model.DateTo,
								VersionName = Model.VersionName,
							}
							, 
							new { style = "color:black; text-decoration:none;" } )%></span>
				</div>
				<% Html.RenderPartial( "/Views/Buggs/ViewBuggs.ascx" ); %>
			</form>
		</div>
	</div>

	<div class="PaginationBox">
		<%=Html.PageLinks( Model.PagingInfo, i => Url.Action( "", 
			new 
			{ 
				page = i, 
				SortTerm = Model.SortTerm,
				SortOrder = Model.SortOrder,
				CrashType = Model.CrashType,
				UserGroup = Model.UserGroup,
				SearchQuery = Model.SearchQuery,
				DateFrom = Model.DateFrom,
				DateTo = Model.DateTo,
				VersionName = Model.VersionName,
			} 
		) )%>
		<div id="clear"></div>
	</div>
</asp:Content>
