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

<table>
	<tr>
		<th>
			#
		</th>
		<th>
			URL
		</th>
		<th>
			JIRA
		</th>
		<th>
			FixCL
		</th>
		<th>
			# Occurrences
		</th>
		<th>
			Latest Version Affected
		</th>
		<th>
			# Affected Users
		</th>
		<th>
			Latest CL Affected
		</th>
		<th>
			Latest Environment Affected
		</th>
		<th>
			First Crash Timestamp
		</th>

		<%--JIRA--%>
		<th>
			JiraSummary
		</th>
		<th>
			JiraComponentsText
		</th>
		<th>
			JiraResolution
		</th>
		<th>
			JiraFixVersionsText
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
			<%--<a href="https://jira.ol.epicgames.net/browse/<%=Model.Bugg.TTPID%>" target="_blank"><%=Model.Bugg.TTPID%></a>--%>
			<%=Html.ActionLink( Bugg.Id.ToString(), "Show", new { controller = "Buggs", id = Bugg.Id }, null )%>
		</td>

		<%if( string.IsNullOrEmpty( Bugg.TTPID ) && !string.IsNullOrEmpty(Bugg.Pattern ) )
		{ %>
			<td>
				<input type="submit" value="CopyToJira" title="<%=Bugg.ToTooltip()%>" id="id <%=Bugg.Id%>" name="CopyToJira-<%=Bugg.Id%>" class="CopyToJiraStyle" />
			</td>
		<% } else if( string.IsNullOrEmpty( Bugg.TTPID ) && string.IsNullOrEmpty(Bugg.Pattern ) )
		{ %>
			<td>
				No pattern!
			</td>
		<% } else { %>
			<td>
				<%--NewBugg.TTPID = RealBugg.TTPID;							// JIRA--%>
				<a href="https://jira.ol.epicgames.net/browse/<%=Bugg.TTPID%>" target="_blank"><%=Bugg.TTPID%></a>
			</td>
		<% } %>

		<td>
			<%--NewBugg.FixedChangeList = RealBugg.FixedChangeList;		// FixCL--%>
			<%=Bugg.FixedChangeList%>
		</td>
		<td>
			<%--NewBugg.NumberOfCrashes = Top.Value;					// # Occurrences--%>
			<%=Bugg.NumberOfCrashes%>
		</td>
		<td>
			<%--NewBugg.BuildVersion = NewBugg.AffectedVersions.Last();	// Latest Version Affected--%>
			<%=Bugg.BuildVersion%>
		</td>
		<td>
			<%--NewBugg.NumberOfUniqueMachines = MachineIds.Count;		// # Affected Users--%>
			<%=Bugg.NumberOfUniqueMachines%>
		</td>
		<td>
			<%--NewBugg.LatestCLAffected = ILatestCLAffected;			// Latest CL Affected--%>
			<%=Bugg.LatestCLAffected%>
		</td>
		<td>
			<%--NewBugg.LatestOSAffected = LatestOSAffected;			// Latest Environment Affected--%>
			<%=Bugg.LatestOSAffected%>
		</td>
		<td>
			<%--NewBugg.TimeOfFirstCrash = RealBugg.TimeOfFirstCrash;	// First Crash Timestamp	--%>
			<%=Bugg.TimeOfFirstCrash%>
		</td>

		<%--JIRA--%>
		<td style="max-width: 256px;">
			<%=Bugg.JiraSummary%>
		</td>
		<td>
			<%=Bugg.JiraComponentsText%>
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