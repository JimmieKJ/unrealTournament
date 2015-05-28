<%-- // Copyright 1998-2015 Epic Games, Inc. All Rights Reserved. --%>

<%@ Page Title="" Language="C#" MasterPageFile="~/Views/Shared/Site.Master" Inherits="System.Web.Mvc.ViewPage<BuggsViewModel>" %>
<%@ Import Namespace="Tools.CrashReporter.CrashReportWebSite.Models" %>

<asp:Content ID="StyleSheet" ContentPlaceHolderID="CssContent" runat="server">
	<link href="../../Content/Site.css" rel="stylesheet" type="text/css" />
</asp:Content>

<asp:Content ID="TitleContent" ContentPlaceHolderID="TitleContent" runat="server">
	Crash Report: Buggs
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
			$("#FilterBuggsForm").submit();
		}
	});

	$(document).ready(function ()
	{
		//Shift Check box
		$(":checkbox").shiftcheckbox();

		//Zebrastripes
		$("#CrashesTable tr:nth-child(even)").css("background-color", "#C3CAD0");
		$("#CrashesTable tr:nth-child(odd)").css("background-color", "#eeeeee");

		$(".CrashType").click(function ()
		{
			$("#FilterBuggsForm").submit();
		});
	});
</script>
</asp:Content>

<asp:Content ID="AboveMainContent" ContentPlaceHolderID="AboveMainContent" runat="server" >
	<div style="clear:both;"><small style="color: lightgray;">Generated in <%=Model.GenerationTime%> second(s)</small><br /></div>

<div id='SearchForm' style="clear:both;">
<%using( Html.BeginForm( "", "Buggs", FormMethod.Get, new { id = "FilterBuggsForm" } ) )
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

	<div id="SearchBox"><%=Html.TextBox( "SearchQuery", Model.SearchQuery, new { width = "1000" })%><input type="submit" value="Search" class='SearchButton' /></div>

	<script>$.datepicker.setDefaults($.datepicker.regional['']);</script>

	<span style="margin-left: 10px; font-weight: bold;">Filter by Date </span>

	<span>From: 
	<input id="dateFromVisible" type="text" class="date" autocomplete="OFF" /></span>
	<input id="dateFrom" name="dateFrom" type="hidden" value="<%=Model.DateFrom %>" autocomplete="OFF" />

	<span>To: 
	<input id="dateToVisible" type="text" class="date" autocomplete="OFF" /></span>
	<input id="dateTo" name="dateTo" type="hidden" value="<%=Model.DateTo %>" autocomplete="OFF" />


	<span style="margin-left: 10px; font-weight:bold;">Filter Build Version:</span>
	<span><input id="BuildVersion" name="BuildVersion" type="text" value="<%=Model.BuildVersion%>" AUTOCOMPLETE=OFF title="Build version to filter by; eg: 4.4.0 or 4.3"/></span>

<%--	<select id="BuildVersionVisible" name="BuildVersionVisible">
		<option selected="selected" value=""></option>
		<%foreach( var BuildVersion in Model.BuildVersions )
		{%>
			<option value="<%=BuildVersion%>"><%=BuildVersion%></option>
		<%}
		%>
	</select>--%>

<%} %>
</div>
</asp:Content>

<asp:Content ID="MainContent" ContentPlaceHolderID="MainContent" runat="server">
	<div id='CrashesTableContainer'>
		<div id='UserGroupBar'>
			<%foreach( var GroupCount in Model.GroupCounts )
			{%>
				<span class=<%if( Model.UserGroup == GroupCount.Key ){ %> "UserGroupTabSelected" <%} else {%> "UserGroupTab"<%} %> id="<%=GroupCount.Key%>Tab">
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
								SearchQuery = Model.SearchQuery, 
								SortTerm = Model.SortTerm, 
								SortOrder = Model.SortOrder, 
								UserGroup = Model.UserGroup, 
								DateFrom = Model.DateFrom, 
								DateTo = Model.DateTo, 
								BuildVersion = Model.BuildVersion,
								CrashType = Model.CrashType
							}
							, 
							new { style = "color:black; text-decoration:none;" } )%></span>
					<span style="background-color: #E8EEF4; font-size: medium; padding:0 1em;"
						  title="<%= BuggsViewModel.Tooltip %>">
						<%=Html.ActionLink( "CrashGroups", "Index", "Buggs", 
							new 
							{ 
								SearchQuery = Model.SearchQuery, 
								SortTerm = Model.SortTerm, 
								SortOrder = Model.SortOrder, 
								UserGroup = Model.UserGroup, 
								DateFrom = Model.DateFrom, 
								DateTo = Model.DateTo, 
								BuildVersion = Model.BuildVersion,
								CrashType = Model.CrashType
							}
							, 
							new { style = "color:black; text-decoration:none;" } )%></span>
				</div>
				<% Html.RenderPartial("/Views/Buggs/ViewBuggs.ascx"); %>
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
				BuildVersion = Model.BuildVersion,
				CrashType = Model.CrashType
			} 
		) )%>
		<div id="clear"></div>
	</div>
</asp:Content>
