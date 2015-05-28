<%-- // Copyright 1998-2015 Epic Games, Inc. All Rights Reserved. --%>

<%@ Page Title="" Language="C#" MasterPageFile="~/Views/Shared/Site.Master" Inherits="System.Web.Mvc.ViewPage<ReportsViewModel>" %>
<%@ Import Namespace="Tools.CrashReporter.CrashReportWebSite.Models" %>

<asp:Content ID="StyleSheet" ContentPlaceHolderID="CssContent" runat="server">
	<link href="../../Content/Site.css" rel="stylesheet" type="text/css" />
</asp:Content>

<asp:Content ID="TitleContent" ContentPlaceHolderID="TitleContent" runat="server">
	Crash Reports
</asp:Content>

<asp:Content ID="ScriptContent" ContentPlaceHolderID="ScriptContent" runat="server" >
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
				$("#ReporsForm").submit();
			}
		});

	</script>
</asp:Content>

<asp:Content ID="AboveMainContent" ContentPlaceHolderID="AboveMainContent" runat="server" >
<div style="clear:both;"><small style="color: lightgray;">Generated in <%=Model.GenerationTime%> second(s)</small><br /></div>

<div id='SearchForm' style="clear:both;">
<%using( Html.BeginForm( "", "Reports", FormMethod.Get, new { id = "ReporsForm" } ) )
{ %>
	<script>$.datepicker.setDefaults($.datepicker.regional['']);</script>

	<span style="margin-left: 10px; font-weight: bold;">Filter by Date </span>

	<span>From: 
	<input id="dateFromVisible" type="text" class="date" autocomplete="OFF" /></span>
	<input id="dateFrom" name="dateFrom" type="hidden" value="<%=Model.DateFrom %>" autocomplete="OFF" />

	<span>To: 
	<input id="dateToVisible" type="text" class="date" autocomplete="OFF" /></span>
	<input id="dateTo" name="dateTo" type="hidden" value="<%=Model.DateTo %>" autocomplete="OFF" />

	<%} %>

</div>
</asp:Content>

<asp:Content ID="ReportsContent" ContentPlaceHolderID="ReportsContent" runat="server">
	<div>
		<div>
			<% Html.RenderPartial("/Views/Reports/ViewReports.ascx"); %>
		</div>
	</div>

</asp:Content>
