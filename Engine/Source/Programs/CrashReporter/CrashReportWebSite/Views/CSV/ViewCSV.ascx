<%-- // Copyright 1998-2015 Epic Games, Inc. All Rights Reserved. --%>

<%@ Control Language="C#" Inherits="System.Web.Mvc.ViewUserControl<CSV_ViewModel>" %>
<%@ Import Namespace="Tools.CrashReporter.CrashReportWebSite.Models" %>
<%@ Import Namespace="Tools.CrashReporter.CrashReportWebSite.Controllers" %>

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
			Total Crashes:&nbsp;<%=Model.TotalCrashes %>
		</td>
	</tr>
	<tr>
		<td>
			Total Crashes YearToDate:&nbsp;<%=Model.TotalCrashesYearToDate %>
		</td>
	</tr>

	<tr>
		<td>
			<strong>
			Filtered = CrashOrAssert, Anonymous, EpicId != 0, BuggId != 0,
			</strong>
		</td>
	</tr>

	<tr>
		<td>
			Total Unique Crashes:&nbsp;<%=Model.UniqueCrashesFiltered %>
		</td>
	</tr>
	<tr>
		<td>
			Total Affected Users:&nbsp;<%=Model.AffectedUsersFiltered %>
		</td>
	</tr>
		

	<tr>
		<td>
			<strong>
			Filtered = CrashOrAssert, Anonymous, EpicId != 0, BuggId != 0, TimeOfCrash within the time range, from <%=Model.DateTimeFrom.ToString()%> to <%=Model.DateTimeTo.ToString()%>
			</strong>
		</td>
	</tr>

	<tr>
		<td>
			Crashes:&nbsp;<%=Model.CrashesFilteredWithDate %>
		</td>
	</tr>


	<tr>
		<td>
			File:&nbsp;<a href="<%=Model.CSVPathname%>" target="_blank"><%=Model.GetCSVFilename()%></a>
		</td>
	</tr>

	<tr>
		<td>
			Directory:&nbsp;<a href="<%=Model.GetCSVDirectory()%>" target="_blank"><%=Model.GetCSVDirectory()%></a>
		</td>
	</tr>

	<tr>
		<td title="Copy the link and open in the explorer">
			Public Directory:&nbsp;<strong><%=Model.GetCSVPublicDirectory() %></strong>
		</td>
	</tr>
</table>
<br />

<p>Displaying top 32 records, ordered by the date of a crash</p>
<table style="width: 100%">
	<tr>
		<th>GameName</th>
		<th>TimeOfCrash</th>
		<th>BuiltFromCL</th>
		<th>PlatformName</th>
		<th>EngineMode</th>
		<th>MachineId</th>
		<th>Module</th>
		<th>BuildVersion</th>
		<th>Jira</th>
		<th>Branch</th>
		<th>CrashType</th>
		<th>EpicId</th>
		<th>BuggId</th>
	</tr>

	<%
		for (int Index = 0; Index < Model.CSVRows.Count; Index++)
		{
			FCSVRow CSVRow = Model.CSVRows[Index];
	%>
	<tr>
		<td><%=CSVRow.GameName%></td>
		<td><%=CSVRow.TimeOfCrash%></td>
		<td><%=CSVRow.BuiltFromCL%></td>
		<td><%=CSVRow.PlatformName%></td>
		<td><%=CSVRow.EngineMode%></td>
		<td><%=CSVRow.MachineId%></td>
		<td><%=CSVRow.Module%></td>
		<td><%=CSVRow.BuildVersion%></td>
		<td><%=CSVRow.Jira%></td>
		<td><%=CSVRow.Branch%></td>
		<td><%=CSVRow.GetCrashTypeAsString()%></td>
		<td><%=CSVRow.EpicId%></td>
		<td><%=CSVRow.BuggId%></td>
	</tr>
	<%
		}	
	%>

</table>

</form>
</body>