<%-- // Copyright 1998-2016 Epic Games, Inc. All Rights Reserved. --%>

<%@ Page Title="" Language="C#" MasterPageFile="~/Views/Shared/Site.Master" Inherits="System.Web.Mvc.ViewPage<BuggViewModel>" %>
<%@ Import Namespace="Tools.CrashReporter.CrashReportWebSite.DataModels" %>
<%@ Import Namespace="Tools.CrashReporter.CrashReportWebSite.ViewModels" %>

<asp:Content ID="StyleSheet" ContentPlaceHolderID="CssContent" runat="server">
	<link href="../../Content/Site.css" rel="stylesheet" type="text/css" />
</asp:Content>

<asp:Content ID="TitleContent" ContentPlaceHolderID="TitleContent" runat="server">
	Show Bugg <%=Html.DisplayFor( m => Model.Bugg.Id )%> 
</asp:Content>

<asp:Content ID="ScriptContent"  ContentPlaceHolderID="ScriptContent" runat="server" >
	<script type="text/javascript">
		$(document).ready(function ()
		{
			// Zebra stripes
			$("#CrashesTable tr:nth-child(even)").css("background-color", "#C3CAD0");
			$("#CrashesTable tr:nth-child(odd)").css("background-color", "#eeeeee");

			// Get Table Height and set Main Height
			$("#main").height($("#CrashesTable").height() + 800);
			$("#CrashesTable").width($("#ShowCommandLine").width());

			// Show/Hide parts of the call stack
			$("#DisplayModuleNames").click(function ()
			{
				$(".module-name").toggle();
			});
			$("#DisplayFunctionNames").click(function ()
			{
				$(".function-name").toggle();
			});
			$("#DisplayFilePathNames").click(function ()
			{
				$(".file-path").toggle();
			});
			$("#DisplayFileNames").click(function ()
			{
				$(".file-name").toggle();
			});
			$("#DisplayUnformattedCallStack").click(function () 
			{
				$("#FormattedCallStackContainer").toggle();
				$("#RawCallStackContainer").toggle();
			});
		});
	</script>
</asp:Content>

<asp:Content ID="AboveMainContent" ContentPlaceHolderID="AboveMainContent" runat="server">
	<div style="clear:both;"><small style="color: lightgray;">Generated in <%=Model.GenerationTime%> second(s)</small><br /></div>
</asp:Content>

<asp:Content ID="MainContent" ContentPlaceHolderID="MainContent" runat="server">
<div id="SetBar" style="clear:both;">
	<%using( Html.BeginForm( "Show", "Buggs" ) )
	  { %>
		<div id="SetInput">
			<span id="set-status" style="vertical-align: middle;">Set Status</span>
			<select name="SetStatus" id="SetStatus" >
				<option selected="selected" value=""></option>
				<option value="Unset">Unset</option>
				<option value="Reviewed">Reviewed</option>
				<option value="New">New</option>
				<option value="Coder">Coder</option>
				<option value="Tester">Tester</option>
			</select>
			<input type="submit" name="SetStatusSubmit" value="Set" class="SetButtonGreen" />

			<span id="set-ttp" style="">JIRA</span>
			<input name="SetTTP" type="text" id="ttp-text-box" />
			<input type="submit" name="SetTTPSubmit" value="Set" class="SetButtonGreen" />
	
			<span id="set-fixed-in" style="">FixedIn</span>
			<input name="SetFixedIn" type="text" id="fixed-in-text-box" />
			<input type="submit" name="SetFixedInSubmit" value="Set" class="SetButtonGreen" />
		</div>
	<%} %>
</div>

<div id='CrashesShowContainer'>
	<div id='CrashDataContainer' >
		<dl style='list-style-type: none; font-weight: bold;' >
		<dt>ID</dt>
			<dd ><%=Html.DisplayFor( m => Model.Bugg.Id )%></dd>

		<%--<dt>Build Version of Latest Crash</dt>
			<dd><%=Html.DisplayFor( m => Model.Bugg.BuildVersion )%></dd>--%>

		<dt>Affected Build Versions</dt>
			<dd><%=Model.Bugg.GetAffectedVersions()%></dd>
			
		<dt>Time of Latest Crash</dt> 
			<dd class='even'><%=Model.Bugg.TimeOfLastCrash%></dd>

		<dt>Time of First Crash</dt>
			<dd><%=Model.Bugg.TimeOfFirstCrash%></dd>

		<dt>Bugg life span</dt>
			<dd><%=Model.Bugg.GetLifeSpan()%></dd>

		<dt>Crash type</dt> 
			<dd class='even'><%=Model.Bugg.GetCrashTypeAsString()%></dd>

		<dt>Number of Users</dt>
			<dd class='even'><%=Html.DisplayFor( m => Model.Bugg.NumberOfUsers )%></dd>

		<dt>Number of Unique Machines</dt>
			<dd class='even'><%=Html.DisplayFor( m => Model.Bugg.NumberOfUniqueMachines )%></dd>

		<dt>Number of Crashes</dt>
			<dd ><%=Html.DisplayFor( m => Model.Bugg.NumberOfCrashes )%></dd>

		<dt>JIRA</dt>

		<%using( Html.BeginForm( "Show", "Buggs" ) )
		{	%>

			<%if (string.IsNullOrEmpty( Model.Bugg.TTPID ))
			{ %>
				<dd class='even'>
					<input type="submit" value="CopyToJira" title="<%=Model.Bugg.ToTooltip()%>" id="id <%=Model.Bugg.Id%>" name="CopyToJira-<%=Model.Bugg.Id%>" class="SearchButton CopyToJiraStyle" />
				</dd>
			<% } else { %>
				<dd class='even'>
					<a href="https://jira.ol.epicgames.net/browse/<%=Model.Bugg.TTPID%>" target="_blank"><%=Model.Bugg.TTPID%></a>
				</dd>
			<% } %>

		<%} %>

		<dt>Status</dt>
			<dd ><%=Html.DisplayFor( m => Model.Bugg.Status )%></dd>

		<dt>Fixed Change List</dt>
			<dd class='even'><%=Html.DisplayFor( m => Model.Bugg.FixedChangeList )%></dd>
		
		<%--JIRA--%>
		<dt>&nbsp;</dt>
		<dt style="font-style:italic">*FROM JIRA*</dt>

		<dt style="font-style:italic">Summary</dt>
			<dd style="max-width: 210px;word-wrap: break-word;font-style:italic;"><small><%=Model.Bugg.JiraSummary%></small></dd>

		<dt style="font-style:italic">Components</dt>
			<dd style="font-style:italic"><%=Model.Bugg.JiraComponentsText%></dd>

		<dt style="font-style:italic">Resolution</dt>
			<dd style="font-style:italic"><%=Model.Bugg.JiraResolution%></dd>

		<dt style="font-style:italic">Fix Versions</dt>
			<dd style="font-style:italic"><%=Model.Bugg.JiraFixVersionsText%></dd>

		<dt style="font-style:italic">Fix CL</dt>
		<%if( !string.IsNullOrEmpty( Model.Bugg.JiraFixCL ) )
		{ %>
			<dd style="background-color:blue;font-style:italic;">
				<%=Model.Bugg.JiraFixCL%>
			</dd>
		<%}
		else
		{%>
			<dd></dd>
		<%} %>


	</dl>
	</div>

	<div id="CallStackContainer" >

		<% if( !string.IsNullOrEmpty( Model.Bugg.LatestCrashSummary ) ) 
			{ %>
				<div id='ShowErrorMessage'>
					<br />
					<h3>Error Message</h3>
					<div id='ErrorMessage'>
						<%=Html.DisplayFor( m => m.Bugg.LatestCrashSummary )%>
					</div>
				</div>
				<br />
		<%	}
		%>


		<h3>Call Stack of Most Recent Crash</h3>
		<div id='RawCallStackContainer' style='display:none;'>
			<div>
				<% foreach( CallStackEntry CallStackLine in Model.CallStack.CallStackEntries )
					{%>
						<%=Html.DisplayFor( x => CallStackLine.RawCallStackLine )%>
					<br />
				<% } %>
			</div>
		</div>
		<div id="FormattedCallStackContainer">
			<div>
				<%foreach( CallStackEntry CallStackLine in Model.CallStack.CallStackEntries )
					{%>
						<span class="module-name"><%=Html.DisplayFor( m => CallStackLine.ModuleName )%>!</span><span class="function-name"><%=Html.DisplayFor( m => CallStackLine.FunctionName )%></span>
						<span class="file-name"><%=Html.DisplayFor( m => CallStackLine.FileName )%></span>
						<span class="file-path" style='display: none'><%=Html.DisplayFor( m => CallStackLine.FilePath )%></span>
					<br />
				<%}%>
			</div>
		</div>
		<div id='FormatCallStack'>
			<%using( Html.BeginForm( "Show", "Buggs" ) )
			  {	%>
				<%= Html.CheckBox( "DisplayModuleNames", true )%>
				<%= Html.Label( "Modules" )%>

				<%= Html.CheckBox( "DisplayFunctionNames", true )%>
				<%= Html.Label( "Functions" )%>
					
				<%= Html.CheckBox( "DisplayFileNames", true )%>
				<%= Html.Label( "File Names & Lines" )%>

				<%= Html.CheckBox( "DisplayFilePathNames", false )%>
				<%= Html.Label( "File Paths & Lines" )%>

				<%= Html.CheckBox( "DisplayUnformattedCallStack", false )%>
				<%= Html.Label( "Unformatted" )%>
			<%} %>
		</div>
		<div id='SourceContextContainer'>
			<h3>Source Context</h3>
			<div id='SourceContextDisplay'>
				<pre><%=Html.DisplayFor( x => Model.SourceContext )%></pre>
			</div>
		</div>

		<div class='CrashViewTextBoxRight'>
			<h3>Crashes</h3>
			<table id ='CrashesTable'>
				<tr>
					<!-- if you add a new column be sure to update the switch statement in crashescontroller.cs -->
					<th style='width: 6em;'><%=Url.TableHeader( "Id", "Id", Model )%></th>
					<th style='width: 15em'><%=Url.TableHeader( "BuildVersion", "BuildVersion", Model )%></th>
					<th style='width: 15em'><%=Url.TableHeader( "TimeStamp", "TimeStamp", Model )%></th>
					<th style='width: 12em'><%=Url.TableHeader( "Details", "Details", Model )%></th>
					<th style='width: 12em;'><%=Url.TableHeader( "CallStack", "RawCallStack", Model )%></th>
					<th>Description</th>
				</tr>
				<%if( Model.Crashes != null )
				{
					int IterationCount = 0;
					foreach( Crash CrashInstance in Model.Crashes )
					{
						IterationCount++;
						if (IterationCount > 64)
						{
							break;
						}
						%>
			
						<tr class='CrashRow'>
							<td class="Id"><%=Html.ActionLink( CrashInstance.Id.ToString(), "Show", new { controller = "crashes", id = CrashInstance.Id }, null )%></td>
							<td class="BuildVersion"><%=CrashInstance.BuildVersion%></td>
							<td class="TimeOfCrash">
								<%=CrashInstance.GetTimeOfCrash()[0]%> &nbsp;
								<%=CrashInstance.GetTimeOfCrash()[1]%><br />
								Change: <%= CrashInstance.ChangeListVersion %><br />
							</td>
							<td class="Summary">
								<%=CrashInstance.UserName%><br />
								<%=CrashInstance.GameName %><br />
								<%=CrashInstance.EngineMode %><br />
								<%=CrashInstance.PlatformName %><br />
							</td><td class="CallStack" >
								<%--Display callstack only for the first crash--%>
								<% if(IterationCount==1)
								{ %>
								<div style="clip : auto; ">
									<div id='<%=CrashInstance.Id %>-TrimmedCallStackBox' class='TrimmedCallStackBox'>
										<% foreach( CallStackEntry Entry in Model.CallStack.CallStackEntries.Take(4) )
										{%>
											<span class="function-name">
												<%=Url.CallStackSearchLink( Html.Encode( Entry.GetTrimmedFunctionName( 60 ) ), Model )%>
											</span>
											<br />
										<%} %>
									</div>
						
									<a class='FullCallStackTrigger' ><span class='FullCallStackTriggerText'>Full Callstack</span>
										<div id='<%=CrashInstance.Id %>-FullCallStackBox' class='FullCallStackBox'>
											<%foreach( CallStackEntry Entry in Model.CallStack.CallStackEntries.Take( 40 ) )
												{%>
												<span class="FunctionName">
													<%=Html.Encode( Entry.GetTrimmedFunctionName( 80 ) )%>
												</span>
												<br />
											<%} %>
										</div>
									</a>
								</div>
								<%} %>
							</td>
							<td class="Description"><span><%=CrashInstance.Description%>&nbsp;</span></td> 
						</tr>
					<% } %>
				<%} %>
			</table>
		</div>
	</div>
</div>

</asp:Content>
