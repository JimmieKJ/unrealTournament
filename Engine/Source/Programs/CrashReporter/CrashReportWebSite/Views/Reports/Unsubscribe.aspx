<%-- // Copyright 1998-2016 Epic Games, Inc. All Rights Reserved. --%>

<%@ Page Title="" Language="C#" MasterPageFile="~/Views/Shared/Site.Master" Inherits="System.Web.Mvc.ViewPage<Tools.CrashReporter.CrashReportWebSite.ViewModels.ReportsViewModel>" %>
<%@ Import Namespace="Tools.CrashReporter.CrashReportWebSite.Models" %>

<asp:Content ID="StyleSheet" ContentPlaceHolderID="CssContent" runat="server">
	<link href="../../Content/Site.css" rel="stylesheet" type="text/css" />
</asp:Content>

<asp:Content ID="TitleContent" ContentPlaceHolderID="TitleContent" runat="server">
[CR] Reports - Unsubscribe
</asp:Content>

<asp:Content ID="AboveMainContent" ContentPlaceHolderID="AboveMainContent" runat="server" >
<div style="clear:both;">
    <p style="color: lightgray;">
        Successfully Unsubscribed from weekly Reports Email for the branch <%= ViewData.Eval("BranchName") %> and email address <%= ViewData.Eval("EmailAddress") %>.
    </p><br />
</div>

</asp:Content>
