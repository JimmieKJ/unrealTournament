<%-- // Copyright 1998-2015 Epic Games, Inc. All Rights Reserved. --%>

<%@ Page Title="" Language="C#" MasterPageFile="~/Views/Shared/Site.Master" Inherits="System.Web.Mvc.ViewPage<CrashesViewModel>" %>

<%@ Import Namespace="Tools.CrashReporter.CrashReportWebSite.Models" %>

<asp:Content ID="StyleSheet" ContentPlaceHolderID="CssContent" runat="server">
	<link href="../../Content/Site.css" rel="stylesheet" type="text/css" />
</asp:Content>

<asp:Content ID="TitleContent" ContentPlaceHolderID="TitleContent" runat="server">
Crash Reports
</asp:Content>

<asp:Content ID="ScriptContent"  ContentPlaceHolderID="ScriptContent" runat="server" >

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
				$("#FilterCrashesForm").submit();
			}
		});

		$(document).ready(function ()
		{
			// Select All
			$("#CheckAll").click(function ()
			{
				$("#CrashesForm input:checkbox").attr('checked', true);
				$("#CheckAll").css("color", "Black");
				$("#CheckNone").css("color", "Blue");
				$("#SetStatusInput").unblock();
			});

			// Select None
			$("#CheckNone").click(function ()
			{
				$("#CrashesForm input:checkbox").attr('checked', false);
				$("#CheckAll").css("color", "Blue");
				$("#CheckNone").css("color", "Black");
				$("#SetStatusInput").block({
					message: null
				});
			});

			// Shift Check box
			$("#CrashesForm input:checkbox").shiftcheckbox();

			// Zebra stripes
			$("#CrashesTable tr:nth-child(even)").css("background-color", "#C3CAD0");
			$("#CrashesTable tr:nth-child(odd)").css("background-color", "#eeeeee");
			$.blockUI.defaults.overlayCSS.top = " -4pt";
			$.blockUI.defaults.overlayCSS.left = " -6pt";
			$.blockUI.defaults.overlayCSS.padding = "6pt";
			$.blockUI.defaults.overlayCSS.backgroundColor = "#eeeeee";

			$("#SetStatusInput").block({
				message: null
			});

			$("#CrashesForm input:checkbox").click(function ()
			{
				var n = $("#CrashesForm input:checked").length;
				if (n > 0)
				{
					$("#SetStatusInput").unblock();
				} else
				{
					$("#SetStatusInput").block({
						message: null
					});
				}
			});

			$(".CrashType").click(function ()
			{
				$("#FilterCrashesForm").submit();
			});
		});
	</script>

</asp:Content>

<asp:Content ID="AboveMainContent"  ContentPlaceHolderID="AboveMainContent" runat="server" >
	<div id='SearchForm' style="clear:both">
		
	<% using( Html.BeginForm( "", "Crashes", FormMethod.Get, new { id = "FilterCrashesForm" } ) )
	{ %>
		<%=Html.HiddenFor( u => u.UserGroup )%>
		<%=Html.Hidden( "SortTerm", Model.SortTerm )%>
		<%=Html.Hidden( "SortOrder", Model.SortOrder )%>
		<span style="float:left;">
			<input type="radio" name="CrashType" class="CrashType" value="CrashesAsserts" <%=( Model.CrashType == "CrashesAsserts" ) ? "checked='checked'" : "" %> /> <span title='All Crashes Except Ensures'>Crashes+Asserts</span>
			<input type="radio" name="CrashType" class="CrashType" value="Ensure" <%=( Model.CrashType == "Ensure" ) ? "checked='checked'" : "" %>/> <span title='Only Ensures'>Ensures</span>
			<input type="radio" name="CrashType" class="CrashType" value="Assert" <%=( Model.CrashType == "Assert" ) ? "checked='checked'" : "" %>/> <span title='Only Asserts'>Asserts</span>
			<input type="radio" name="CrashType" class="CrashType" value="Crashes" <%=( Model.CrashType == "Crashes" ) ? "checked='checked'" : "" %>/> <span title='Crashes Except Ensures and Asserts'>Crashes</span>
			<input type="radio" name="CrashType" class="CrashType" value="All" <%=( Model.CrashType == "All" ) ? "checked='checked'" : "" %>/> <span title='All Crashes'>All</span>
		</span>

		<div id="SearchBox">
			<input id="SearchQuery" name="SearchQuery" type="text" value="<%=Model.SearchQuery %>" width="1000" title="For searching for an user use 'user:[name]'" />
			<input type="submit" value="Search" class='SearchButton' />
		</div>
	
		<script>$.datepicker.setDefaults($.datepicker.regional['']);</script>

		<span style="margin-left: 10px; font-weight:bold;">Filter by Date</span>
		<span>From: <input id="dateFromVisible" type="text" class="date" AUTOCOMPLETE=OFF /></span>
		<input id="dateFrom" name="dateFrom" type="hidden" value="<%=Model.DateFrom %>" AUTOCOMPLETE=OFF />
		<span>To: <input id="dateToVisible" type="text" class="date" AUTOCOMPLETE=OFF /></span>
		<input id="dateTo" name="dateTo" type="hidden" value="<%=Model.DateTo %>" AUTOCOMPLETE=OFF />

		<span style="margin-left: 10px; font-weight:bold;">Filter Branch:</span>
		<span><input id="BranchName" name="BranchName" type="text" value="<%=Model.BranchName %>" title="Branch to filter by; prefix with '-' to exclude branch."/></span>
		<span style="margin-left: 10px; font-weight:bold;">Filter Game:</span>
		<span><input id="GameName" name="GameName" type="text" value="<%=Model.GameName %>" title="Game to filter by; prefix with '-' to exclude game."/></span>
	<% } %>
	</div>
</asp:Content>

<asp:Content ID="MainContent" ContentPlaceHolderID="MainContent" runat="server">

<div id='CrashesTableContainer'>
	<div id='UserGroupBar'>
		<%foreach( var GroupCount in Model.GroupCounts ){%>
			<span class=<%if( Model.UserGroup == GroupCount.Key ) { %> "UserGroupTabSelected" <%} else {%> "UserGroupTab"<%} %> id="<%=GroupCount.Key%>Tab">
				<%=Url.UserGroupLink( GroupCount.Key, Model )%> 
				<span class="UserGroupResults">
					(<%=GroupCount.Value%>)
				</span>
			</span>
		<%} %>
	</div>

	<div id='CrashesForm'>
		<form action="/" method="POST" >
			<div style="background-color: #E8EEF4; margin-bottom: 0px; width: 19.7em;">
				<div style="background-color: #E8EEF4; margin-bottom: 10px; width: 19.7em; padding-bottom:0px;">
				<span style="background-color: #E8EEF4; font-size: medium; padding-left: 1em;"><%=Html.ActionLink(
					"Crashes", "Index", "Crashes",
					new 
					{
						SearchQuery = Model.SearchQuery, 
						SortTerm = Model.SortTerm, 
						SortOrder = Model.SortOrder, 
						UserGroup = Model.UserGroup, 
						DateFrom = Model.DateFrom, 
						DateTo = Model.DateTo, 
						CrashType = Model.CrashType,
						RealUserName = Model.RealUserName
					}
					, 
					new { style = "color:black; text-decoration:none;" }
				)%></span>
				<span style="background-color: #C3CAD0; font-size: medium; margin-left: 1em; padding:0 1em;"
					  title="<%= BuggsViewModel.Tooltip %>"><%=Html.ActionLink( "CrashGroups", "Index", "Buggs", 
					new 
					{ 
						SearchQuery = Model.SearchQuery, 
						SortTerm = Model.SortTerm, 
						SortOrder = Model.SortOrder, 
						UserGroup = Model.UserGroup, 
						DateFrom = Model.DateFrom, 
						DateTo = Model.DateTo, 
						CrashType = Model.CrashType
					}
					, 
					new { style = "color:black; text-decoration:none;" } )%></span>
			</div>
			<%Html.RenderPartial( "/Views/Crashes/ViewCrash.ascx" );%>
		</form>
	</div>
</div>

<div class="PaginationBox">
	<%=Html.PageLinks( Model.PagingInfo, i => Url.Action( "", 
		new 
		{ 
			page = i, 
			SearchQuery = Model.SearchQuery, 
			SortTerm = Model.SortTerm, 
			SortOrder = Model.SortOrder, 
			UserGroup = Model.UserGroup, 
			DateFrom = Model.DateFrom, 
			DateTo = Model.DateTo, 
			CrashType = Model.CrashType,
			RealUserName = Model.RealUserName
		} 
	) )%>
	<div id="clear"></div>
</div>

</asp:Content>
