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
	            //$("#FilterCrashesForm").submit();
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

	        //$(".CrashType").click(function () {
	        //    $("#FilterCrashesForm").submit();
	        //});
	    });
	</script>

</asp:Content>

<asp:Content ID="AboveMainContent"  ContentPlaceHolderID="AboveMainContent" runat="server" >
	<div id='SearchForm' style="clear:both">
	<div id="titleBar" style="width:1230px; border: 1px solid rgba(128, 128, 128, 0.44); padding:10px;">
        <h4 id="titleText">SEARCH OPTIONS</h4>
    </div>	
	<% using( Html.BeginForm( "", "Crashes", FormMethod.Get, new { id = "FilterCrashesForm" } ) )
	{ %>
		<%=Html.HiddenFor( u => u.UserGroup )%>
		<%=Html.Hidden( "SortTerm", Model.SortTerm )%>
		<%=Html.Hidden( "SortOrder", Model.SortOrder )%>
        <div id="Container">
            <div id ="searchTypeBox">
                <p class="searchText">Show crashes of type</p>
                <ul>
		            <li><input type="radio" name="CrashType" class="CrashType" value="CrashesAsserts" <%=( Model.CrashType == "CrashesAsserts" ) ? "checked='checked'" : "" %> /> <span title='All Crashes Except Ensures'>Crashes+Asserts</span></li>
			        <li><input type="radio" name="CrashType" class="CrashType" value="Ensure" <%=( Model.CrashType == "Ensure" ) ? "checked='checked'" : "" %>/> <span title='Only Ensures'>Ensures</span></li>
			        <li><input type="radio" name="CrashType" class="CrashType" value="Assert" <%=( Model.CrashType == "Assert" ) ? "checked='checked'" : "" %>/> <span title='Only Asserts'>Asserts</span></li>
			        <li><input type="radio" name="CrashType" class="CrashType" value="Crashes" <%=( Model.CrashType == "Crashes" ) ? "checked='checked'" : "" %>/> <span title='Crashes Except Ensures and Asserts'>Crashes</span></li>
			        <li><input type="radio" name="CrashType" class="CrashType" value="All" <%=( Model.CrashType == "All" ) ? "checked='checked'" : "" %>/> <span title='All Crashes'>All</span></li>
		        </ul>
            </div>
            <div id ="searchFields">
                <div id="SearchBox">
                    <div id="fieldSeperator">
                        <p class="searchText">Username</p>
			            <input id="SearchQuery" name="UsernameQuery" type="text" value="<%=Model.UsernameQuery %>" width="1000" title="For searching for an user use 'user:[name]'" />
                    </div>
                    <div id="fieldSeperator">
                        <p class="searchText">EpicID</p>
			            <input id="SearchQuery" name="EpicIdQuery" type="text" value="<%=Model.EpicIdQuery %>" width="1000" title="For searching for an user use 'user:[name]'" />
                    </div>
                    <div id="fieldSeperator">
                        <p class="searchText">MachineID</p>
			            <input id="SearchQuery" name="MachineIdQuery" type="text" value="<%=Model.MachineIdQuery %>" width="1000" title="For searching for an user use 'user:[name]'" />    
                    </div>
                    <div id="fieldSeperator">
                        <p class="searchText">Jira</p>
			            <input id="SearchQuery" name="JiraQuery" type="text" value="<%=Model.JiraQuery %>" width="1000" title="For searching for an user use 'user:[name]'" />
                    </div>
		        </div>
            </div>
            <div id ="searchFields">
                <div id="SearchBox">
                    <div id="fieldSeperator">
                        <p class="searchText">Call Stack</p>
			            <input id="SearchQuery" name="SearchQuery" type="text" value="<%=Model.SearchQuery %>" width="1000" title="For searching for an user use 'user:[name]'" />
                    </div>
                    <div id="fieldSeperator">
                        <p class="searchText">Summary/Message</p>
			            <input id="SearchQuery" name="MessageQuery" type="text" value="<%=Model.MessageQuery %>" width="1000" title="For searching for an user use 'user:[name]'" />
                    </div>
                    <div id="fieldSeperator">
                        <p class="searchText">Description</p>
			            <input id="SearchQuery" name="DescriptionQuery" type="text" value="<%=Model.DescriptionQuery %>" width="1000" title="For searching for an user use 'user:[name]'" />    
                    </div>
		        </div>
            </div>
            <div id="dateBox">
                <div id="rowSeperator">
                    <script>$.datepicker.setDefaults($.datepicker.regional['']);</script>
                    <p class="searchText">Filter by Date</p>
                    <span style="margin-left: 13px;">From:</span>
		            <input id="dateFromVisible" type="text" class="date" AUTOCOMPLETE=OFF />
		            <input id="dateFrom" name="dateFrom" type="hidden" value="<%=Model.DateFrom %>" AUTOCOMPLETE=OFF />
                    <span style="margin-left: 10px;">To:</span>
                    <input id="dateToVisible" type="text" class="date" AUTOCOMPLETE=OFF />
                    <input id="dateTo" name="dateTo" type="hidden" value="<%=Model.DateTo %>" AUTOCOMPLETE=OFF />
                </div>
                <div id="rowSeperator">
                    <p class="searchText">Filter by Date/Branch/Game</p>
                    <%=Html.DropDownListFor(m=>m.BranchName,Model.BranchNames)%>
                </div>
                <div id="rowSeperator">
                    <p class="searchText">Filter by Game</p>
                    <span><input id="GameName" name="GameName" type="text" value="<%=Model.GameName %>" title="Game to filter by; prefix with '-' to exclude game."/></span>
                </div>
                <div id="branchBox" style="clear: both;">
                        <input type="submit" value="Search" class='SearchButton' />
                </div>
            </div>
        </div>
	<% } %>
	</div>
</asp:Content>
