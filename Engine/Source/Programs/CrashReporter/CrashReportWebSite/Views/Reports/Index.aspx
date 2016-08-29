<%-- // Copyright 1998-2016 Epic Games, Inc. All Rights Reserved. --%>

<%@ Page Title="" Language="C#" MasterPageFile="~/Views/Shared/Site.Master" Inherits="System.Web.Mvc.ViewPage<ReportsViewModel>" %>
<%@ Import Namespace="Tools.CrashReporter.CrashReportWebSite.ViewModels" %>

<asp:Content ID="StyleSheet" ContentPlaceHolderID="CssContent" runat="server">
	<link href="../../Content/Site.css" rel="stylesheet" type="text/css" />
    <link href="../../Content/ReportsStyle.css" rel="stylesheet" type="text/css" />
    
    <style>
        table, table tr, table th, table td
        {
	        border: solid;
	        border-width: 1px;

	        color: white;
	        background-color: black;

	        font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
	        font-size: 12px;

	        padding: 2px;
	        margin: 2px;
        }
    </style>
</asp:Content>

<asp:Content ID="TitleContent" ContentPlaceHolderID="TitleContent" runat="server">
[CR] Reports
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
			}
		});

		function SubscribeToEmail() {
		    var url = "/Reports/SubscribeToEmail";
		    var formElement = $("#ReportsEmailForm");
		    var branchName = formElement.find("#BranchFilter option:selected").val();
		    var emailAddress = $("#subscribeEmail").val();

		    $.ajax({
		        type: "POST",
		        url: url,
		        data: { branchName: branchName, emailAddress: emailAddress },
		        success: function (response) {
		                $("#SignupResponsePartial").html(response);
		            }
		        }
		    );
		}
	</script>
</asp:Content>

<asp:Content ID="AboveMainContent" ContentPlaceHolderID="AboveMainContent" runat="server" >
<div style="clear:both;"><small style="color: lightgray;">Generated in <%=Model.GenerationTime%> second(s)</small><br /></div>
<div id='SearchForm' style="clear:both;">
    <div id="ReportsEmailForm" style="float:right;">
        <span class="SearchTextTitle">Sign up to weekly reports e-mail</span>
        <p>
            <span class="SearchTextTitle">Select the report branch</span>
        </p>
        <p id="BranchFilter"><%=Html.DropDownListFor( m => m.BranchName, Model.BranchNames )%></p>
        <p>
            <span class="SearchTextTitle">Enter email : </span>
            <input id="subscribeEmail" type="text" class="date" autocomplete="OFF" style="width:200px" />
        </p>
        <input type="button" value="Email" class='SearchButton' onclick="SubscribeToEmail()"/>
    </div>
<%using( Html.BeginForm( "", "Reports", FormMethod.Get, new { id = "ReportsForm" } ) )
{ %>
	<script>$.datepicker.setDefaults($.datepicker.regional['']);</script>
    
	<p class="SearchTextTitle">Filter by Date</p>
    
	<p>
		<span class="SearchTextTitle">From:</span>
		<input id="dateFromVisible" type="text" class="date" autocomplete="OFF" style="width:80px" />
		<input id="dateFrom" name="dateFrom" type="hidden" value="<%=Model.DateFrom %>" autocomplete="OFF" />
		<span class="SearchTextTitle">To:</span>
		<input id="dateToVisible" type="text" class="date" autocomplete="OFF" style="width:80px" />
		<input id="dateTo" name="dateTo" type="hidden" value="<%=Model.DateTo %>"autocomplete="OFF"  />
	</p>
    <p class="SearchTextTitle">Filter by Branch</p>
    <p id="BranchFilter"><%=Html.DropDownListFor( m=>m.BranchName, Model.BranchNames )%></p>
    
    <p>
		<input type="submit" value="Search" class='SearchButton' id="SearchButton"/>
	</p>
<%} %>

</div>
    <div style="clear:both;" id="SignupResponsePartial">
        
    </div>
</asp:Content>

<asp:Content ID="ReportsContent" ContentPlaceHolderID="ReportsContent" runat="server">
	<div>
		<div>
			<% Html.RenderPartial("/Views/Reports/ViewReports.ascx"); %>
		</div>
	</div>
</asp:Content>
