<%-- // Copyright 1998-2015 Epic Games, Inc. All Rights Reserved. --%>

<%@ Page Title="" Language="C#" MasterPageFile="~/Views/Shared/Site.Master" Inherits="System.Web.Mvc.ViewPage<DashboardViewModel>" %>
<%@ Import Namespace="Tools.CrashReporter.CrashReportWebSite.Models" %>

<asp:Content ID="StyleSheet" ContentPlaceHolderID="CssContent" runat="server">
	<link href="../../Content/Site.css" rel="stylesheet" type="text/css" />

<style>
table
{
	border: 0px solid black;
	color: white;
	padding: 4px;
}

</style>

</asp:Content>

<asp:Content ID="TitleContent" ContentPlaceHolderID="TitleContent" runat="server">
	Crash Reports
</asp:Content>

<asp:Content ID="ScriptContent" ContentPlaceHolderID="ScriptContent" runat="server" >
	<script type="text/javascript">

		$(document).ready(function ()
		{
		});
			 
	</script>
</asp:Content>

<asp:Content ID="AboveMainContent" ContentPlaceHolderID="AboveMainContent" runat="server" >
	<div style="clear:both;"><small style="color: lightgray;">Generated in <%=Model.GenerationTime%> second(s)</small><br /></div>
</asp:Content>

<asp:Content ID="MainContent" ContentPlaceHolderID="MainContent" runat="server">

		<script type='text/javascript' src='http://www.google.com/jsapi'></script>
		<script type='text/javascript'>
			google.load('visualization', '1', { 'packages': ['annotatedtimeline'] });
			google.setOnLoadCallback(drawChart);
			function drawChart()
			{
				var data = new google.visualization.DataTable();
				data.addColumn('date', 'Date');
				data.addColumn('number', 'General Crashes');
				data.addColumn('number', 'Coder Crashes');
				data.addColumn('number', 'EngineQA Crashes');
				data.addColumn('number', 'GameQA Crashes');
				data.addColumn('number', 'Anonymous Crashes');
				data.addColumn('number', 'All Crashes');
				data.addRows([<%=Model.CrashesByWeek%>]);
				var chart = new google.visualization.AnnotatedTimeLine(document.getElementById('weekly_chart'));
				chart.draw(data, { displayAnnotations: true });
			}
		</script>

		<script type='text/javascript'>
			google.load('visualization', '1', { 'packages': ['annotatedtimeline'] });
			google.setOnLoadCallback(drawChart);
			function drawChart()
			{
				var data = new google.visualization.DataTable();
				data.addColumn('date', 'Date');
				data.addColumn('number', 'General Crashes');
				data.addColumn('number', 'Coder Crashes');
				data.addColumn('number', 'EngineQA Crashes');
				data.addColumn('number', 'GameQA Crashes');
				data.addColumn('number', 'Anonymous Crashes');
				data.addColumn('number', 'All Crashes');
				data.addRows([<%=Model.CrashesByDay%>]);
				var chart = new google.visualization.AnnotatedTimeLine(document.getElementById('daily_chart'));
				chart.draw(data, { displayAnnotations: true });
			}
		</script>

		<script type='text/javascript'>
			google.load('visualization', '1', { 'packages': ['annotatedtimeline'] });
			google.setOnLoadCallback(drawChart);
			function drawChart()
			{
				var data = new google.visualization.DataTable();
				data.addColumn('date', 'Date');
				data.addColumn('number', 'Daily New Buggs');
				data.addRows([<%=Model.BuggsByDay%>]);
				var chart = new google.visualization.AnnotatedTimeLine(document.getElementById('bugg_daily_chart'));
				chart.draw(data, { displayAnnotations: true });
			}
		</script>

		<table>
			<tr>
				<td>
				Crashes By Week
				</td>
			</tr>
			<tr>
				<td>
					<div id="weekly_chart" style="height: 256px; width: 1600px;"></div>
				</td>
			</tr>
			<tr>
				<td>
				Crashes By Day
				</td>
			<tr>
				<td>
					<div id="daily_chart" style="height: 256px; width: 1600px;"></div>
				</td>
			</tr>
			<tr>
				<td>
				Buggs By Day
				</td>
			</tr>
			<tr>
				<td>
					<div id="bugg_daily_chart" style="height: 256px; width: 1600px;"></div>
				</td>
			</tr>
		</table>	

</asp:Content>
