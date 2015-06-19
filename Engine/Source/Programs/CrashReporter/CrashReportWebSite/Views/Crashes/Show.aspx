<%-- // Copyright 1998-2015 Epic Games, Inc. All Rights Reserved. --%>

<%@ Page Title="" Language="C#" MasterPageFile="~/Views/Shared/Site.Master" Inherits="System.Web.Mvc.ViewPage<CrashViewModel>" %>
<%@ Import Namespace="Tools.CrashReporter.CrashReportWebSite.Models" %>

<asp:Content ID="StyleSheet" ContentPlaceHolderID="CssContent" runat="server">
	<link href="../../Content/Site.css" rel="stylesheet" type="text/css" />
</asp:Content>

<asp:Content ID="TitleContent" ContentPlaceHolderID="TitleContent" runat="server">
	Show
</asp:Content>

<asp:Content ID="ScriptContent"  ContentPlaceHolderID="ScriptContent" runat="server" >
	<script type="text/javascript">
		$(document).ready(function ()
		{
			$("#EditDescription").click(function ()
			{
				$("#CrashDescription").css("display", "none");
				$("#ShowCrashDescription input").css("display", "block");
				$("#EditDescription").css("display", "none");
				$("#SaveDescription").css("display", "inline");
			});

			$("#SaveDescription").click(function ()
			{
				$('#EditCrashDescriptionForm').submit();
				$('Description').disable();
			});

			$("#DisplayModuleNames").click(function ()
			{
				$(".module-name").toggle();
			});

			$("#DisplayFunctionNames").click(function ()
			{
				$(".function-name").toggle();
			});

			$("#DisplayFileNames").click(function ()
			{
				$(".file-name").toggle();
			});

			$("#DisplayFilePathNames").click(function ()
			{
				$(".file-path").toggle();
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
	<% using( Html.BeginForm( "Show", "Crashes" ) )
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

			<span id="set-ttp" style="">Bug</span>
			<input name="SetTTP" type="text" id="ttp-text-box" />
			<input type="submit" name="SetTTPSubmit" value="Set" class="SetButtonGreen" />

			<span id="set-fixed-in" style="">FixedIn</span>
			<input name="SetFixedIn" type="text" id="fixed-in-text-box" />
			<%=Html.Hidden( "Description", Model.Crash.Description ) %> 
			<input type="submit" name="SetFixedInSubmit" value="Set" class="SetButtonGreen" />
		</div>
	<% } %>
</div>

<div id='CrashesShowContainer'>
	<div id='CrashDataContainer' >
		<dl style='list-style-type: none; font-weight: bold' >
			<dt>ID</dt>
				<dd ><%=Html.DisplayFor( m => Model.Crash.Id ) %></dd>

			<dt>Bugg(s)</dt>
				<%
				if( Model.Crash.Buggs.Count > 0 )
				{
					foreach( Bugg BuggInstance in Model.Crash.Buggs )
					{%>
					<dd>
						<%=Html.ActionLink( BuggInstance.Id.ToString(), "Show", new { controller = "Buggs", Id = BuggInstance.Id } ) %>
					</dd>
					<%}
				}
				else
				{%>
					<dd>
					</dd>
				<%	
				}%>
				
			<dt>Saved Files</dt>
				<dd class='even'>
					<%if( !Model.Crash.HasLogFile.HasValue || Model.Crash.HasLogFile.Equals( true ) ) 
					{ %>
						<a style='text-decoration:none; vertical-align:middle;' href='<%=Model.Crash.GetLogUrl() %>'><img src="../../Content/Images/Icons/log.png" style="height:20px;border:none;" />&nbsp;Log</a>
						<%
					} %>
		
				</dd>
			<dt>&nbsp;</dt>
				<dd>	
					<%if( !Model.Crash.HasMiniDumpFile.HasValue || Model.Crash.HasMiniDumpFile.Equals( true ) ) 
					{ %>
						<a style='text-decoration:none;' href='<%=Model.Crash.GetDumpUrl() %>'><img src="../../Content/Images/Icons/miniDump.png" style="height:20px;border:none;" />&nbsp;MiniDump</a>
						<%
					} %>
				</dd>
			<dt>&nbsp;</dt>
				<dd>
					<%if( Model.Crash.HasVideoFile.HasValue && Model.Crash.HasVideoFile.Equals( true ) ) 
					{ %>
						<a id="VideoLink" style="text-decoration:none;" href='<%=Model.Crash.GetVideoUrl() %>'><img src="../../Content/Images/Icons/video.png" style="height:20px;border:none;" />&nbsp;Video</a>
						<%
					} %>
				</dd>
			<dt>&nbsp;</dt>
				<dd>
					<%if( Model.Crash.HasDiagnosticsFile.HasValue && Model.Crash.HasDiagnosticsFile.Equals( true ) ) 
					{ %>
						<a id="A1" style="text-decoration:none;" href='<%=Model.Crash.GetDiagnosticsUrl() %>'><img src="../../Content/Images/Icons/log.png" style="height:20px;border:none;" />&nbsp;Diags</a>
						<%
					} %>
				</dd>
			<dt>&nbsp;</dt>
				<dd>
					<%if( Model.Crash.HasMetaData.HasValue && Model.Crash.HasMetaData.Equals( true ) ) 
					{ %>
						<a id="A2" style="text-decoration:none;" href='<%=Model.Crash.GetMetaDataUrl() %>'><img src="../../Content/Images/Icons/log.png" style="height:20px;border:none;" />&nbsp;WERInfo</a>
						<%
					} %>
				</dd>

			<dt>Branch</dt> 
				<dd class='even'>
				<%=Model.Crash.Branch %><br />

			<dt>Time of Crash</dt> 
				<dd class='even'>
				<%=Model.Crash.GetTimeOfCrash()[0] %>
				<%=Model.Crash.GetTimeOfCrash()[1] %>

			<dt>Crash type</dt> 
				<dd class='even' style='width:8em'><%=Model.Crash.GetCrashTypeAsString()%></dd>

			<dt>User</dt>
				<dd>
					<%=Html.DisplayFor(m => Model.Crash.UserName) %>
				</dd>

			<dt>User Group</dt>
				<dd><%=Html.DisplayFor(m => Model.Crash.UserGroupName) %></dd>

			<dt>Game Name</dt>
				<dd class='even'><%=Html.DisplayFor(m => Model.Crash.GameName) %></dd>

			<dt>Engine Mode</dt>
				<dd class='even'><%=Html.DisplayFor(m => Model.Crash.EngineMode) %></dd>

			<dt>Language</dt>
				<dd ><%=Html.DisplayFor(m => Model.Crash.LanguageExt) %></dd>
			
			<dt>Platform</dt>
				<dd ><%=Html.DisplayFor(m => Model.Crash.PlatformName) %></dd>

			<dt>Machine Id</dt> 
				<dd class='even'><small><%=Html.DisplayFor(m => Model.Crash.ComputerName) %></small></dd>

			<dt>Epic Account Id</dt> 
				<dd class='even'><small><%=Html.DisplayFor(m => Model.Crash.EpicAccountId) %></small></dd>

			<dt>Allowed to contact</dt> 
				<dd class='even'><%=Html.DisplayFor(m => Model.Crash.AllowToBeContacted) %></dd>

			<dt>Build Version</dt>
				<dd ><%=Html.DisplayFor(m => Model.Crash.BuildVersion) %></dd>

			<dt>Fixed Changelist #</dt>
				<dd class='even'><%=Html.DisplayFor(m => Model.Crash.FixedChangeList) %></dd>

			<dt>Changelist #</dt>
				<dd class='even'><%=Html.DisplayFor(m => Model.Crash.ChangeListVersion) %></dd>

			<dt>JIRA</dt>
				<%--<dd ><%=Html.DisplayFor(m => Model.Crash.TTPID) %></dd>--%>
				<dd><a href="https://jira.ol.epicgames.net/browse/<%=Model.Crash.TTPID%>" target="_blank"><%=Model.Crash.TTPID%></a></dd>

			<dt>Status</dt>
				<dd ><%=Html.DisplayFor(m => Model.Crash.Status) %></dd>

			<dt>Module</dt>
				<dd ><%=Html.DisplayFor(m => Model.Crash.Module) %></dd>
		</dl>
	</div>

	<div id="CallStackContainer" >
		<% if( !string.IsNullOrEmpty( Model.Crash.Summary ) ) 
			{ %>
				<div id='ShowErrorMessage'>
					<br />
					<h3>Error Message</h3>
					<div id='ErrorMessage'>
						<%=Html.DisplayFor( m => m.Crash.Summary )%>
					</div>
				</div>
				<br />
		<%	}
		%>

		<div class='CrashViewTextBox'>
			<div class='CrashViewTextBoxRight'>
				<h3>Call Stack</h3>
				<div id='RawCallStackContainer' style='display: none'>
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
						<% foreach( CallStackEntry CallStackLine in Model.CallStack.CallStackEntries )
						   {%>
								<span class="module-name"><%=Html.DisplayFor( m => CallStackLine.ModuleName )%>!</span>
								<span class="function-name"><%=Html.DisplayFor( m => CallStackLine.FunctionName )%></span>
								<span class="file-name"><%=Html.DisplayFor( m => CallStackLine.FileName )%></span>
								<span class="file-path" style='display: none'><%=Html.DisplayFor( m => CallStackLine.FilePath )%></span>
								<br />
						<% } %>
					</div>
				</div>
			</div>
		</div>
		<div id='FormatCallStack'>
		<% using( Html.BeginForm( "Show", "Crashes" ) )
		   { %>
			<%= Html.CheckBox( "DisplayModuleNames", true )%>
			<%= Html.Label( "Modules" ) %>

			<%= Html.CheckBox( "DisplayFunctionNames", true )%>
			<%= Html.Label( "Functions" ) %>

			<%= Html.CheckBox( "DisplayFileNames", true )%>
			<%= Html.Label( "File Names & Lines" ) %>
			
			<%= Html.CheckBox( "DisplayFilePathNames", false )%>
			<%= Html.Label( "File Paths & Lines" ) %>

			<%= Html.CheckBox( "DisplayUnformattedCallStack", false )%>
			<%= Html.Label( "Unformatted" ) %>
		<% } %>
		</div>
		<div id='SourceContextContainer'>
			<h3>Source Context</h3>
			<div id='SourceContextDisplay'>
				<pre><%=Html.DisplayFor( x => Model.Crash.SourceContext )%></pre>
			</div>
		</div>
		<% using( Html.BeginForm( "Show", "Crashes", FormMethod.Post, new { id = "EditCrashDescriptionForm" } ) )
		   { %>
			<div id='ShowCrashDescription'>
				<br />
				<h3>Description 
					<span class='EditButton' id='EditDescription'>Edit</span> 
					<span class='EditButton' id='SaveDescription'>Save</span>
				</h3>
				<div id='InputCrashDescription' >
					<%=Html.TextBox( "Description", Model.Crash.Description ) %> 
				</div>
			</div>
		<% } %>
		<div id='ShowCommandLine'>
			<br />
			<h3>Command Line</h3>
			<div id='CrashCommandLine'>
				<%=Model.Crash.CommandLine %> 
			</div>
		</div>
	</div>
</div>
</asp:Content>
