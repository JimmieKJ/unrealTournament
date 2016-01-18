<%-- // Copyright 1998-2015 Epic Games, Inc. All Rights Reserved. --%>

<%@ Control Language="C#" Inherits="System.Web.Mvc.ViewUserControl<ReportsViewModel>" %>
<%@ Import Namespace="Tools.CrashReporter.CrashReportWebSite.Models" %>

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
	
<body>
<form runat="server">
	
<table >
	<tr>
		<td>
			TotalAnonymousCrashes:&nbsp;<%=Model.TotalAnonymousCrashes %>
		</td>
	</tr>

	<tr>
		<td>
			TotalUniqueAnonymousCrashes:&nbsp;<%=Model.TotalUniqueAnonymousCrashes %>
		</td>
	</tr>

	<tr>
		<td>
			TotalAffectedUsers:&nbsp;<%=Model.TotalAffectedUsers %>
		</td>
	</tr>
</table>
<br />

<table style="width: 100%">
	<tr>
		<th>
			#
		</th>
		<th>
			URL
		</th>
		<th>
			# Occurrences
		</th>
		<th>
			# Affected Users
		</th>
		<th>
			Versions Affected
		</th>
		<th>
			Latest CL Affected
		</th>
		<th>
			Latest Environment Affected
		</th>
		<th>
			JIRA
		</th>
		<%--JIRA--%>
		<th>
			JiraComponents
		</th>
		<th style="width: 384px">
			JiraSummary
		</th>
		<th>
			JiraResolution
		</th>
		<th>
			JiraFixVersions
		</th>
		<th>
			JiraFixCL
		</th>
	</tr>

	<%
		for( int Index = 0; Index < Model.Buggs.Count; Index ++ )
		{
			Bugg Bugg = Model.Buggs[Index];
	%>
	<tr>
		<td>
			<%--Number--%>
			<%=Index%>
		</td>
		<td>
			<%--NewBugg.Id = RealBugg.Id;								// CrashGroup URL (a link to the Bugg)--%>
			<%--<a href="https://jira.ol.epicgames.net/browse/<%=Model.Bugg.Jira%>" target="_blank"><%=Model.Bugg.Jira%></a>--%>
			<%=Html.ActionLink( Bugg.Id.ToString(), "Show", new { controller = "Buggs", id = Bugg.Id }, null )%>
		</td>
		<td>
			<%--NewBugg.NumberOfCrashes = Top.Value;					// # Occurrences--%>
			<%=Bugg.CrashesInTimeFrameGroup%>
		</td>
		<td>
			<%--NewBugg.NumberOfUniqueMachines = MachineIds.Count;		// # Affected Users--%>
			<%=Bugg.NumberOfUniqueMachines%>
		</td>
		<td>
			<%--NewBugg.BuildVersion = NewBugg.AffectedVersions.Last();	// Latest Version Affected--%>
			<%=Bugg.GetAffectedVersions()%>
		</td>
		<td>
			<%--NewBugg.LatestCLAffected = ILatestCLAffected;			// Latest CL Affected--%>
			<%=Bugg.LatestCLAffected%>
		</td>
		<td>
			<%--NewBugg.LatestOSAffected = LatestOSAffected;			// Latest Environment Affected--%>
			<%=Bugg.LatestOSAffected%>
		</td>
		<%if (string.IsNullOrEmpty( Bugg.Jira ) && !string.IsNullOrEmpty( Bugg.Pattern ))
		{ %>
			<td>
				<input type="submit" value="CopyToJira" title="<%=Bugg.ToTooltip()%>" id="id <%=Bugg.Id%>" name="CopyToJira-<%=Bugg.Id%>" class="SearchButton CopyToJiraStyle CopyToJiraStyle" />
			</td>
		<% } else if( string.IsNullOrEmpty( Bugg.Jira ) && string.IsNullOrEmpty(Bugg.Pattern ) )
		{ %>
			<td>
				No pattern!
			</td>
		<% } else { %>
			<td>
				<%--NewBugg.Jira = RealBugg.Jira;							// JIRA--%>
				<a href="https://jira.ol.epicgames.net/browse/<%=Bugg.Jira%>" target="_blank"><%=Bugg.Jira%></a>
			</td>
		<% } %>

		<td>
			<%=Bugg.JiraComponentsText%>
		</td>
		<%--JIRA--%>
		<td style="max-width: 256px;">
			<%=Bugg.JiraSummary%>
		</td>		
		<td>
			<%=Bugg.JiraResolution%>
		</td>
		<td>
			<%=Bugg.JiraFixVersionsText%>
		</td>
		
		<%if( !string.IsNullOrEmpty( Bugg.JiraFixCL ) )
		{ %>
			<td style="background-color:blue">
				<%=Bugg.JiraFixCL%>
			</td>
		<%}
		else
		{%>
			<td>
			</td>
		<%} %>

	</tr>
	<%
		}	
	%>

</table>

</form>
</body>