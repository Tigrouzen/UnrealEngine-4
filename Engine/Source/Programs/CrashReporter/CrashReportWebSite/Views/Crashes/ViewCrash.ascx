﻿<%-- // Copyright 1998-2014 Epic Games, Inc. All Rights Reserved. --%>

<%@ Control Language="C#" Inherits="System.Web.Mvc.ViewUserControl<CrashesViewModel>" %>
<%@ Import Namespace="Tools.CrashReporter.CrashReportWebSite.Models" %>

<table id ="CrashesTable">
	<tr id="SelectBar">
		<td colspan='17'>
			<span class="SelectorTitle">Select:</span>
			<span class="SelectorLink" id="CheckAll">All</span>
			<span class="SelectorLink" id="CheckNone">None</span>

			<div class="PaginationBox">
				<%=Html.PageLinks( Model.PagingInfo, i => Url.Action( "", new { page = i, SearchQuery = Model.Query, SortTerm = Model.Term, SortOrder = Model.Order, UserGroup = Model.UserGroup, DateFrom = Model.DateFrom, DateTo = Model.DateTo, CrashType = Model.CrashType } ) )%>
			</div>
			<!-- form starts and ends in site.master-->
			<div id='SetStatusForm'>
				<%=Html.HiddenFor( u => u.UserGroup )%>
				<%=Html.Hidden( "SortTerm", Model.Term )%>
				<%=Html.Hidden( "SortOrder", Model.Order )%>
				<%=Html.Hidden( "Page", Model.PagingInfo.CurrentPage )%>
				<%=Html.Hidden( "PageSize", Model.PagingInfo.PageSize )%>
				<%=Html.Hidden( "SearchQuery", Model.Query )%>
				<%=Html.HiddenFor( m => m.DateFrom )%>
				<%=Html.HiddenFor( m => m.DateTo )%>
				<div id="SetStatusInput">
					<span id="SetStatusTitle">Set Status</span>

					<select name="SetStatus" id="SetStatus" >
						<option selected="selected" value=""></option>
						<option value="Unset">Unset</option>
						<option value="Reviewed">Reviewed</option>
						<option value="New">New</option>
						<option value="Coder">Coder</option>
						<option value="Tester">Tester</option>
					</select>

					<input type="submit" name="SetStatusSubmit" value="Set" class="SetButton" />

					<span id="set-ttp" style="">TTP</span>
					<input name="SetTTP" type="text" id="ttp-text-box" />
					<input type="submit" name="SetTTPSubmit" value="Set" class="SetButton" />

					<span id="set-fixed-in" style="">FixedIn</span>
					<input name="SetFixedIn" type="text" id="fixed-in-text-box" />
					<input type="submit" name="SetFixedInSubmit" value="Set" class="SetButton" />
				</div>
			</div>
		</td>
	</tr>

	<tr id="CrashesTableHeader">
		<th>&nbsp;</th>
		<!-- if you add a new column be sure to update the switch statement in crashescontroller.cs -->
		<th style='width: 6em;'><%=Url.TableHeader( "Id", "Id", Model )%></th>
		<th style='width: 15em'><%=Url.TableHeader( "TimeOfCrash", "TimeOfCrash", Model )%></th>
		<th style='width: 12em'><%=Url.TableHeader( "UserName", "UserName", Model )%></th>
		<th style='width: 12em;'><%=Url.TableHeader( "CallStack", "RawCallStack", Model )%></th> 
		<th style='width: 11em;'><%=Url.TableHeader( "Game", "GameName", Model )%></th> 
		<th style='width: 11em;'><%=Url.TableHeader( "Mode", "EngineMode", Model )%></th> 
		<th style='width: 16em;'><%=Url.TableHeader( "FixedCL#", "FixedChangeList", Model )%></th>
		<th style='width: 9em;'><%=Url.TableHeader( "TTP", "TTPID", Model) %></th>
		<th style='width: 9em;'><%=Url.TableHeader( "Branch", "Branch", Model) %></th>
		<th>Description</th>
		<th style='width: 9em;'><%=Url.TableHeader( "CL#", "ChangeListVersion", Model )%></th>
		<th style='width: 12em;'><%=Url.TableHeader( "Computer", "ComputerName", Model )%></th>
		<th style='width: 14em;'><%=Url.TableHeader( "Platform", "PlatformName", Model )%></th>
		<th style='width: 12em;'><%=Url.TableHeader( "Status", "Status", Model )%></th>
		<th style='width: 12em;'><%=Url.TableHeader( "Module", "Module", Model )%></th>
	</tr>
	<% if( Model.Results.ToList() != null )
	   { %>
		<%
		int Iteration = 0;
		foreach( Crash CurrentCrash in ( IEnumerable )Model.Results )
		{
			Buggs_Crash BuggCrash = null;
			try
			{
				BuggCrash = CurrentCrash.Buggs_Crashes.FirstOrDefault();
			}
			catch
			{
				BuggCrash = null;
			}

			Iteration++;
			string CrashRowColor = "grey";
			string CrashColorDescription = "Incoming Crash";

			if( string.IsNullOrWhiteSpace( CurrentCrash.FixedChangeList ) && string.IsNullOrWhiteSpace( CurrentCrash.TTPID ) )
			{
				CrashRowColor = "#FFFF88"; // yellow
				CrashColorDescription = "This crash has not been fixed or assigned a TTP";
			}

			if( !string.IsNullOrWhiteSpace( CurrentCrash.FixedChangeList ) )
			{
				// Green
				CrashRowColor = "#008C00"; //green
				CrashColorDescription = "This crash has been fixed in CL# " + CurrentCrash.FixedChangeList;
			}

			if( ( BuggCrash != null ) && !string.IsNullOrWhiteSpace( CurrentCrash.TTPID ) && string.IsNullOrWhiteSpace( CurrentCrash.FixedChangeList ) )
			{
				CrashRowColor = "#D01F3C"; // red
				CrashColorDescription = "This crash has occurred more than once and been assigned a TTP: " + CurrentCrash.TTPID + " but has not been fixed.";
			}

			if( CurrentCrash.Status == "Tester" )
			{
				CrashRowColor = "#718698"; // red
				CrashColorDescription = "This crash status has been set to Tester";
			}
			%>

			<tr class='CrashRow <%=CurrentCrash.User.UserGroup %>'>
				<td class="CrashTd" style="background-color: <%=CrashRowColor %>;" title = "<%=CrashColorDescription %>"><INPUT TYPE="CHECKBOX" Value="<%=Iteration %>"NAME="<%=CurrentCrash.Id%>" id="<%=CurrentCrash.Id %>" class='input CrashCheckBox' ></td> 
				<td class="Id"><%=Html.ActionLink(CurrentCrash.Id.ToString(), "Show", new { controller = "crashes", id = CurrentCrash.Id }, null)%> <br /> <em style="font-size:x-small;"> <%= ( BuggCrash != null ) ? BuggCrash.BuggId.ToString() : ""  %></em></td> 
				<td class="TimeOfCrash">
					<%=CurrentCrash.GetTimeOfCrash()[0]%>
					<br />
					<%=CurrentCrash.GetTimeOfCrash()[1]%>
				</td>
				<td class="Username"><%=CurrentCrash.User.UserName%></td>
				<td class="CallStack" >
					<div style="clip : auto; ">
						<div id='<%=CurrentCrash.Id %>-TrimmedCallStackBox' class='TrimmedCallStackBox'>
							<% foreach( CallStackEntry Entry in CurrentCrash.GetCallStackEntries( 0, 4 ) )
							   { %>
								<span class="function-name">
									<%=Url.CallStackSearchLink( Html.Encode( Entry.GetTrimmedFunctionName( 45 ) ), Model )%>
								</span><br />
							<% } %>
						</div>
						<a class='FullCallStackTrigger'><span class='FullCallStackTriggerText'>Full Callstack</span>
							<div id='<%=CurrentCrash.Id %>-FullCallStackBox' class='FullCallStackBox'>
								<% foreach( CallStackEntry Entry in CurrentCrash.GetCallStackEntries( 0, 40 ) )
								   {%>
										<span class="FunctionName">
											<%=Html.Encode( Entry.GetTrimmedFunctionName( 60 ) )%>
										</span>
										<br />
								<% } %>
							</div>
						</a>
					</div>

				</td>
				<td class="Game"><%=CurrentCrash.GameName%></td>
				<td class="Mode"><%=CurrentCrash.EngineMode%></td>
				<td class="FixedChangeList"><%=CurrentCrash.FixedChangeList%></td>
				<td class="Ttp"><%=CurrentCrash.TTPID%>&nbsp;</td>
				<td class="Branch"><%=CurrentCrash.Branch%>&nbsp;</td>
				<td class="Description"><span class="TableData"><%=CurrentCrash.Description%>&nbsp;</span></td>
				<td class="ChangeListVersion"><%=CurrentCrash.ChangeListVersion%></td>
				<td class="Computer"><%=CurrentCrash.ComputerName%></td>
				<td class="Platform"><%=CurrentCrash.PlatformName%></td>
				<td class="Status"><%=CurrentCrash.Status%></td>
				<td class="Module"><%=CurrentCrash.Module%></td>
			</tr>

		<% } %>
	<% } %>

</table>
