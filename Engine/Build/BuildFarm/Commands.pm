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
use Text::ParseWords;

# local modules
use Build;
use Dashboard;
use Workspace;
use Notifications;
use Utility;

# execute a command with the given arguments
sub execute_command
{
	my ($raw_arguments) = @_;

	# send all errors to stdout, so they appear in the right order. we don't need them in different streams.
	*STDERR = *STDOUT;

	# if we were given a string, split the command line into an array
	if(ref $raw_arguments ne ref [])
	{
		my $escaped_command_line = $raw_arguments;
		$escaped_command_line =~ s/\\/\\\\/g;
		$raw_arguments = [grep { $_ ne '' } quotewords('\s+', 0, $escaped_command_line)];
	}

	# print the full command line if we're running inside a job. we don't need to do so from the command line, because we just entered it.
	if($ENV{COMMANDER_JOBID})
	{
		print "Command: ".join(' ', (map { / /? "\"$_\"" : $_ } @{$raw_arguments}))."\n";
		print "\n";
	}
	
	# parse all the arguments into a hash
	my $arguments = {};
	my $command;
	my $pass_through_arguments = "";
	for(my $i = 0; $i <= $#{$raw_arguments}; $i++)
	{
		my $argument = $raw_arguments->[$i];
		if(!defined $argument)
		{
			next;
		}
		elsif($argument eq '--')
		{
			$pass_through_arguments = join(' ', map { quote_argument($_) } @{$raw_arguments}[$i+1..$#{$raw_arguments}]), last;
		}
		elsif($argument =~ m/^--([a-zA-Z0-9_-]+)$/)
		{
			$arguments->{lc $1}=1
		}
		elsif($argument =~ m/^--([a-zA-Z0-9_-]+)=(.*)/)
		{
			$arguments->{lc $1}=$2
		}
		elsif($argument =~ m/^[a-zA-Z0-9_-]+$/ && !defined $command)
		{
			$command = lc $argument;
		}
		else
		{
			fail("Badly formatted command line ('$argument').");
		}
	}
	fail("Missing command") if !defined $command;

	# default directory
	my $root_dir = $arguments->{'root'};
	$root_dir = is_windows()? 'D:\\Build' : "$ENV{'HOME'}/Build" if !defined $root_dir;

	# construct the EC instance
	my $ec = new ElectricCommander;
	$ec->setTimeout(600);

	# get the global ec properties
	my $ec_project = 'GUBP_V5';
	my $ec_job = $arguments->{'ec-job'} || $ENV{'COMMANDER_JOBID'};
	my $ec_jobstep = $arguments->{'ec-jobstep'} || $ENV{'COMMANDER_JOBSTEPID'};
	my $ec_server = $arguments->{'ec-server'} || $ENV{'COMMANDER_SERVER'};
	my $ec_update = $arguments->{'ec-update'};

	# update the resource name for this subprocedure if necessary
	my $set_exclusive_resource = $arguments->{'set-exclusive-resource'};
	if($set_exclusive_resource)
	{
		ec_set_property($ec, "/myCall/Exclusive Resource", $set_exclusive_resource, $ec_update);
	}

	# check for the command name
	if($command eq 'checkforchanges')
	{
		my $stream = get_required_parameter($arguments, 'stream');
		check_for_changes($ec, $ec_project, $ec_job, $stream, $ec_update);
	}
	elsif($command eq 'getlatestchange')
	{
		my $stream = get_required_parameter($arguments, 'stream');
		my $change = get_latest_change($stream);
		get_latest_code_change($stream, $change);
	}
	elsif($command eq 'setupworkspace')
	{
		my $stream = get_required_parameter($arguments, 'stream');
		setup_workspace($root_dir, $stream, { ec => $ec, ec_project => $ec_project, agent_type => $arguments->{'agent-type'} });
	}
	elsif($command eq 'cleanworkspace')
	{
		my $stream = get_required_parameter($arguments, 'stream');
		my $agent_type = get_required_parameter($arguments, 'agent-type');
		
		my $workspace = setup_workspace($root_dir, $stream, { ec => $ec, ec_project => $ec_project, agent_type => $agent_type });
		terminate_processes($workspace, $root_dir);
		clean_workspace($workspace);
	}
	elsif($command eq 'syncworkspace')
	{
		my $stream = get_required_parameter($arguments, 'stream');
		my $change = get_job_change_number($ec, $ec_job, $stream, $arguments, $ec_update);
		my $unshelve_change = $arguments->{'unshelve-change'}; # deliberately different to preflight-change argument used by build setup, so we can use this procedure to clean the workspace

		if($arguments->{'autosdk'} && is_windows())
		{
			my $autosdk_workspace = setup_autosdk_workspace($root_dir);
			sync_autosdk_workspace($autosdk_workspace);
			setup_autosdk_environment($root_dir);
		}
		
		my $workspace = setup_workspace($root_dir, $stream, { ec => $ec, ec_project => $ec_project, agent_type => $arguments->{'agent-type'} });
		terminate_processes($workspace, $root_dir);
		sync_workspace($workspace, { change => $change, unshelve_change => $unshelve_change, show_traffic => $arguments->{'show-traffic'} });
	}
	elsif($command eq 'setupautosdkworkspace')
	{
		setup_autosdk_workspace($root_dir);
	}
	elsif($command eq 'syncautosdkworkspace')
	{
		my $autosdk_workspace = setup_autosdk_workspace($root_dir);
		sync_autosdk_workspace($autosdk_workspace);
	}
	elsif($command eq 'buildsetup')
	{
		my $stream = get_required_parameter($arguments, 'stream');
		my $change = get_job_change_number($ec, $ec_job, $stream, $arguments, $ec_update);
		my $code_change = get_job_code_change_number($ec,  $ec_job, $stream, $change, $arguments, $ec_update);
		my $resource_name = $set_exclusive_resource || get_required_parameter($arguments, 'resource-name');
		my $target = get_required_parameter($arguments, 'target');
		my $token_signature = $arguments->{'token-signature'} || ec_get_full_url("/commander/link/jobDetails/jobs/$ec_job");

		ec_set_property($ec, "/myCall/postSummary", "Hosted by $resource_name", $ec_update);

		my $trigger = $arguments->{'trigger'};
		if($trigger)
		{
			my $upstream_job = ec_get_property($ec, "/jobs[$ec_job]/Upstream Job");
			ec_set_property($ec, "/jobs[$ec_job]/report-urls/Parent Job", "/commander/link/jobDetails/jobs/$upstream_job?linkPageType=jobDetails", $ec_update);
			
			my $counter = ec_get_property($ec, "/increment /jobs[$upstream_job]/Triggers/$trigger", $ec_update);
			if($counter != 1)
			{
				print "This trigger has already been activated.\n";
				ec_set_property($ec, "/myJobStep/postSummary", "Trigger has already been activated.", $ec_update);
				exit 1;
			}
			
			ec_delete_property($ec, "/jobs[$upstream_job]/report-urls/Trigger: $trigger", $ec_update);
			ec_set_property($ec, "/jobs[$upstream_job]/report-urls/Trigger: $trigger (Activated)", "/commander/link/jobDetails/jobs/$ec_job?linkPageType=jobDetails", $ec_update);
		}
		
		my $preflight_change = $arguments->{'preflight-change'};
		if($preflight_change)
		{
			$pass_through_arguments .= " -set:PreflightChange=$preflight_change -reportName=\"Preflight Summary\"";
		}
		
		my $start_time = time;
		if($arguments->{'autosdk'} && is_windows())
		{
			my $autosdk_workspace = setup_autosdk_workspace($root_dir);
			sync_autosdk_workspace($autosdk_workspace);
			setup_autosdk_environment($root_dir);
		}
		my $settings = read_stream_settings($ec, $ec_project, $stream);
		my $build_script = ($settings->{'Default Build Script'}? ($arguments->{'build-script'} || $settings->{'Default Build Script'}) : get_required_parameter($arguments, 'build-script'));
		my $agent_type = $settings->{'Initial Agent Type'};
		my $workspace = setup_workspace($root_dir, $stream, { ec => $ec, ec_project => $ec_project, agent_type => $agent_type });
		terminate_processes($workspace, $root_dir);
		sync_workspace($workspace, { change => $change, unshelve_change => $arguments->{'preflight-change'}, show_traffic => $arguments->{'show-traffic'}, ensure_recent => !$arguments->{'allow-old-change'} }) unless $arguments->{'skip-sync'};
		my $sync_time = time;
		my $shared_storage_block = $arguments->{'temp-storage-block'} || $arguments->{'shared-storage-block'} || get_shared_storage_block($ec, $ec_job);
		my $shared_storage_dir = get_shared_storage_dir($stream, $settings, $agent_type, $shared_storage_block);
		build_setup($workspace, $change, $code_change, $build_script, $target, $trigger, $token_signature, $shared_storage_dir, $pass_through_arguments);
		my $uat_time = time;
		build_job_setup($ec, $ec_project, $ec_job, $ec_update, $settings, $workspace, $change, $build_script, $target, $token_signature, $shared_storage_block, $arguments, $pass_through_arguments, { email_only => $arguments->{'email-only'} });
		build_agent_setup($ec, $ec_job, $workspace, $change, $code_change, $build_script, $target, 'Startup', $token_signature, $resource_name, $shared_storage_dir, $ec_update, { fake_build => $arguments->{'fake-build'}, fake_fail => $arguments->{'fake-fail'}, pass_through_arguments => $pass_through_arguments, email_only => $arguments->{'email-only'}, autosdk => $arguments->{'autosdk'} });
		my $finish_time = time;
		ec_set_property($ec, "/myJobStep/postSummary", "Setup completed in ".format_time($finish_time - $start_time). " (Sync: ".format_time($sync_time - $start_time).", UAT: ".format_time($uat_time - $sync_time).", EC: ".format_time($finish_time - $uat_time).")", $ec_update);
	}
	elsif($command eq 'buildagentsetup')
	{
		my $stream = get_required_parameter($arguments, 'stream');
		my $agent_type = get_required_parameter($arguments, 'agent-type');
		my $change = get_required_parameter($arguments, 'change');
		my $code_change = get_required_parameter($arguments, 'code-change');
		my $resource_name = get_required_parameter($arguments, 'resource-name');
		my $group = get_required_parameter($arguments, 'group');
		my $build_script = get_required_parameter($arguments, 'build-script');
		my $target = get_required_parameter($arguments, 'target');
		my $shared_storage_dir = get_required_parameter($arguments, 'shared-storage-dir');
		my $token_signature = get_required_parameter($arguments, 'token-signature');

		ec_set_property($ec, "/myCall/postSummary", "Hosted by $resource_name", $ec_update);
		
		my $start_time = time;
		if($arguments->{'autosdk'} && is_windows())
		{
			my $autosdk_workspace = setup_autosdk_workspace($root_dir);
			sync_autosdk_workspace($autosdk_workspace);
			setup_autosdk_environment($root_dir);
		}
		my $workspace = setup_workspace($root_dir, $stream, { ec => $ec, ec_project => $ec_project, agent_type => $agent_type });
		terminate_processes($workspace, $root_dir);
		sync_workspace($workspace, { change => $change, unshelve_change => $arguments->{'preflight-change'}, show_traffic => $arguments->{'show-traffic'} }) unless $arguments->{'skip-sync'};
		print "Copying UAT binaries...\n";
		copy_recursive(join_paths(getcwd(), "UAT"), $workspace->{'dir'});
		my $sync_time = time;
		build_agent_setup($ec, $ec_job, $workspace, $change, $code_change, $build_script, $target, $group, $token_signature, $resource_name, $shared_storage_dir, $ec_update, { fake_build => $arguments->{'fake-build'}, fake_fail => $arguments->{'fake-fail'}, pass_through_arguments => $pass_through_arguments, temp_storage_dir => $arguments->{'temp-storage-dir'}, email_only => $arguments->{'email-only'}, autosdk => $arguments->{'autosdk'} });
		my $finish_time = time;
		ec_set_property($ec, "/myJobStep/postSummary", "Setup completed in ".format_time($finish_time - $start_time). " (Sync: ".format_time($sync_time - $start_time).", EC: ".format_time($finish_time - $sync_time).")", $ec_update);
	}
	elsif($command eq 'buildsinglenode')
	{
		my $stream = get_required_parameter($arguments, 'stream');
		my $change = get_required_parameter($arguments, 'change');
		my $workspace_name = get_required_parameter($arguments, 'workspace-name');
		my $workspace_dir = get_required_parameter($arguments, 'workspace-dir');
		my $build_script = get_required_parameter($arguments, 'build-script');
		my $node = get_required_parameter($arguments, 'node');
		my $token_signature = get_required_parameter($arguments, 'token-signature');

		if($arguments->{'autosdk'} && is_windows())
		{
			setup_autosdk_environment($root_dir);
		}

		build_single_node($ec, $ec_project, $ec_job, $ec_jobstep, $stream, $change, $workspace_name, $workspace_dir, $build_script, $node, $token_signature, $ec_update, { fake_build => $arguments->{'fake-build'}, fake_fail => $arguments->{'fake-fail'}, email_only => $arguments->{'email-only'}, pass_through_arguments => $pass_through_arguments });
	}
	elsif($command eq 'buildreportsetup')
	{
		my $stream = get_required_parameter($arguments, 'stream');
		my $change = get_required_parameter($arguments, 'change');
		my $report_name = $arguments->{'trigger-name'} || get_required_parameter($arguments, 'report-name');
		my $token_signature = get_required_parameter($arguments, 'token-signature');
		my $shared_storage_block = get_required_parameter($arguments, 'shared-storage-block');

		build_report_setup($ec, $ec_job, $stream, $change, $report_name, $token_signature, $shared_storage_block, $ec_update, { email_only => $arguments->{'email-only'} });
	}
	elsif($command eq 'buildbadgesetup')
	{
		my $stream = get_required_parameter($arguments, 'stream');
		my $change = get_required_parameter($arguments, 'change');
		my $badge_name = $arguments->{'badge-name'};

		build_badge_setup($ec, $ec_project, $ec_job, $stream, $change, $badge_name, $ec_update);
	}
	elsif($command eq 'conformresources')
	{
		my $names = get_required_parameter($arguments, 'names');
		my $set_pools = $arguments->{'set-pools'};
		my $p4_clean = $arguments->{'p4-clean'};
		my $max_parallel = $arguments->{'max-parallel'};
		conform_resources($ec, $ec_project, $ec_job, $ec_jobstep, $names, $set_pools, $p4_clean, $max_parallel, $ec_update);
	}
	elsif($command eq 'conformresource')
	{
		my $name = get_required_parameter($arguments, 'name');

		my $set_pools = $arguments->{'set-pools'};
		if($set_pools && $ec_update)
		{
			my $resource = $ec->getResource($name);
			
			my @old_resource_pools = split /\s+/, $resource->findvalue("//pools")->string_value();
			foreach my $resource_pool(@old_resource_pools)
			{
				print "Removing from $resource_pool\n";
				$ec->removeResourcesFromPool($resource_pool, { resourceName => [ $name ] })
			}
		
			my @new_resource_pools = split /\s+/, $set_pools;
			foreach my $resource_pool(@new_resource_pools)
			{
				print "Adding to $resource_pool\n";
				$ec->addResourcesToPool($resource_pool, { resourceName => [ $name ] })
			}
		}
		
		my $p4_clean = $arguments->{'p4-clean'};
		conform_resource($ec, $ec_project, $name, $root_dir, $p4_clean);
	}
	elsif($command eq 'printbuildhistory')
	{
		my $stream = get_required_parameter($arguments, 'stream');
		my $node = get_required_parameter($arguments, 'node');
		print_build_history($ec, $ec_job, $stream, $node, 20, $arguments->{'before-change'});
	}
	elsif($command eq 'runuat')
	{
		my $stream = get_required_parameter($arguments, 'stream');
		my $agent_type = get_required_parameter($arguments, 'agent-type');
		my $resource_name = get_required_parameter($arguments, 'resource-name');
		my $change = get_job_change_number($ec, $ec_job, $stream, $arguments, $ec_update);
		my $code_change = get_job_code_change_number($ec, $ec_job, $stream, $change, $arguments, $ec_update);

		if($arguments->{'autosdk'} && is_windows())
		{
			my $autosdk_workspace = setup_autosdk_workspace($root_dir);
			sync_autosdk_workspace($autosdk_workspace);
			setup_autosdk_environment($root_dir);
		}
		
		my $workspace = setup_workspace($root_dir, $stream, { ec => $ec, ec_project => $ec_project, agent_type => $agent_type });
		run_uat($workspace, $change, $code_change, $pass_through_arguments);
	}
	elsif($command eq 'updatedashboard')
	{
		my $stream = get_required_parameter($arguments, 'stream');
		update_dashboard($ec, $ec_project, $stream, $ec_update, { no_cache => $arguments->{'no-cache'} });
	}
	elsif($command eq 'printsuspectedcausers')
	{
		my $stream = get_required_parameter($arguments, 'stream');
		my $jobstep_id = get_required_parameter($arguments, 'jobstep-id');
		my $min_change = get_required_parameter($arguments, 'min-change');

		my $workspace = setup_workspace($root_dir, $stream, { ec => $ec, ec_project => $ec_project, agent_type => $arguments->{'agent-type'} });
		print_suspected_causers($ec, $jobstep_id, $workspace, $min_change);
	}
	elsif($command eq 'writestepnotification')
	{
		my $stream = get_required_parameter($arguments, 'stream');
		my $ec_jobstep = get_required_parameter($arguments, 'ec-jobstep');
		
		my $workspace = setup_workspace($root_dir, $stream, { ec => $ec, ec_project => $ec_project, agent_type => $arguments->{'agent-type'} });
		write_step_notification($ec, $ec_project, $ec_jobstep, $workspace->{'name'}, $workspace->{'dir'}, $arguments->{'send-to'});
	}
	elsif($command eq 'writereportnotification')
	{
		my $report_name = get_required_parameter($arguments, 'report');
		fail("Missing --ec-job parameter") if !$ec_job;
		write_report_notification($ec, $ec_project, $ec_job, $report_name);
	}
	elsif($command eq 'findresourcepool')
	{
		my $stream = get_required_parameter($arguments, 'stream');
		my $agent_type = get_required_parameter($arguments, 'agent-type');

		my $settings = read_stream_settings($ec, $ec_project, $stream);
		my $resource_pool = $settings->{'Agent Types'}->{$agent_type}->{'Resource Pool'};
		ec_set_property($ec, "/myCall/Resource Pool", $resource_pool, $ec_update);
	}
	else
	{
		fail("Unknown command '$command'");
	}

	# delete the job if asked to
	if($arguments->{'delete-job-on-success'})
	{
		fail("Missing --ec-job=... parameter") if !defined $ec_job;
		$ec->deleteJob($ec_job);
	}
}

# get a parameter or default value
sub get_parameter_or_default
{
	my ($arguments, $name, $default_value) = @_;
	(exists $arguments->{$name})? $arguments->{$name} : $default_value;
}

# get a required parameter
sub get_required_parameter
{
	my ($arguments, $name) = @_;
	if(!exists $arguments->{$name})
	{
		print "Missing '--$name=...' parameter on command line.\n";
		exit 1;
	}
	$arguments->{$name};
}

########################################################################################################################################################################################################

# figure out which changelist the job is to be built at. Takes it from the --change parameter on the command line if possible, otherwise from the CL property on the job,
# otherwise from the latest change in the stream. In the latter case, the job is renamed to match the CL, and the job CL property is set.
sub get_job_change_number
{
	my ($ec, $ec_job, $stream, $arguments, $ec_update) = @_;

	my $change = $arguments->{'change'};
	if(!$change)
	{
		$change = ec_try_get_property($ec, "/jobs[$ec_job]/CL") if $ec_job;
		if(!$change)
		{
			$change = get_latest_change($stream);
			$arguments->{'change'} = $change;
			rename_job_with_change($ec, $ec_job, $change);
			ec_set_property($ec, "/jobs[$ec_job]/CL", $change, $ec_update) if $ec_job;
		}
	}
	$change;
}

# figure out which changelist the job is to be built at. Takes it from the --change parameter on the command line if possible, otherwise from the CL property on the job,
# otherwise from the latest change in the stream. In the latter case, the job is renamed to match the CL, and the job CL property is set.
sub get_job_code_change_number
{
	my ($ec, $ec_job, $stream, $change, $arguments, $ec_update) = @_;

	my $code_change = $arguments->{'code-change'};
	if(!$code_change)
	{
		$code_change = ec_try_get_property($ec, "/jobs[$ec_job]/CodeCL") if $ec_job;
		if(!$code_change)
		{
			$code_change = get_latest_code_change($stream, $change);
			$arguments->{'code-change'} = $code_change;
			ec_set_property($ec, "/jobs[$ec_job]/CodeCL", $code_change, $ec_update) if $ec_job;
		}
	}
	$code_change;
}

# gets the latest change for a given stream
sub get_latest_change
{
	my ($stream) = @_;
	
	# since we're running a manually triggered build, we want to take the latest change that's in this branch or any of its imports.
	# this is different to the logic in check_for_changes, where we want to filter out changes which actually happen in this stream.
	# create a full workspace, so we can check for the latest change with a client filespec.
	my $workspace_name = setup_temporary_workspace($stream);
	
	# find the last change submitted by a user
	my @change_lines = p4_command("-c $workspace_name changes -m 1 //$workspace_name/...");
	fail("Unexpected output line when querying changes: $change_lines[0]") if $change_lines[0] !~ m/^Change (\d+) on /;
	my $change = $1;
	print "Last submitted change in $stream/... is $change\n";

	# return the new changelist
	$change;
}

# gets the latest change for a given stream
sub get_latest_code_change
{
	my ($stream, $change) = @_;
	
	# since we're running a manually triggered build, we want to take the latest change that's in this branch or any of its imports.
	# this is different to the logic in check_for_changes, where we want to filter out changes which actually happen in this stream.
	# create a full workspace, so we can check for the latest change with a client filespec.
	my $workspace_name = setup_temporary_workspace($stream);
	
	# find the last change submitted by a user
	my @change_lines = p4_command("-c $workspace_name changes -m 1 //$workspace_name/....cpp\@$change //$workspace_name/....h\@$change //$workspace_name/....inl\@$change //$workspace_name/....cs\@$change //$workspace_name/....usf\@$change");

	# find the largest changelist from any of them
	my $code_change = 0;
	foreach(@change_lines)
	{
		fail("Unexpected output line when querying changes: $change_lines[0]") if !m/^Change (\d+) on /;
		$code_change = $1 if $1 > $code_change;
	}
	$code_change = $change if !$code_change;

	# print the result
	print "Last submitted code change in $stream/... is $code_change\n";

	# return the new changelist
	$code_change;
}

# gets the latest change for a given stream, and renames the job to match it
sub rename_job_with_change
{
	my ($ec, $ec_job, $change) = @_;
	if($ec && $ec_job)
	{
		# get the current job name
		my $response = $ec->getJobDetails($ec_job);
		my $job_name = $response->findvalue('/responses/response/job/jobName')->string_value();
		fail("Couldn't get job name for '$ec_job'") if !$job_name;

		# substitute the unique id string with the changelist
		my $new_job_name = $job_name;
		if($new_job_name =~ s/ UID \d+ - / CL $change - /)
		{
			$new_job_name =~ s/\s+\(#\d+\)$//;
			
			# make sure the new name is unique. if we find a job with the same name, use the UniqueNameCounter on it to determine the new name.
			my $existing_job_response = $ec->findObjects('job', { numObjects => 0, filter => [{ propertyName => "jobName", operator => "equals", operand1 => $new_job_name }] });
			foreach my $object_id ($existing_job_response->findnodes("/responses/response/objectId"))
			{
				$new_job_name .= ' (#'.(ec_get_property($ec, "/increment /jobs[$new_job_name]/UniqueNameCounter") + 1).')';
				last;
			}

			# rename the job
			print "Renaming job from '$job_name' to '$new_job_name'\n";
			$ec->setJobName($ec_job, $new_job_name);
		}
	}
}

1;