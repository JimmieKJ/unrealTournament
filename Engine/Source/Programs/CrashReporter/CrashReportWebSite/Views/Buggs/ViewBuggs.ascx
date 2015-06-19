<%-- // Copyright 1998-2015 Epic Games, Inc. All Rights Reserved. --%>

<%@ Control Language="C#" Inherits="System.Web.Mvc.ViewUserControl<BuggsViewModel>" %>
<%@ Import Namespace="Tools.CrashReporter.CrashReportWebSite.Models" %>

<table id ='CrashesTable'>
	<tr id="SelectBar"> 
		<td colspan='15'>
			<span class="SelectorTitle">Select:</span>
			<span class='SelectorLink' id='CheckAll' >All</span>
			<span class='SelectorLink' id="CheckNone">None</span>
			<div class="PaginationBox">
			<%=Html.PageLinks( Model.PagingInfo, i => Url.Action( "", 
				new 
				{ 
					page = i, 
					SearchQuery = Model.SearchQuery,
					SortTerm = Model.SortTerm, 
					SortOrder = Model.SortOrder, 
					UserGroup = Model.UserGroup, 
					DateFrom = Model.DateFrom, 
					DateTo = Model.DateTo,
					BuildVersion = Model.BuildVersion,
					CrashType = Model.CrashType
				} 
			) )%>
			</div>
			<%=Html.HiddenFor( u => u.UserGroup )%>
			<%=Html.Hidden( "SortTerm", Model.SortTerm )%>
			<%=Html.Hidden( "SortOrder", Model.SortOrder )%>
			<%=Html.Hidden( "Page", Model.PagingInfo.CurrentPage )%>
			<%=Html.Hidden( "PageSize", Model.PagingInfo.PageSize )%>
			<%=Html.Hidden( "SearchQuery", Model.SearchQuery )%>
			<%=Html.HiddenFor( m => m.DateFrom )%>
			<%=Html.HiddenFor( m => m.DateTo )%>
			<%=Html.HiddenFor( m => m.BuildVersion )%>
		</td>
	</tr>
	<tr id="CrashesTableHeader">
		<th>&nbsp;</th>
		<!-- if you add a new column be sure to update the switch statement in the repository SortBy function -->
		<th style='width:  6em;'><%=Url.TableHeader( "Id", "Id", Model )%></th>
		<th style='width: 8em;'><%=Url.TableHeader( "Build Version", "BuildVersion", Model )%></th>
		<th style='width: 8em;'><%=Url.TableHeader( "Crashes In Time Frame (Group)", "CrashesInTimeFrameGroup", Model )%></th>
		<th style='width: 8em;'><%=Url.TableHeader( "Crashes In Time Frame (All)", "CrashesInTimeFrameAll", Model )%></th>
		<th style='width: 12em;'><%=Url.TableHeader( "Latest Crash", "LatestCrash", Model )%></th>
		<th style='width: 12em;'><%=Url.TableHeader( "First Crash", "FirstCrash", Model )%></th> 
		<th style='width: 8em;'><%=Url.TableHeader( "# of Crashes", "NumberOfCrashes", Model )%></th> 
		<th style='width: 8em;'><%=Url.TableHeader( "Users Affected", "NumberOfUsers", Model )%></th> 
		<th style='width: 24em;'><%=Url.TableHeader( "Pattern", "Pattern", Model )%></th>
		<th style='width: 8em;'><%=Url.TableHeader( "CrashType", "CrashType", Model )%></th>
		<th style='width: 8em;'><%=Url.TableHeader( "Status", "Status", Model )%></th>
		<th style='width: 8em;'><%=Url.TableHeader( "FixedCL#", "FixedChangeList", Model )%></th>
		<th style='width: 8em;'><%=Url.TableHeader( "JIRA", "TTPID", Model )%></th>
	</tr>
	<%
		using( FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer( this.GetType().ToString() ) )
		{ 
			try
			{
				foreach( Bugg CurrentBugg in Model.Results )
				{
					using( FScopedLogTimer LogTimer2 = new FScopedLogTimer( "CurrentBugg" + "(" + CurrentBugg.Id + ")" ) )
					{ 
						string BuggRowColor = "grey";
						string BuggColorDescription = "Incoming CrashGroup";

						if( string.IsNullOrWhiteSpace( CurrentBugg.FixedChangeList ) && string.IsNullOrWhiteSpace( CurrentBugg.TTPID ) )
						{
							BuggRowColor = "#FFFF88"; // yellow
							BuggColorDescription = "This CrashGroup has not been fixed or assigned a JIRA";
						}

						if( !string.IsNullOrWhiteSpace( CurrentBugg.TTPID ) && string.IsNullOrWhiteSpace( CurrentBugg.FixedChangeList ) )
						{
							BuggRowColor = "#D01F3C"; // red
							BuggColorDescription = "This CrashGroup has  been assigned a JIRA: " + CurrentBugg.TTPID + " but has not been fixed.";
						}

						if( CurrentBugg.Status == "Coder" )
						{
							BuggRowColor = "#D01F3C"; // red
							BuggColorDescription = "This CrashGroup status has been set to Coder";
						}
						if( CurrentBugg.Status == "Tester" )
						{
							BuggRowColor = "#5C87B2"; // blue
							BuggColorDescription = "This CrashGroup status has been set to Tester";
						}
						if( !string.IsNullOrWhiteSpace( CurrentBugg.FixedChangeList ) )
						{
							// Green
							BuggRowColor = "#008C00"; //green
							BuggColorDescription = "This CrashGroup has been fixed in CL# " + CurrentBugg.FixedChangeList;
						}
	%>

		<tr class='BuggRow'>
			<td class="BuggTd" style="background-color: <%=BuggRowColor %>;" title="<%=BuggColorDescription %>"></td>
			<td class="Id"><%=Html.ActionLink( CurrentBugg.Id.ToString(), "Show", new { controller = "Buggs", id = CurrentBugg.Id }, null )%></td>
			<td><%=CurrentBugg.BuildVersion%></td>
			<td><%=CurrentBugg.CrashesInTimeFrameGroup%></td>
			<td><%=CurrentBugg.CrashesInTimeFrameAll%></td>
			<td><%=CurrentBugg.TimeOfLastCrash%></td>
			<td><%=CurrentBugg.TimeOfFirstCrash%></td>
			<td><%=CurrentBugg.NumberOfCrashes%> </td>

			
			<td><%=CurrentBugg.NumberOfUniqueMachines%></td>
			
			<td class="CallStack">
				<div>
					<div id='Div1' class='TrimmedCallStackBox'>
								<%
						var FunctionCalls = CurrentBugg.GetFunctionCalls();
						int i = 0;
						foreach( string FunctionCall in FunctionCalls )
						{
							string FunctionCallDisplay = FunctionCall;
							if( FunctionCall.Length > 60 )
							{
								FunctionCallDisplay = FunctionCall.Substring( 0, 60 );
							}%>
								<span class="FunctionName">
									<%=Url.CallStackSearchLink( Html.Encode( FunctionCallDisplay ), Model )%>
								</span>
								<br />
								<%
												i++;
												if( i > 3 )
												{
													break;
												}
											} %>
					</div>
					<a class='FullCallStackTrigger'><span class='FullCallStackTriggerText'>Full Callstack</span>
						<div id='<%=CurrentBugg.Id %>-FullCallStackBox' class='FullCallStackBox'>
							<%foreach( string FunctionCall in FunctionCalls )
							{%>
							<span class="FunctionName">
								<%=Html.Encode( FunctionCall )%>
							</span>
							<br />
							<%} %>
						</div>
					</a>
				</div>
			</td>
			<td><%=CurrentBugg.GetCrashTypeAsString()%></td>
			<td><%=CurrentBugg.Status%></td>
			<td><%=CurrentBugg.FixedChangeList%></td>
			<td> <span><a href="https://jira.ol.epicgames.net/browse/<%=CurrentBugg.TTPID%>" target="_blank"><%=CurrentBugg.TTPID%></a></span>  </td>
		</tr>
	<%				}
				}
			}
			catch
			{%>
				<tr>
					<td colspan="9">No Results Found. Please try adjusting your search. Or contact support.</td>
				</tr>
		<%	} 
		}
		%>
</table>
