<%-- // Copyright 1998-2016 Epic Games, Inc. All Rights Reserved. --%>

<%@ Import Namespace="Tools.CrashReporter.CrashReportWebSite.ViewModels" %>

<%@ Page Title="" Language="C#" MasterPageFile="~/Views/Shared/Site.Master" Inherits="System.Web.Mvc.ViewPage<CrashesViewModel>" %>

<asp:Content ID="StyleSheet" ContentPlaceHolderID="CssContent" runat="server">
	<link href="../../Content/Site.css" rel="stylesheet" type="text/css" />
</asp:Content>

<asp:Content ID="TitleContent" ContentPlaceHolderID="TitleContent" runat="server">
[CR] Home
</asp:Content>

<asp:Content ID="ScriptContent"  ContentPlaceHolderID="ScriptContent" runat="server" >
	<script src="../../Scripts/searchCollapse.js"></script>
	<script type="text/javascript">
		$(function () {
			$("#dateFromVisible")
				.datepicker({ maxDate: '+0D' })
				.datepicker('setDate', new Date(parseInt($('#dateFrom').val())));

			$("#dateToVisible")
				.datepicker({ maxDate: '+0D' })
				.datepicker('setDate', new Date(parseInt($('#dateTo').val())));
		});

		$.datepicker.setDefaults({
			onSelect: function () {
				$("#dateFrom").val($("#dateFromVisible").datepicker('getDate').getTime());
				$("#dateTo").val($("#dateToVisible").datepicker('getDate').getTime());
			}
		});

		$(document).ready(function () {
			// Select All
			$("#CheckAll").click(function () {
				$("#CrashesForm input:checkbox").attr('checked', true);
				$("#CheckAll").css("color", "Black");
				$("#CheckNone").css("color", "Blue");
				$("#SetStatusInput").unblock();
			});

			// Select None
			$("#CheckNone").click(function () {
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

			$("#CrashesForm input:checkbox").click(function () {
				var n = $("#CrashesForm input:checked").length;
				if (n > 0) {
					$("#SetStatusInput").unblock();
				} else {
					$("#SetStatusInput").block({
						message: null
					});
				}
			});
		});
	</script>
</asp:Content>

<asp:Content ID="AboveMainContent"  ContentPlaceHolderID="AboveMainContent" runat="server" >
	<div style="clear: both;">
		<small style="color: lightgray;">Generated in <%=Model.GenerationTime%> second(s)</small>
		<br />
		<div id='SearchForm' style="clear: both">
			<div id="TitleBar" class="TitleBar">
				SEARCH OPTIONS
			</div>
			<% using (Html.BeginForm( "", "Crashes", FormMethod.Get, new { id = "FilterCrashesForm" } ))
	  { %>
			<%=Html.HiddenFor( u => u.UserGroup )%>
			<%=Html.Hidden( "SortTerm", Model.SortTerm )%>
			<%=Html.Hidden( "SortOrder", Model.SortOrder )%>

			<div id="Container">
				<table>
					<tr>
						<td rowspan="4">
							<div id="CrashTypeList">
								<p class="SearchTextTitle" title="Press to hide">Show crashes of type</p>
								<ul>
									<li>
										<input type="radio" name="CrashType" value="CrashesAsserts" <%=( Model.CrashType == "CrashesAsserts" ) ? "checked='checked'" : "" %> />
										<span title='All Crashes Except Ensures'>Crashes+Asserts</span></li>
									<li>
										<input type="radio" name="CrashType" value="Ensure" <%=( Model.CrashType == "Ensure" ) ? "checked='checked'" : "" %> />
										<span title='Only Ensures'>Ensures</span></li>
									<li>
										<input type="radio" name="CrashType" value="Assert" <%=( Model.CrashType == "Assert" ) ? "checked='checked'" : "" %> />
										<span title='Only Asserts'>Asserts</span></li>
									<li>
										<input type="radio" name="CrashType" value="Crashes" <%=( Model.CrashType == "Crashes" ) ? "checked='checked'" : "" %> />
										<span title='Crashes Except Ensures and Asserts'>Crashes</span></li>
									<li>
										<input type="radio" name="CrashType" value="All" <%=( Model.CrashType == "All" ) ? "checked='checked'" : "" %> />
										<span title='All Crashes'>All</span></li>
								</ul>
							</div>
						</td>
						<td>
							<p class="SearchTextTitle">Username</p>
						</td>
						<td>
							<input name="UsernameQuery" type="text" value="<%=Model.UsernameQuery %>" title="For searching for an user use 'user:[name]'" />
						</td>

						<td>
							<p class="SearchTextTitle">Call Stack</p>
						</td>
						<td>
							<input name="SearchQuery" type="text" value="<%=Model.SearchQuery%>" title="" />
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
							<input id="dateTo" name="dateTo" type="hidden" value="<%=Model.DateTo %>" autocomplete="OFF"  />
						</td>
					</tr>

					<tr>
						<td>
							<p class="SearchTextTitle">EpicID/MachineID</p>
						</td>
						<td>
							<input name="EpicIdOrMachineQuery" type="text" value="<%=Model.EpicIdOrMachineQuery %>" title="" />
						</td>

						<td>
							<p class="SearchTextTitle">Summary/Message/Description</p>
						</td>
						<td>
							<input name="MessageQuery" type="text" value="<%=Model.MessageQuery%>" title="" />
						</td>

						<td>
							<p class="SearchTextTitle">Filter by Branch</p>
						</td>
						<td>
							<%=Html.DropDownListFor( m=>m.BranchName, Model.BranchNames )%>
						</td>
					</tr>

					<tr>
						<td>
							<p class="SearchTextTitle">Jira</p>
						</td>
						<td>
							<input name="JiraQuery" type="text" value="<%=Model.JiraQuery %>" title="" />
						</td>

						<td>
							<p class="SearchTextTitle">Filter by Game</p>
						</td>
						<td>
							<input name="GameName" type="text" value="<%=Model.GameName %>" title="Use -GameName to exclude" />
						</td>

						<td>
							<p class="SearchTextTitle">Filter by Version</p>
						</td>
						<td>
							<%=Html.DropDownListFor( m=>m.VersionName, Model.VersionNames )%>
						</td>
					</tr>

					<tr>
						<td>
							<p class="SearchTextTitle">Bugg Id</p>
						</td>
						<td>
							<input name="BuggId" type="text" value="<%=Model.BuggId%>" title="" />
						</td>

						<td>
							<p class="SearchTextTitle">Built From CL</p>
						</td>
						<td>
							<input name="BuiltFromCL" type="text" value="<%=Model.BuiltFromCL %>" title="" />
						</td>

						<td>
							<p class="SearchTextTitle">Filter by Platform</p>
						</td>
						<td>
							<%=Html.DropDownListFor( m=>m.PlatformName, Model.PlatformNames )%>
						</td>
					</tr>
                    
                    <tr>
						<td rowspan="1" >
						</td>
                        <td>
							<p class="SearchTextTitle">Filter by Engine Mode</p>
						</td>
						<td>
							<%=Html.DropDownListFor( m=>m.EngineMode, Model.EngineModes )%>
						</td>
                        <td>
							<p class="SearchTextTitle">Filter by Engine Version</p>
						</td>
						<td>
							<%=Html.DropDownListFor( m=>m.EngineVersion, Model.EngineVersions )%>
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
