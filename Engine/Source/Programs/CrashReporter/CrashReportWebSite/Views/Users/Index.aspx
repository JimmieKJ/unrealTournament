<%-- // Copyright 1998-2015 Epic Games, Inc. All Rights Reserved. --%>

<%@ Page Title="" Language="C#" MasterPageFile="~/Views/Shared/Site.Master" Inherits="System.Web.Mvc.ViewPage<UsersViewModel>" %>
<%@ Import Namespace="Tools.CrashReporter.CrashReportWebSite.Models" %>

<asp:Content ID="StyleSheet" ContentPlaceHolderID="CssContent" runat="server">
	<link href="../../Content/Site.css" rel="stylesheet" type="text/css" />
</asp:Content>

<asp:Content ID="TitleContent" ContentPlaceHolderID="TitleContent" runat="server">
[CR] Edit the groups the users belong to
</asp:Content>

<asp:Content ID="ScriptContent" ContentPlaceHolderID="ScriptContent" runat="server">
</asp:Content>

<asp:Content ID="AboveMainContent"  ContentPlaceHolderID="AboveMainContent" runat="server" >
</asp:Content>

<asp:Content ID="MainContent" ContentPlaceHolderID="MainContent" runat="server">
	<div id='UserGroupBar'>
		<%foreach( var GroupCount in Model.GroupCounts )
		 {%>
			<span class=<%if( Model.UserGroup == GroupCount.Key )
			 { %> "UserGroupTabSelected" <% }
				 else
			 { %>"UserGroupTab"<% } %> id="<%=GroupCount.Key%>Tab">
				<%=Url.UserGroupLink( GroupCount.Key, Model )%> 
			<span class="UserGroupResults">
				(<%=GroupCount.Value%>)
			</span>
		</span>
		<%} %>
	</div>

	<div id="UserNames">
		<%foreach( string UserName in Model.Users )
		{%>
		<div>
			<form action="<%="/Users/Index/" + Model.UserGroup %>" method="POST" id="UserNamesForm" style="text-align: center">
				<%=UserName%>

				<span id="set-user-group"></span>
				<select name="<%=UserName%>" id="SetUserGroup" onchange="submit()">
					<option selected="selected" value="<%=Model.UserGroup%>"><%=Model.UserGroup %></option>
					<%foreach( var GroupCount in Model.GroupCounts )
					{%>
						<option value="<%=GroupCount.Key%>"><%=GroupCount.Key%></option>
					<%} %>
				</select>
				<br />
			</form>
		</div>
		<% } %>
	</div>
</asp:Content>


