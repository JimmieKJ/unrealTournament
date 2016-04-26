# Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

use strict;
use warnings;
use Data::Dumper;
use File::Copy;
use File::Path;
use File::Find;
use List::Util ('min', 'max');
use ElectricCommander();
use JSON;
use URI::Escape;
use Cwd;
use HTML::Entities;

# local modules
use Utility;
use Workspace;

# standard colors
my $error_color = '#bd2424';
my $warning_color = '#F9BB00';
my $success_color = '#18a752';
my $unknown_color = '#cccccc';

### Commands ###########################################################################################################################################################################################

# prints the history of builds in the given stream
sub print_build_history
{
	my ($ec, $ec_job, $stream, $node, $count, $before_change) = @_;
	
	my $history = get_build_history($ec, $ec_job, $stream, $node, $count, $before_change);
	foreach(@{$history})
	{
		print "CL $_->{'properties'}->{'CL'}: Job $_->{'job_id'} \"$_->{'job_name'}\", JobStep $_->{'jobstep_id'}, Result = $_->{'result'}\n";
	}
}

# prints a list of all suspected 
sub print_suspected_causers
{
	my ($ec, $jobstep_id, $workspace, $min_change) = @_;
	
	# read the jobstep info from EC
	my $jobstep = ec_get_jobstep($ec, $jobstep_id);

	# read diagnostics for the given jobstep
	my $diagnostics = read_jobstep_diagnostics($jobstep);
	fail("Couldn't read diagnostics file") if !$diagnostics;

	# parse out the files with errors
	my $files = parse_files_containing_errors($diagnostics);
	if(@{$files})
	{
		# print out all the files
		print "\n";
		print "Found file: $_\n" foreach(@{$files});
		print "\n";

		# map them to changelists
		my $change_to_files = find_changes_affecting_files($files, $workspace->{'name'}, $min_change);
		foreach my $change_number(sort { $b <=> $a } keys %{$change_to_files})
		{
			print "Change $change_number:\n";
			foreach my $file (@{$change_to_files->{$change_number}})
			{
				print "    Modifies $file\n";
			}
		}
	}
}

# writes a notification email to the local directory
sub write_step_notification
{
	my ($ec, $ec_project, $jobstep_id, $workspace_name, $workspace_dir) = @_;

	# get the metadata for this job step
	my $jobstep = ec_get_jobstep($ec, $jobstep_id);
	
	# read the job definition the original workspace on the network
	my $job_definition_file = join_paths($jobstep->{'workspace_dir'}, 'job.json');
	my $job_definition = read_json($job_definition_file);

	# format the email
	my $notifications = get_jobstep_notifications($ec, $ec_project, $job_definition, $jobstep, $workspace_name, $workspace_dir);
	fail("No notifications for jobstep $jobstep_id") if !$notifications;
	print "Default recipients: ".join(", ", @{$notifications->{'default_recipients'}})."\n";
	print "Fail Causers: ".join(", ", @{$notifications->{'fail_causers'}})."\n";
	
	# write the file to disk
	my $output_file = "StepNotification.html";
	print "Writing $output_file...\n";
	write_file($output_file, $notifications->{'message_body'});
}

# writes a notification email to the local directory
sub write_trigger_notification
{
	my ($ec, $ec_project, $job_id, $trigger_name) = @_;
	
	# get the job details
	my $job = ec_get_job($ec, $job_id);
	
	# read the job definition
	my $job_definition_file = join_paths($job->{'workspace_dir'}, 'job.json');
	my $job_definition = read_json($job_definition_file);
	
	# find the trigger definition
	my $trigger_definition = find_trigger_definition($job_definition, $trigger_name) || fail("Couldn't find definition for trigger $trigger_name");
	
	# get all the jobsteps for it
	my $trigger_jobsteps = get_trigger_jobsteps($job, $trigger_definition);
	
	# get the notification info
	my $message = get_trigger_notification($trigger_definition->{'Name'}, $job, $trigger_jobsteps);
	
	# write the file to disk
	my $output_file = "TriggerNotification.html";
	print "Writing $output_file...\n";
	write_file($output_file, $message);
}

### Utility functions ##################################################################################################################################################################################

# gets notification information for a trigger
sub get_trigger_notification
{
	my ($trigger_name, $job, $jobsteps) = @_;

	# figure out the overall result
	my $succeeded = ec_get_combined_result($jobsteps);

	# get the summary
	my $summary_message;
	if($succeeded)
	{
		$summary_message = "The <b>'$trigger_name'</b> trigger is available for CL $job->{'properties'}->{'CL'} in the $job->{'properties'}->{'Stream'} stream.";
	}
	else
	{
		$summary_message = "The <b>'$trigger_name'</b> trigger for CL $job->{'properties'}->{'CL'} in the $job->{'properties'}->{'Stream'} stream is blocked due to errors.";
	}
	
	# create the output message
	my $message;
	$message .= "<html xmlns=\"http://www.w3.org/1999/xhtml\">\n";
	$message .= "<head>\n";
	$message .= "    <meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />\n";
	$message .= "<head>\n";
	$message .= "<body style=\"margin:0;font-family: Arial, Helvetica, sans-serif;\">\n";

	# write the header
	my $header_info_key_style = "font-size: 11pt;font-weight: bold;text-align: left;vertical-align:top;padding-left: 30px;width:150px;color:white;";
	my $header_info_value_style = "font-size: 11pt;padding: 3px 5px;vertical-align:top;color:white;";
	my $header_info_link_style = "color: #ffffff;";
	$message .= "    <div style=\"overflow:auto;border-spacing: 0;border-collapse: collapse;table-layout: fixed;padding-bottom: 30px;padding-left: 30px;padding-right: 30px; background: ".($succeeded? $success_color : $error_color).";color: #ffffff;\">\n";
	$message .= "        <h1 style=\"margin-top: 20px;font-size: 36px;vertical-align: bottom;\">".($succeeded? "" : "<img src=\"https://cdn2.unrealengine.com/Maintenance/error-414x391-793315300.png\" width=\"42\" height=\"40\" style=\"vertical-align:bottom\"> ")."$trigger_name</h1>\n";
	$message .= "        <div style=\"border: 2px solid #ffffff; padding-top:16px; padding-bottom:16px;\">\n";
	$message .= "            <table>\n";
	$message .= "                <tr>\n";
	$message .= "                    <th style=\"$header_info_key_style\">Job</th>\n";
	$message .= "                    <td style=\"$header_info_value_style\"><a href=\"".ec_get_full_url($job->{'job_url'})."\" style=\"$header_info_link_style\">$job->{'job_name'}</a></td>\n";
	$message .= "                </tr>\n";
	$message .= "                <tr>\n";
	$message .= "                    <th style=\"$header_info_key_style\">Outcome</th>\n";
	$message .= "                    <td style=\"$header_info_value_style\"><b>".($succeeded? "Ready" : "Failed")."</b></td>\n";
	$message .= "                </tr>\n";
	$message .= "            </table>\n";
	$message .= "        </div>\n";
	$message .= "    </div>\n";

	# find the max length of any dependency name
	$message .= "    <div style=\"display:inline-block;margin-left:30px;\">\n";
	$message .= "        <p style=\"font-size:small;margin-top:30px;margin-bottom:0px;\">$summary_message <a href=\"".ec_get_full_url($job->{'job_url'})."\"><b>Show Job</b></a></p>\n";
	$message .= "        <table align=\"left\" style=\"margin-top:30px;margin-left:30px;\">\n";
	foreach my $jobstep (@{$jobsteps})
	{
		my $result = $jobstep->{'result'};
		my ($result_color, $result_text) = ($result eq 'success')? ($success_color, 'Success') : ($result eq 'warning')? ($warning_color, 'Warning') : (($result eq 'skipped')? 'gray' : $error_color, (uc (substr $result, 0, 1)).(substr $result, 1));
		my $link = $jobstep->{'jobstep_url'}? ec_get_full_url($jobstep->{'jobstep_url'}) : undef;
		$message .= "            <tr>\n";
		$message .= "                <td style=\"padding:5px;padding-left:1em;padding-right:3em;background-color:#f0f0f0;font-size:small;\">".($link? "<a href=\"$link\">" : "").$jobstep->{'jobstep_name'}.($link? "</a>" : "")."</td>\n";
		$message .= "                <td style=\"padding:3px;background-color:$result_color;text-align:center;color:white;min-width:8em;font-size:small;\">".($link? "<a href=\"$link\"" : "<span")." style=\"font-weight:bold;color:white;text-decoration:none;\">$result_text".($link? "</a>" : "</span>")."</td>\n";
		$message .= "            </tr>\n";
	}
	$message .= "        </table>\n";
	$message .= "    </div>\n";
	$message .= "</body>\n";
	$message .= "</html>\n";

	# return everything
	$message;
}

# determines the notification settings and generates a notification email for a given build
sub get_jobstep_notifications
{
	my ($ec, $ec_project, $job_definition, $jobstep, $workspace_name, $workspace_dir) = @_;
	
	# read the step properties
	my $current_change = $jobstep->{'properties'}->{'CL'};
	my $stream = $jobstep->{'properties'}->{'Stream'};

	# read the postp output from the workspace
	my $diagnostics = read_jobstep_diagnostics($jobstep);
	return undef if !$diagnostics || ($diagnostics->{'num_errors'} == 0 && $diagnostics->{'num_warnings'} == 0);

	# find the node, and return if we don't want to notify on warnings
	my $node = find_node_definition($job_definition, $jobstep->{'jobstep_name'});
	return undef if !$node->{'Notify'}->{'Warnings'} && $diagnostics->{'num_errors'} == 0;

	# standard colors
	my $outcome_color = ($diagnostics->{'num_errors'} > 0)? $error_color : ($diagnostics->{'num_warnings'} > 0)? $warning_color : $success_color;
	my $outcome_color_lookup = { error => $error_color, warning => $warning_color, success => $success_color };
	
	# get the outcome description
	my $outcome = "Succeeded";
	if($diagnostics->{'num_errors'} > 0)
	{
		$outcome = "<span style=\"font-weight:bold;\">Failed</span> ($diagnostics->{'num_errors'}".(($diagnostics->{'num_errors'} == 50)? "+" : "")." Errors";
		$outcome .= ", $diagnostics->{'num_warnings'}".(($diagnostics->{'num_warnings'} == 50)? "+" : "")." warnings" if $diagnostics->{'num_warnings'} > 0;
		$outcome .= ")";
	}
	elsif($diagnostics->{'num_warnings'} > 0)
	{
		$outcome = "Completed with $diagnostics->{'num_warnings'}".(($diagnostics->{'num_warnings'} == 50)? "+" : "")." warnings";
	}

	# get the build history for this node
	my $build_history = get_build_history($ec, $jobstep->{'job_id'}, $stream, $jobstep->{'jobstep_name'}, 50, $current_change);

	# find the last change that build successfully
	my $last_successful_change = $current_change;
	if($#{$build_history} >= 0)
	{
		$last_successful_change = $build_history->[$#{$build_history}]->{'properties'}->{'CL'};
	}
	
	# if we're including headers in the output message, figure out which paths we want to filter
	my $author_paths;
	my $default_recipients = [];
	if($node)
	{
		my $notify = $node->{'Notify'};
		if($notify)
		{
			$default_recipients = split_list($notify->{'Default'} || '', ";");
			my $author_paths_string = $notify->{'Submitters'};
			$author_paths = split_list($author_paths_string, ";") if $author_paths_string;
		}
	}

	# get the p4 history from this change
	my $change_history = get_change_history($stream, $author_paths || [ "..." ], $last_successful_change + 1, 100);
	
	# add all the unique authors from the change history to the list of recipients
	my $fail_causers = [];
	if($author_paths)
	{
		my $unique_recipients = { };
		$unique_recipients->{$_->{'author_email'}} = 1 foreach @{$change_history};
		$fail_causers = [keys %{$unique_recipients}];
	}

	# print the list of builds
	if($#{$build_history} >= 0)
	{
		# print the list of changes interleaved with the results of each build
		print "\n**** Changes since last succeeded *****************************\n\n";
		print_interleaved_history($build_history, $change_history, { current_jobstep => $jobstep->{'jobstep_id'} });
		print "\n***************************************************************\n\n";
	}

	# find which changes look like they caused failures, based on the files they touched
	my $suspected_files = parse_files_containing_errors($diagnostics);
	my $suspected_changes = find_changes_affecting_files($suspected_files, $workspace_name, $last_successful_change + 1);
	
	# opening boilerplate
	my $html = "";
	$html .= "<html xmlns=\"http://www.w3.org/1999/xhtml\">\n";
	$html .= "<head>\n";
	$html .= "    <meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />\n";
	$html .= "<head>\n";
	$html .= "<body style=\"margin:0;font-family: Arial, Helvetica, sans-serif;\">\n";

	# write the job info
	my $title = $jobstep->{'jobstep_name'};
	$title =~ s/_/ /g;
	$title =~ s/([a-z])([A-Z])/$1 $2/g;
	
	# css styles. gmail strips out all <style> tags, so we have to have to inline them.
	my $section_style = "overflow:auto; border-spacing:0; border-collapse:collapse; table-layout:fixed; min-height:200px; padding-bottom:30px; padding-left:30px; padding-right:30px;";
	my $header_title_style = "margin-top:20px; font-size:36px; vertical-align:bottom;";
	my $header_info_key_style = "font-size:11pt; font-weight:bold; text-align:left; vertical-align:top; padding-left:30px; width:150px; color:white;";
	my $header_info_value_style = "font-size:11pt; padding: 3px 5px; vertical-align:top; color:white;";
	my $header_info_link_style = "color:#ffffff;";
	
	$html .= "    <div style=\"$section_style background:$outcome_color; color:#ffffff;\">\n";
	$html .= "        <h1 style=\"$header_title_style\"><img src=\"https://cdn2.unrealengine.com/Maintenance/error-414x391-793315300.png\" width=\"42\" height=\"40\" style=\"vertical-align:bottom\"> $title</h1>\n";
	$html .= "        <div style=\"border: 2px solid #ffffff; padding-top:16px; padding-bottom:16px;\">\n";
	$html .= "            <table>\n";
	$html .= "                <tr>\n";
	$html .= "                    <th style=\"$header_info_key_style\">Job</th>\n";
	$html .= "                    <td style=\"$header_info_value_style\"><a href=\"".ec_get_full_url($jobstep->{'job_url'})."\" style=\"$header_info_link_style\">$jobstep->{'job_name'}</a></td>\n";
	$html .= "                </tr>\n";
	$html .= "                <tr>\n";
	$html .= "                    <th style=\"$header_info_key_style\">Step</th>\n";
	$html .= "                    <td style=\"$header_info_value_style\"><a href=\"".ec_get_full_url($jobstep->{'jobstep_url'})."\" style=\"$header_info_link_style\">$jobstep->{'jobstep_name'}</a> (<a href=\"".ec_get_full_url($jobstep->{'jobstep_log_url'})."\" style=\"$header_info_link_style\">Output</a>)</td>\n";
	$html .= "                </tr>\n";
	$html .= "                <tr>\n";
	$html .= "                    <th style=\"$header_info_key_style\">Outcome</th>\n";
	$html .= "                    <td style=\"$header_info_value_style\">$outcome</td>\n";
	$html .= "                </tr>\n";
	$html .= "            </table>\n";
	$html .= "        </div>\n";
	$html .= "    </div>\n";

	# write the error report
	$html .= "    <div style=\"$section_style background: white;\">\n";
	$html .= "        <h2 style=\"color: #202020;margin-top: 20px;margin-bottom:20px;font-size: 16pt;\">Report</h2>\n";
	$html .= "        <table cellspacing=\"6\" style=\"margin-top: 10px;margin-left: 0px;margin-right: 40px;\">\n";
	my $diagnostics_list = $diagnostics->{'list'};
	for(my $diagnostic_idx = 0; $diagnostic_idx <= $#{$diagnostics_list}; )
	{
		my @escaped_text;
		
		# try to merge all the identical diagnostics on consecutive lines together
		my $first_diagnostic = $diagnostics_list->[$diagnostic_idx];
		my $diagnostic_line = $first_diagnostic->{'first_line'};
		while($diagnostic_idx <= $#{$diagnostics_list})
		{
			# get the next diagnostic, and make sure it's the same type
			my $diagnostic = $diagnostics_list->[$diagnostic_idx];
			last if $diagnostic->{'first_line'} != $diagnostic_line || $diagnostic->{'type'} ne $first_diagnostic->{'type'};

			# append all the output lines to the list
			foreach(split /\n/, $diagnostic->{'text'})
			{
				push(@escaped_text, encode_entities($_));
				$diagnostic_line++;
			}
			
			# move to the next one
			$diagnostic_idx++;
			
			# always keep separate for now
			last;
		}

		# write out these lines
		my $line_url = "$jobstep->{'jobstep_log_url'}&firstLine=$first_diagnostic->{'first_line'}&numLines=$first_diagnostic->{'num_lines'}";
		$html .= "            <tr>\n";
		$html .= "                <td style=\"font-size:11pt; display:table-cell; padding:0px; background:".($outcome_color_lookup->{$first_diagnostic->{'type'}} || $error_color).";\"><div style=\"width:8px;height:1px;\"></div></td>\n";
		$html .= "                <td style=\"font-size:11pt; vertical-align:middle; padding:.65em 1em;\">\n";
		$html .= "                    <div style=\"font-size:x-small; color:#000000; margin-bottom:3px;\"><a href=\"".ec_get_full_url($line_url)."\" style=\"color:black;\">[Line $first_diagnostic->{'first_line'}]</a></div>\n";
		$html .= "                    <div style=\"font-family:monospace,Arial,Helvetica,sans-serif; font-size:9pt; white-space:pre;\">".join("<br />", @escaped_text)."</div>\n";
		$html .= "                </td>\n";
		$html .= "            </tr>\n";
	}
	$html .= "        </table>\n";
	$html .= "    </div>\n";
	
	# write the list of changes
	my $timeline_change_style = "color:#ffffff; text-decoration:none;";
	my $timeline_item_border_style = "border:2px solid #f7f7f7;";
	my $timeline_point_style = "width:20px; height:20px; border:4px solid #ccc; border-radius:999px;";
	my $timeline_identifier_style = "background:#ccc; padding:5px; width:70px; text-align:center; margin-left:10px; margin-right:10px; font-size:12px; $timeline_item_border_style";
	$html .= "    <div style=\"$section_style background:#f7f7f7;\">\n";
	$html .= "        <h2 style=\"color:#202020; margin-top:20px; margin-bottom:20px; font-size:16pt;\">Timeline</h2>\n";
	$html .= "        <table cellspacing=\"0\" cellpadding=\"0\" style=\"border-collapse:collapse; margin-top:15px; margin-left:10px; margin-right:10px;\">\n";

	my $build_idx = 0;
	my $change_idx = 0;
	while($build_idx <= $#{$build_history} || $change_idx <= $#{$change_history})
	{
		my $build = ($build_idx <= $#{$build_history})? $build_history->[$build_idx] : undef;
		my $change = ($change_idx <= $#{$change_history})? $change_history->[$change_idx] : undef;
		
		if($build_idx > 0 || $change_idx > 0)
		{
			$html .= "            <tr>\n";
			$html .= "                <td>&nbsp; </td>\n";
			$html .= "                <td width=\"12\"></td>\n";
			$html .= "                <td width=\"4\" style=\"background:#ccc;\"><div style=\"width:4px; height:20px;\">&nbsp;</div></td>\n";
			$html .= "                <td width=\"12\"></td>\n";
			$html .= "                <td>&nbsp; </td>\n";
			$html .= "                <td>&nbsp; </td>\n";
			$html .= "            </tr>\n";
		}

		if(!$change || ($build && $build->{'properties'}->{'CL'} >= $change->{'number'}))
		{
			my $build_color = $outcome_color_lookup->{$build->{'result'}} || (($build->{'jobstep_id'} == $jobstep->{'jobstep_id'})? $outcome_color : $unknown_color);
			$html .= "            <tr>\n";
			$html .= "                <td style=\"vertical-align:top;\">\n";
			$html .= "                    <div style=\"$timeline_identifier_style margin-left:0px; background:$build_color;\"><a href=\"".ec_get_full_url($build->{'jobstep_url'})."\" style=\"color: white;text-decoration: none;\">$build->{'properties'}->{'CL'}</a></div>\n";
			$html .= "                </td>\n";
			$html .= "                <td colspan=\"3\">\n";
			$html .= "                    <div style=\"$timeline_point_style background:$build_color; border-color:$build_color;\"></div>\n";
			$html .= "                </td>\n";
			$html .= "            </tr>\n";
			$build_idx++;
		}
		else
		{
			my $escaped_description = $change->{'description'};
			$escaped_description =~ s/\n.*//g;
			$escaped_description = encode_entities($escaped_description);

			my $suspected_change_note = '';
			if($suspected_changes->{$change->{'number'}})
			{
				my @file_names = map { /([^\\\/]*)$/ && $1 } @{$suspected_changes->{$change->{'number'}}};
				$suspected_change_note = "<div style=\"background: #FFF075;padding: 5px;padding-left: 10px;padding-right: 10px;display:inline-block;$timeline_item_border_style\">Modifies ".join(", ", @file_names)."</div>";
			}
			
			# add the changelist number
			$html .= "            <tr>\n";
			$html .= "                <td>&nbsp; </td>\n";
			$html .= "                <td colspan=\"3\" height=\"20px\">\n";
			$html .= "                    <div style=\"$timeline_point_style\"></div>\n";
			$html .= "                </td>\n";
			$html .= "                <td style=\"vertical-align:top;\">\n";
			$html .= "                    <div style=\"$timeline_identifier_style\"><a href=\"http://p4-web/\@md=d&cd=$stream&c=B5m\@/$change->{'number'}?ac=10\" style=\"color: black;text-decoration: none;\">$change->{'number'}</a></div>\n";
			$html .= "                </td>\n";
			$html .= "                <td rowspan=\"2\" style=\"vertical-align:top;\">\n";
			$html .= "                    <div style=\"color: black;font-size: 12px;\"><div style=\"padding:6px;display:inline-block;\"><a href=\"mailto:$change->{'author_email'}\">$change->{'author'}</a> - $escaped_description</div>$suspected_change_note</div>\n";
			$html .= "                </td>\n";
			$html .= "            </tr>\n";

			$change_idx++;
		}
	}
	$html .= "        </table>\n";
	$html .= "    </div>\n";
		
	# closing boilerplate
	$html .= "</body>\n";
	$html .= "</html>\n";
	
	# return all the settings
	{ default_recipients => $default_recipients, fail_causers => $fail_causers, message_body => $html };
}

# gets the history of builds for a given node, stopping at the last successful build before the current one
sub get_build_history
{
	my ($ec, $ec_job, $stream, $node, $count, $before_change) = @_;

	# find a recent list of job steps with the right name
	my $jobsteps_xpath = $ec->findObjects("jobStep", { maxIds => "$count", numObjects => "$count", 
		filter => [{propertyName => 'stepName', operator => 'equals', operand1 => $node}, {propertyName => 'Stream', operator => 'equals', operand1 => $stream} ],
		  sort => [{propertyName => 'jobId', order => 'descending'}],
		select => [{propertyName => 'CL'}] });

	# manually join the jobsteps with the list of jobs
	my $history = [];
	foreach my $object_node ($jobsteps_xpath->findnodes("/responses/response/object"))
	{
		my $jobstep_node = $jobsteps_xpath->findnodes("./jobStep", $object_node)->get_node(1);
		my $jobstep = ec_parse_jobstep($jobsteps_xpath, $jobstep_node);
		next if $jobstep->{'job_name'} =~ /Preflight/i;
		$jobstep->{'properties'} = ec_parse_property_sheet($jobsteps_xpath, $object_node);
		push(@{$history}, $jobstep);
		last if $jobstep->{'result'} eq 'success' && (!$before_change || $jobstep->{'properties'}->{'CL'} < $before_change);
	}
		
	# return the results sorted in decending changelist order
	$history = [ sort { $b->{'properties'}->{'CL'} <=> $a->{'properties'}->{'CL'} || $b->{'job_id'} <=> $a->{'job_id'} } @{$history} ];
}

# gets the changes in a given stream, along with the emails of each submitter
sub get_change_history
{
	my ($stream, $filter_list, $first_change_number, $max_results) = @_;

	# query perforce for the list of changes, running through each filter at a time
	my @change_history = ();
	my %unique_change_numbers = ();
	foreach my $filter(@{$filter_list})
	{
		my $current_change;
		foreach(p4_command("changes -L -m $max_results $stream/$filter\@$first_change_number,now"))
		{
			if(/^Change (\d+) on [^ ]+ by ([^ ]+)@/)
			{
				$current_change = { number => $1, author => $2, description => '' };
				if($2 ne 'buildmachine' && !$unique_change_numbers{$1})
				{
					push(@change_history, $current_change);
					$unique_change_numbers{$1} = 1;
				}
			}
			elsif($current_change && /^\t(.*)/)
			{
				$current_change->{'description'} .= "$1\n" if $1 || $current_change->{'description'};
			}
		}
	}

	# find the email addresses for each user
	my $p4_user_to_email = {};
	foreach my $change(@change_history)
	{
		my $user = $change->{'author'};
		if(!defined $p4_user_to_email->{$user})
		{
			my @records = p4_tagged_command("user -o \"$user\"");
			$p4_user_to_email->{$user} = $records[0]->{'Email'};
		}
		$change->{'author_email'} = $p4_user_to_email->{$user};
	}

	# return the changes sorted in descending changelist order
	@change_history = sort { $b->{'number'} <=> $a->{'number'} } @change_history;
	\@change_history;
}

# find files containing errors in the given diagnostic output
sub parse_files_containing_errors
{
	my ($diagnostics) = @_;

	my $suspected_files = { };
	foreach my $diagnostic (@{$diagnostics->{'list'}})
	{
		foreach(split /\n/, $diagnostic->{'text'})
		{
			if(/([^ \\\/]*[\\\/][^ ()]*)\(\d+\)\s*:\s*error\s+/)
			{
				# Visual studio style error; "<FileName>(<Line>): error"
				$suspected_files->{$1} = 1;
			}
		}
	}

	[keys %{$suspected_files}];
}

# find changes that any of the given files
sub find_changes_affecting_files
{
	my ($files, $workspace_name, $min_change) = @_;

	# convert the suspected files into a list of potential changelists
	my $change_to_files = { };
	foreach my $file (@{$files})
	{
		foreach(p4_command("-c$workspace_name filelog -m 10 \"$file\""))
		{
			if(/^#\d+ change (\d+) /)
			{
				last if $1 < $min_change;
				push(@{$change_to_files->{$1}}, $file);
			}
		}
	}
	$change_to_files;
}

# prints the build history and change history to the log together
sub print_interleaved_history
{
	my ($build_history, $change_history, $optional_arguments) = @_;
	
	my $change_idx = 0;
	foreach my $build (@{$build_history})
	{
		# print all the changes after this build
		while($change_idx < $#{$change_history} && $change_history->[$change_idx]->{'number'} > $build->{'properties'}->{'CL'})
		{
			my $change = $change_history->[$change_idx++];
			
			my $short_description = $change->{'description'};
			$short_description =~ s/\n/ /g;
			
			print "               $change->{'number'} $change->{'author_email'} $short_description\n";
		}

		# print the current build
		if($optional_arguments->{'current_jobstep'} && $build->{'jobstep_id'} == $optional_arguments->{'current_jobstep'})
		{
			print "  >>>> $build->{'properties'}->{'CL'} THIS BUILD\n";
		}
		else
		{
			print "       $build->{'properties'}->{'CL'} ".(uc $build->{'result'})." ($build->{'jobstep_id'})\n";
		}
	}
}

# read diagnostic output from the given xpath
sub read_jobstep_diagnostics
{
	my ($jobstep) = @_;

	# find the name of the diagnostics output file
	my $diagnostics_file = $jobstep->{'properties'}->{'diagFile'};

	# parse the output
	my $diagnostics;
	if($diagnostics_file)
	{
		# read the file from the workspace
		my $full_diagnostics_file = join_paths($jobstep->{'workspace_dir'}, $diagnostics_file);
		my $diagnostics_text = read_file($full_diagnostics_file);
		my $diagnostics_xpath = XML::XPath->new(xml => $diagnostics_text);

		# set the initial state
		$diagnostics = { num_errors => 0, num_warnings => 0, list => [] };
		
		# loop through all the output
		foreach my $diagnostics_node($diagnostics_xpath->findnodes("/diagnostics/diagnostic"))
		{
			my $type = $diagnostics_xpath->findvalue("./type", $diagnostics_node)->string_value();
			my $first_line = $diagnostics_xpath->findvalue("./firstLine", $diagnostics_node)->string_value();
			my $num_lines = $diagnostics_xpath->findvalue("./numLines", $diagnostics_node)->string_value();
			my $text = $diagnostics_xpath->findvalue("./message", $diagnostics_node)->string_value();
			push(@{$diagnostics->{'list'}}, { type => $type, first_line => $first_line, num_lines => $num_lines, text => $text });
			$diagnostics->{'num_errors'}++ if $type eq 'error';
			$diagnostics->{'num_warnings'}++ if $type eq 'warning';
		}
	}
	$diagnostics;
}

1;
