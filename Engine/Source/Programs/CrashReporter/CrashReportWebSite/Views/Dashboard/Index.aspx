<%-- // Copyright 1998-2015 Epic Games, Inc. All Rights Reserved. --%>

<%@ Page Title="" Language="C#" MasterPageFile="~/Views/Shared/Site.Master" Inherits="System.Web.Mvc.ViewPage<DashboardViewModel>" %>
<%@ Import Namespace="Tools.CrashReporter.CrashReportWebSite.Models" %>

<asp:Content ID="StyleSheet" ContentPlaceHolderID="CssContent" runat="server">
	<link href="../../Content/Site.css" rel="stylesheet" type="text/css" />
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
</asp:Content>

<asp:Content ID="MainContent" ContentPlaceHolderID="MainContent" runat="server">
	<div id='CrashesTableContainer'>
    <div id="DashBoard">

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
                data.addColumn('number', 'Automated Crashes');
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
                data.addColumn('number', 'Automated Crashes');
                data.addColumn('number', 'All Crashes');
                data.addRows([<%=Model.CrashesByDay%>]);
                var chart = new google.visualization.AnnotatedTimeLine(document.getElementById('daily_chart'));
                chart.draw(data, { displayAnnotations: true });
            }
        </script>

        <h2>Crashes By Week</h2>
        <div id='weekly_chart'></div>
        <br />
        
        <h2>Crashes By Day</h2>
        <div id='daily_chart'></div>
    </div>
 </div>

</asp:Content>
