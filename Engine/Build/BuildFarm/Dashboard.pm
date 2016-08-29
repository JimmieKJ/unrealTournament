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
use Time::Local;
use HTML::Entities;

require Exporter;
our @EXPORT = (
		qw(update_all_dashboards update_dashboard)
	);

########################################################################################################################################################################################################

# update the cis and build pages for the given stream
sub update_dashboard
{
	my ($ec, $ec_project, $stream, $ec_update, $optional_arguments) = @_;
	
	# read the stream settings, and cache it under the dashboard link
	my $settings = read_stream_settings($ec, $ec_project, $stream);
	
	# get a list of jobs
	log_print("Querying job status for $stream from EC.");
	
	my $filter = [];
	push(@{$filter}, { propertyName => 'projectName', operator => 'equals', operand1 => $ec_project });
	push(@{$filter}, { propertyName => 'procedureName', operator => 'equals', operand1 => 'Run BuildGraph' });
	push(@{$filter}, { propertyName => 'Stream', operator => 'equals', operand1 => $stream });
	push(@{$filter}, { propertyName => 'Preflight CL', operator => 'equals', operand1 => '' });

	my $jobs_response = $ec->findObjects('job', { maxIds => 40, filter => $filter, sort => [{ propertyName => "createTime", order => "descending" }], select => [{ propertyName => 'CL'}, { propertyName => 'Arguments' }, { propertyName => 'CachedJobSteps' }, { propertyName => 'BuildInfo' }, { propertyName => 'Upstream Job' }] });
	
	# convert it to a list, with all the properties we care about
	my $job_ids = [];
	my $job_id_to_properties = {};
	foreach my $object ($jobs_response->findnodes("/responses/response/object"))
	{
		# add the job id to the list
		my $job_id = $jobs_response->findvalue("job/jobId", $object)->string_value();
		push(@{$job_ids}, $job_id);

		# create a properties hash for it
		my $properties = {};
		$job_id_to_properties->{$job_id} = $properties;
		
		# get the standard job properties
		$properties->{'job_id'} = $job_id;
		$properties->{'job_name'} = $jobs_response->findvalue("job/jobName", $object)->string_value();
		$properties->{'status'} = $jobs_response->findvalue("job/status", $object)->string_value();
		$properties->{'outcome'} = $jobs_response->findvalue("job/outcome", $object)->string_value();
		$properties->{'started_by'} = $jobs_response->findvalue("job/owner", $object)->string_value();
		$properties->{'aborted_by'} = $jobs_response->findvalue("job/abortedBy", $object)->string_value();
		$properties->{'error_code'} = $jobs_response->findvalue("job/errorCode", $object)->string_value();
		$properties->{'start_time'} = parse_sql_time($jobs_response->findvalue("job/start", $object)->string_value());
		$properties->{'elapsed_time'} = $jobs_response->findvalue("job/elapsedTime", $object)->string_value();
	
		# parse the custom properties
		foreach my $property ($jobs_response->findnodes("property", $object))
		{
			my $name = $jobs_response->findvalue("propertyName", $property)->string_value();
			my $value = $jobs_response->findvalue("value", $property)->string_value();
			if($name eq 'CL')
			{
				$properties->{'change_number'} = $value;
			}
			elsif($name eq 'Arguments')
			{
				$properties->{'arguments'} = $value;
			}
			elsif($name eq 'CachedJobSteps')
			{
				$properties->{'job_steps'} = decode_json($value);
			}
			elsif($name eq 'BuildInfo')
			{
				$properties->{'build_info'} = decode_json($value);
			}
			elsif($name eq 'Upstream Job')
			{
				$properties->{'upstream_job'} = $value;
			}
		}

		# fixup the canonical order for node names. NodesInGraph, the previous property giving the canonical node order for the graph, is being replaced by just sorting NodesInJob.
		my $build_info = $properties->{'build_info'};
		if($build_info && $build_info->{'NodesInGraph'})
		{
			my %nodes_in_job_lookup = (map { $_ => 1 } @{$build_info->{'NodesInJob'}});
			my @nodes_in_job = grep { $nodes_in_job_lookup{$_} } @{$build_info->{'NodesInGraph'}};
			$build_info->{'NodesInJob'} = \@nodes_in_job;
		}
		
		# if we don't already have the step results, query the server for them directly
		if(!defined $properties->{'job_steps'} || ($optional_arguments->{'no_cache'} && ($optional_arguments->{'no_cache'} == 1 || $optional_arguments->{'no_cache'} == $job_id)))
		{
			my $job_steps = [];
		
			# harvest the properties from the current job state. if we don't have a build_info, we didn't complete job setup yet (so the rest is moot).
			if($build_info)
			{
				# build a lookup of valid nodes in this job. we only care about the status of these nodes.
				my %valid_node_names = (map { $_ => 1 } @{$build_info->{'NodesInJob'}});
		
				# query the server
				log_print("Querying detailed status of job $job_id...");
				my $job_steps_response = $ec->findJobSteps({ jobId => $job_id, select => [{ propertyName => 'Telemetry' }] });
				
				# parse a list of names and mapping from name to outcome
				my $node_to_outcome = {};
				foreach my $job_step_object ($job_steps_response->findnodes("/responses/response/object"))
				{
					my $job_step = $job_steps_response->findnodes("./jobStep", $job_step_object)->get_node(1);
					my $id = $job_steps_response->findvalue("jobStepId", $job_step)->string_value();
					
					my $status = $job_steps_response->findvalue("status", $job_step)->string_value();
					my $outcome = $job_steps_response->findvalue("outcome", $job_step)->string_value();
					my $error_code = $job_steps_response->findvalue("errorCode", $job_step)->string_value();
					my $abort_status = $job_steps_response->findvalue("abortStatus", $job_step)->string_value();
					my $start_time = parse_sql_time($job_steps_response->findvalue("start", $job_step)->string_value());
					my $run_time = $job_steps_response->findvalue("runTime", $job_step)->string_value();
					
					my $result = $status;
					if($status eq 'completed')
					{
						$result .= ":$outcome";
						if($outcome eq 'error')
						{
							if($error_code)
							{
								$result .= ":$error_code";
							}
							elsif($abort_status eq 'FORCE_ABORT')
							{
								$result .= ":ABORTED";
							}
						}
					}
					
					my @outcome_fields = ( id => $id, result => $result, status => $status, outcome => $outcome, error => $error_code || $abort_status, start_time => $start_time, run_time => $run_time );

					foreach my $property ($job_steps_response->findnodes("./property", $job_step_object))
					{
						if($job_steps_response->findvalue("./propertyName", $property)->string_value() eq 'Telemetry')
						{
							my $value = $job_steps_response->findvalue("./value", $property)->string_value();
							push(@outcome_fields, telemetry => decode_json($value));
							last;
						}
					}

					my $name = $job_steps_response->findvalue("stepName", $job_step)->string_value();
					if($valid_node_names{$name})
					{
						$node_to_outcome->{$name} = { name => $name, @outcome_fields };
					}
					else
					{
						my $node_names = $build_info->{'GroupToNodes'}->{$name};
						if($node_names)
						{
							foreach(@{$node_names})
							{
								$node_to_outcome->{$_} = { name => $_, @outcome_fields };
							}
						}
					}
				}
				
				# reorder the list of job steps to match the build order
				foreach(@{$build_info->{'NodesInJob'}})
				{
					push(@{$job_steps}, $node_to_outcome->{$_}) if $node_to_outcome->{$_};
				}
			}

			# if this job has completed, cache the results - they can't change from this point onwards
			if($properties->{'status'} eq 'completed')
			{
				my $property_name = "/jobs[$job_id]/CachedJobSteps";
				log_print("Job complete; caching to $property_name");
				$ec->setProperty($property_name, encode_json($job_steps));
			}
			
			# add them to the properties array
			$properties->{'job_steps'} = $job_steps;
		}
	}
	
	# update the builds page
	update_settings($ec, $ec_project, $stream, $settings, $ec_update);
	update_latest_builds($ec, $ec_project, $stream, $settings, $ec_update);
	update_build_links($ec, $ec_project, $stream, $settings, $ec_update);
	update_custom_build_link($ec, $ec_project, $stream, $ec_update);
	update_preflight_links($ec, $ec_project, $stream, $settings, $ec_update);
	update_telemetry($ec, $ec_project, $stream, $job_ids, $job_id_to_properties, $ec_update);
	update_dashboard_grid_view($ec, $ec_project, $stream, $job_ids, $job_id_to_properties, $settings, $ec_update);
	update_dashboard_list_view($ec, $ec_project, $stream, $job_ids, $job_id_to_properties, $settings, $ec_update);
	update_github_view($ec, $ec_project, $stream, $settings, $ec_update);
}

# update the settings property in EC
sub update_settings
{
	my ($ec, $ec_project, $stream, $settings, $ec_update) = @_;
	
	# store the full settings object
	my $json = encode_json($settings);
	ec_set_property($ec, "/projects[$ec_project]/Generated/".escape_stream_name($stream)."/Settings", $json, $ec_update);

	# store the initial resource pool for easy access on new jobs
	my $initial_agent_type = $settings->{'Initial Agent Type'};
	if($initial_agent_type)
	{
		my $agent_type_definition = $settings->{'Agent Types'}->{$initial_agent_type};
		if($agent_type_definition)
		{
			my $resource_pool = $agent_type_definition->{'Resource Pool'};
			if($resource_pool)
			{
				ec_set_property($ec, "/projects[$ec_project]/Generated/".escape_stream_name($stream)."/Initial Resource Pool", $resource_pool, $ec_update);
			}
		}
	}
}

# remove any outdated items from the latest builds list
sub update_latest_builds
{
	my ($ec, $ec_project, $stream, $settings, $ec_update) = @_;

	# get the time at which to remove entries
	my $delete_time = time - (7 * 24 * 60 * 60);
	log_print("Removing 'Latest Build' entries older than $delete_time...");
	
	# make sure the property sheet exists
	my $latest_path = "/projects[$ec_project]/Generated/".escape_stream_name($stream)."/Latest";
	ec_set_property($ec, "$latest_path/_placeholder", "", 1); 
	
	# read all the properties
	my $latest_sheet = ec_get_property_sheet($ec, { path => $latest_path }) || {};
	foreach my $key(keys %{$latest_sheet})
	{
		if($key ne '_placeholder')
		{
			my $latest_build;
			eval { $latest_build = decode_json($latest_sheet->{$key}); };
			
			my $build_time = ($latest_build && $latest_build->{'time'}) || 0;
			if($build_time < $delete_time)
			{
				my $build_path = "$latest_path/$key";
				log_print("Removing '$build_path' (time $build_time)");
				if($ec_update)
				{
					$ec->deleteProperty($build_path);
				}
				else
				{
					log_print("Skipping delete without --ec-update");
				}
			}
		}
	}
}

# update the list of new builds for the current stream
sub update_build_links
{
	my ($ec, $ec_project, $stream, $settings, $ec_update) = @_;

	my $build_links = [];
	my $build_types = $settings->{'Build Types'};
	if($build_types)
	{
		foreach my $build_type(@{$build_types})
		{
			if($build_type->{'ShowLink'})
			{
				my $name = $build_type->{'Name'};
				
				my $link = "/commander/link/runProcedure/projects/$ec_project/procedures/".encode_form_parameter("Run BuildGraph")."?runNow=1&priority=high";
				$link .= "&parameters1_name=".encode_form_parameter("Stream");
				$link .= "&parameters1_value=".encode_form_parameter($stream);
				$link .= "&parameters2_name=".encode_form_parameter("Job Name");
				$link .= "&parameters2_value=".encode_form_parameter($name);
				$link .= "&parameters3_name=".encode_form_parameter("Arguments");
				$link .= "&parameters3_value=".encode_form_parameter($build_type->{'Arguments'});

				push(@{$build_links}, "$name=$link");
			}
		}
	}
	
	ec_set_property($ec, "/projects[$ec_project]/Generated/".escape_stream_name($stream)."/Dashboard/BuildLinks", join("\n", @{$build_links}), $ec_update);
}

# sets the custom build link for this stream
sub update_custom_build_link
{
	my ($ec, $ec_project, $stream, $ec_update) = @_;

	my $link = "/commander/link/runProcedure/projects/$ec_project/procedures/".encode_form_parameter("Run BuildGraph")."?runNow=1&priority=high";
	$link .= "&parameters1_name=".encode_form_parameter("Stream");
	$link .= "&parameters1_value=".encode_form_parameter($stream);
	$link .= "&parameters2_name=".encode_form_parameter("Job Name");
	$link .= "&parameters2_value=".encode_form_parameter("Custom (").'$(Nodes)'.encode_form_parameter(")");
	$link .= "&parameters3_name=".encode_form_parameter("Arguments");
	$link .= "&parameters3_value=".encode_form_parameter("--Target=\"").'$(Nodes)'.encode_form_parameter("\" ").'$(Arguments)';
	
	ec_set_property($ec, "/projects[$ec_project]/Generated/".escape_stream_name($stream)."/Dashboard/CustomBuildLink", $link, $ec_update);
}

# update the list of new builds for the current stream
sub update_preflight_links
{
	my ($ec, $ec_project, $stream, $settings, $ec_update) = @_;

	my $preflight_links = [];
	my $build_types = $settings->{'Build Types'};
	if($build_types)
	{
		foreach my $build_type(@{$build_types})
		{
			if($build_type->{'ShowPreflight'})
			{
				my $name = $build_type->{'Name'};
				my $link = get_preflight_url($ec_project, $stream, encode_form_parameter($name), encode_form_parameter($build_type->{'Arguments'}));
				push(@{$preflight_links}, "$name=$link");
			}
		}
	}
	
	my $custom_name = encode_form_parameter('Custom (').'$(Nodes)'.encode_form_parameter(')');
	my $custom_arguments = encode_form_parameter("--Target=\"").'$(Nodes)'.encode_form_parameter("\" -- ").'$(Arguments)';
	push(@{$preflight_links}, "Custom...=".get_preflight_url($ec_project, $stream, $custom_name, $custom_arguments));
	
	ec_set_property($ec, "/projects[$ec_project]/Generated/".escape_stream_name($stream)."/Dashboard/PreflightLinks", join("\n", @{$preflight_links}), $ec_update);
}

# creates a preflight link url
sub get_preflight_url
{
	my ($ec_project, $stream, $name, $arguments) = @_;

	my $url = "/commander/link/runProcedure/projects/$ec_project/procedures/".encode_form_parameter("Run BuildGraph")."?runNow=1&priority=low";
	$url .= "&parameters1_name=".encode_form_parameter("Stream");
	$url .= "&parameters1_value=".encode_form_parameter($stream);
	$url .= "&parameters2_name=".encode_form_parameter("CL");
	$url .= "&parameters2_value=\$(BaseCL)";
	$url .= "&parameters3_name=".encode_form_parameter("Preflight CL");
	$url .= "&parameters3_value=\$(ShelvedCL)";
	$url .= "&parameters4_name=".encode_form_parameter("Job Name");
	$url .= "&parameters4_value=$name";
	$url .= "&parameters5_name=".encode_form_parameter("Arguments");
	$url .= "&parameters5_value=$arguments";
	
	$url;
}

# update the telemetry property
sub update_telemetry
{
	my ($ec, $ec_project, $stream, $job_ids, $job_id_to_properties, $ec_update) = @_;
	log_print("Generating telemetry for $stream...");

	# read the existing telemetry
	my $property_name = "/projects[$ec_project]/Generated/".escape_stream_name($stream)."/Telemetry";
	my $telemetry = decode_json(ec_try_get_property($ec, $property_name) || "{}");
	my $job_telemetry_array = ($telemetry && $telemetry->{'version'} && $telemetry->{'version'} == 1)? $telemetry->{'jobs'} : [];

	# remove any jobs from the list that we've got data for
	$job_telemetry_array = [grep { !exists $job_id_to_properties->{$_->{'id'}} } @{$job_telemetry_array} ];
	
	# make a lookup from job id to the telemetry for it
	my $job_id_to_telemetry = { };
	foreach my $job_telemetry (@{$job_telemetry_array})
	{
		$job_id_to_telemetry->{$job_telemetry->{'id'}} = $job_telemetry;
	}

	# add rows for any jobs we're not already tracking
	foreach my $job_id (@{$job_ids})
	{
		my $job_properties = $job_id_to_properties->{$job_id};
		if($job_properties->{'build_info'})
		{
			# build a lookup of node name to group name
			my $node_name_to_group_name = {};
			my $group_name_to_node_names = $job_properties->{'build_info'}->{'GroupToNodes'};
			foreach my $group_name (keys %{$group_name_to_node_names})
			{
				foreach my $node_name (@{$group_name_to_node_names->{$group_name}})
				{
					$node_name_to_group_name->{$node_name} = $group_name;
				}
			}

			# check we have a dependencies list for this job; we can't build telemetry data without it
			my $node_name_to_dependencies =  $job_properties->{'build_info'}->{'NodeToDependencies'};
			if($node_name_to_dependencies)
			{
				my $job_telemetry = { id => $job_id, name => $job_properties->{'job_name'}, change_number => $job_properties->{'change_number'}, start_time => $job_properties->{'start_time'}, job_steps => [] };
				foreach my $jobstep (@{$job_properties->{'job_steps'}})
				{
					my $result = $jobstep->{'result'};
					if($result =~ /^completed:(success|warning)/ && $jobstep->{'run_time'})
					{
						my $jobstep_id = $jobstep->{'id'};
						my $name = $jobstep->{'name'};
						my $group = $node_name_to_group_name->{$name};
						my $start_time = $jobstep->{'start_time'};
						my $finish_time = $start_time + int ($jobstep->{'run_time'} / 1000);
						my $depends_on = $job_properties->{'build_info'}->{'NodeToDependencies'}->{$name};
						my $user_data = $jobstep->{'telemetry'};
						
						my $jobstep_telemetry = { id => $jobstep_id, name => $name, group => $group, start_time => $start_time, finish_time => $finish_time, depends_on => $depends_on };
						$jobstep_telemetry->{'user_data'} = $user_data if $user_data;
						push(@{$job_telemetry->{'job_steps'}}, $jobstep_telemetry);
					}
				}
				push(@{$job_telemetry_array}, $job_telemetry);
			}
		}
	}

	# get the oldest time to retain telemetry
	my $cull_time = time - (14 * 24 * 60 * 60);
	$job_telemetry_array = [ grep { $_->{'start_time'} > $cull_time } @{$job_telemetry_array} ];
	
	# Update EC
	my $new_json = encode_json({ version => 1, jobs => $job_telemetry_array });
	ec_set_property($ec, $property_name, $new_json, $ec_update);
}

# update the grid dashboard for a given stream
sub update_dashboard_grid_view
{
	my ($ec, $ec_project, $stream, $job_ids, $job_id_to_properties, $settings, $ec_update) = @_;
	log_print("Generating grid view for $stream...");

	# remove all the job ids that don't have a valid CL set, or are downstream of another job
	$job_ids = [ grep { !$job_id_to_properties->{$_}->{'upstream_job'} && $job_id_to_properties->{$_}->{'change_number'} =~ /^\d+$/ } @{$job_ids} ];
	
	# remove all the blacklisted job names as specified by the settings file
	my $grid_settings = $settings && $settings->{'Dashboard'} && $settings->{'Dashboard'}->{'Grid'};
	if($grid_settings)
	{
		my $exclude_jobs = $grid_settings->{'Exclude Jobs'};
		if($exclude_jobs)
		{
			# create a new array of job ids that don't match any exclude patterns
			my $new_job_ids = [];
			foreach my $job_id(@{$job_ids})
			{
				my $base_job_name = get_base_job_name($job_id_to_properties->{$job_id}->{'job_name'});
				push(@{$new_job_ids}, $job_id) if !(grep { $base_job_name =~ /^$_$/ } @{$exclude_jobs});
			}
			$job_ids = $new_job_ids;
		}
	}
	
	# find the count of each node name, and order in which we encounter them (from the first job to the last)
	my $node_name_to_weight = { };
	for my $job_id (@{$job_ids})
	{
		my $job_properties = $job_id_to_properties->{$job_id};
		
		my $build_info = $job_properties->{'build_info'};
		next if !$build_info;

		my $nodes_in_job = $build_info->{'NodesInJob'};
		for(my $i = 0; $i <= $#{$nodes_in_job}; $i++)
		{
			my $node_name = $nodes_in_job->[$i];
			$node_name_to_weight->{$node_name} += ((1000.0 - $i) / 1000.0) ** 0.5;
		}
	}
	
	# order the list of job names by count then index
	my $node_names = [sort { -($node_name_to_weight->{$a} <=> $node_name_to_weight->{$b}) } keys %{$node_name_to_weight}];

	# build a lookup from each job id to a hash of its nodes
	my $job_id_to_nodes = {};
	for my $job_id (@{$job_ids})
	{
		my $nodes = {};
	
		my $job_properties = $job_id_to_properties->{$job_id};
		if($job_properties)
		{
			my $job_step_properties = $job_properties->{'job_steps'};
			if($job_step_properties)
			{
				foreach my $node (@{$job_step_properties})
				{
					$nodes->{$node->{'name'}} = $node;
				}
			}
		}
		
		$job_id_to_nodes->{$job_id} = $nodes;
	}
		
	# create the html boilerplate
	my $XHTML = "";	
	$XHTML .= "<link href='/commander/styles/JobsQuickView.css' rel='stylesheet' type='text/css' />\n";
	$XHTML .= "<link href='/commander/lib/styles/ColumnedSections.css' rel='stylesheet' type='text/css' />\n";
	$XHTML .= "<link href='/commander/lib/styles/Header.css' rel='stylesheet' type='text/css' />\n";
	$XHTML .= "<style type=\"text/css\">\n";
	$XHTML .= "  .cis td { white-space:nowrap; align:center; }\n";
	$XHTML .= "</style>\n";
	$XHTML .= "<table class='formBuilder data quickviewTable cis' style='position:absolute;width:100%;'>\n";

	# Header row
	$XHTML .= "  <tr>";
	$XHTML .= "    <td width=\"460px\"></td>\n";
	for my $job_id (@{$job_ids})
	{
		my $job_properties = $job_id_to_properties->{$job_id};
		my $heading = $job_properties->{'change_number'} || "Latest";
		my $started_time = "Started ".format_recent_time($job_properties->{'start_time'});
		$XHTML .= "<td bgcolor=#C8C8CE height=\"20\" width=\"65px\" align=\"center\" style=\"padding-left:0.75em;padding-right:0.75em;\" title=\"$started_time\"><a href=/commander/link/jobDetails/jobs/$job_id\><strong>$heading</strong></a></td>";
	}
	$XHTML .= "  </tr>\n";
		
	# Body rows
	my $row_idx = 0;
	foreach my $node_name (@{$node_names})
	{
		$XHTML .= "  <tr bgcolor='".((($row_idx++ & 1) == 0)? "#dedee4" : "#f1f1f4")."'>\n";

		# create a link to a search for all nodes of this type
		my $search_link = "/commander/link/searchBuilder?formId=editSearch";
		$search_link .= "&objectType=jobStep";
		$search_link .= "&maxIds=200";
		$search_link .= "&maxResults=40";
		$search_link .= "&filtersJobStep1_intrinsic_name=stepName";
		$search_link .= "&filtersJobStep1_intrinsic_operator=equals";
		$search_link .= "&filtersJobStep1_intrinsic_operand1=".encode_form_parameter($node_name);
		$search_link .= "&filtersJobStep_intrinsic_last=2";
		$search_link .= "&filtersJobStep1_custom_name=Stream";
		$search_link .= "&filtersJobStep1_custom_operator=equals";
		$search_link .= "&filtersJobStep1_custom_operand1=".encode_form_parameter($stream);
		$search_link .= "&filtersJobStep_custom_last=2";
		$search_link .= "&sort1_name=createTime";
		$search_link .= "&sort1_order=descending";
		$search_link .= "&sort_last=2";
		$search_link .= "&shortcutName=Job+Step+Search+Results";
		$search_link .= "&redirectTo=/commander/link/searchResults?filterName=jobStepSearch&reload=jobStep&s=JobSteps";

		# add the node name, with a link to a search for all nodes of that type
		$XHTML .= "    <td style=\"align:left; padding-left:2em; padding-right:2em;\"><a href=\"$search_link\">$node_name</a></strong></td>\n";

		# add the results for each job
		for my $job_id (@{$job_ids})
		{
			my $cell = "";
		
			my $node = $job_id_to_nodes->{$job_id}->{$node_name};
			if($node)
			{
				my $result = $node->{'result'};
				
				# get the image to display for this cell
				my $image;
				if($result eq 'completed:success')
				{
					$image = "<img src=\"/commander/lib/images/icn16px_success.gif\" width=\"16px\" height=\"16px\" />";
				}
				elsif($result eq 'completed:error:ABORTED')
				{
					$image = "<img src=\"/commander/lib/images/icn16px_stopped.gif\" width=\"16px\" height=\"16px\" />";
				}
				elsif($result =~ /^completed:error/)
				{
					$image = "<img src=\"/commander/lib/images/icn16px_error.gif\" width=\"16px\" height=\"16px\" />";
				}
				elsif($result eq 'completed:skipped' || $result eq 'completed:error:CANCELED')
				{
					$image = "<img src=\"/commander/lib/images/icn16px_skipped.gif\" width=\"16px\" height=\"16px\" />";
				}
				elsif($result eq 'completed:warning')
				{
					$image = "<img src=\"/commander/lib/images/icn16px_warning.gif\" width=\"16px\" height=\"16px\" />";
				}
				elsif($node->{'status'} eq 'running')
				{
					$image = "<img src=\"/commander/lib/images/icn16px_running_$node->{'outcome'}.gif\" width=\"16px\" height=\"16px\" />";
				}
				elsif($node->{'status'} eq 'pending' || $node->{'status'} eq 'runnable')
				{
					$image = "<img src=\"/commander/lib/images/icn16px_runnable.gif\" width=\"16px\" height=\"16px\" />";
				}
				else
				{
					$image = $result;
				}

				# format the cell contents
				my $link = get_link_to_job_step($job_id_to_properties->{$job_id}, $node);
				$cell = "<a href=\"$link\" style=\"background-image:none;\">$image</a>";
			}
		
			$XHTML .= "    <td width=\"65px\" height=\"20\" align=\"center\">$cell</td>\n";
		}
		$XHTML .= "  </tr>\n";
	}
	$XHTML .= "</table>\n";
	
	ec_set_property($ec, "/projects[$ec_project]/Generated/".escape_stream_name($stream)."/Dashboard/GridView", $XHTML, $ec_update);
}

# gets the link to a jobstep
sub get_link_to_job_step
{
	my ($job_properties, $job_step_properties) = @_;
	
	my $escaped_job_name = encode_form_parameter($job_properties->{'job_name'});
	my $escaped_job_step_name = encode_form_parameter($job_step_properties->{'name'});
	
	"/commander/link/jobStepDetails/jobSteps/$job_step_properties->{'id'}?stepName=$escaped_job_step_name&jobId=$job_properties->{'job_id'}&jobName=$escaped_job_name&tabGroup=diagnosticHeader";
}

# updates the builds view for a stream
sub update_dashboard_list_view
{
	my ($ec, $ec_project, $stream, $job_ids, $job_id_to_properties, $settings, $ec_update) = @_;
	log_print("Generating list view for $stream...");

	# sort all the job ids
	my @valid_job_ids = grep { $job_id_to_properties->{$_}->{'change_number'} =~ /^\d+$/; } @{$job_ids};
	my @sorted_job_ids = sort { ($job_id_to_properties->{$b}->{'change_number'} <=> $job_id_to_properties->{$a}->{'change_number'}) || ($b <=> $a) } @valid_job_ids;
	
	# parse them into a list and set of steps
	#my $labels = [];
	#my $label_to_node_names = {};
	#if($settings->{'Dashboard'})
	#{
	#	foreach(@{$settings->{'Dashboard'}})
	#	{
	#		my ($label, $node_names) = ($_->{'Label'}, $_->{'Nodes'});
	#		push(@{$labels}, $label);
	#		my @node_names = split(/\+/, $node_names);
	#		$label_to_node_names->{$label} = \@node_names;
	#	}
	#}

	# create the html boilerplate
	my $XHTML = "";	
	$XHTML .= "<link href='/commander/styles/JobsQuickView.css' rel='stylesheet' type='text/css' />\n";
	$XHTML .= "<link href='/commander/lib/styles/ColumnedSections.css' rel='stylesheet' type='text/css' />\n";
	$XHTML .= "<link href='/commander/lib/styles/Header.css' rel='stylesheet' type='text/css' />\n";
	$XHTML .= "<style type=\"text/css\">\n";
	$XHTML .= "  .builds { border-spacing: 1px; border-collapse: collapse; }\n";
	$XHTML .= "  .builds td { white-space:nowrap; padding:0.35em; padding-left:2em; padding-right:2em; border: 1px solid white; }\n";
	$XHTML .= "  .target_label { display:inline-block; padding-left:0.75em; padding-right:0.75em; }\n";
	$XHTML .= "  .target_label img { position: relative; padding-right:0.5em; vertical-align:middle; }\n";
	$XHTML .= "  .change_visible { visibility:visible; display:table-row; }\n";
	$XHTML .= "  .change_hidden { visibility:hidden; display:none; }\n";
	$XHTML .= "</style>\n";
	$XHTML .= "<table class='formBuilder data quickviewTable builds' style='position:absolute;width:100%;'>\n";
	$XHTML .= "  <tr bgcolor=\"#d7d7d7\">\n";
	$XHTML .= "    <td height=\"16\" align=\"center\" style=\"width:1%;\"></td>\n";
	$XHTML .= "    <td height=\"16\" align=\"center\" style=\"width:1%\"><b>Changelist</b></td>\n";
	$XHTML .= "    <td height=\"16\" align=\"center\" style=\"width:1%\"><b>Started By</b></td>\n";
	$XHTML .= "    <td height=\"16\" align=\"center\" style=\"width:1%\"><b>Build</b></td>\n";
	#$XHTML .= "    <td height=\"16\" align=\"center\" style=\"width:1%\"><b>Summary</b></td>\n";
	$XHTML .= "    <td height=\"16\" align=\"center\" style=\"min-width:300px;\"><b>Steps</b></td>\n";
	$XHTML .= "    <td height=\"16\" align=\"center\" style=\"width:1%\"><b>Warnings/Errors</b></td>\n";
	$XHTML .= "    <td height=\"16\" align=\"center\" style=\"width:1%\"><b>Elapsed</b></td>\n";
	$XHTML .= "    <td height=\"16\" align=\"center\" style=\"width:1%\"><b>Start Time</b></td>\n";
	$XHTML .= "  </tr>\n";
	
	# queries all the recent changes in a path
	my $row_idx = 0;
	foreach my $job_id (@sorted_job_ids)
	{
		my $job_properties = $job_id_to_properties->{$job_id};
		
		# open the row
		my $row_background_color = ((++$row_idx % 2) == 0)? "#f0f0f0" : "#f7f7f7";
		$XHTML .= "  <tr bgcolor='$row_background_color' style=\"height:3em\">\n";
		
		# status column
		$XHTML .= "    <td>".get_job_status_image($job_properties)."</td>\n";
		
		# change number column
		my $change_number = $job_properties->{'change_number'};
		my $script = "{ var x = document.getElementsByName('p4change'); for(var i = 0; i &lt; x.length; i++){ x[i].className = (x[i].className == 'change_hidden')? 'change_visible' : 'change_hidden'; } }";
		my $p4swarm_url = "http://p4-swarm.epicgames.net/files/".substr($stream, 1)."#commits";
		$XHTML .= "    <td align=\"center\"><a href=\"$p4swarm_url\">$change_number</a></td>\n";
		
		# started by column
		my $started_by = $job_properties->{'started_by'};
		$started_by =~ s/@.*//;
		$started_by =~ s/^project: .*$/Schedule/;
		$XHTML .= "    <td align=\"center\">$started_by</td>\n";
		
		# job name column
		my $short_job_name = $job_properties->{'job_name'};
		$short_job_name =~ s/^((?! - ).)* - //;
		$short_job_name =~ s/^((?! - ).)* - //;
		$XHTML .= "    <td align=\"center\"><a href=\"../../link/jobDetails/jobs/$job_properties->{'job_id'}\" target=\"_blank\">$short_job_name</a></td>\n";

		# summary column
		#$XHTML .= "    <td align=\"center\">\n";
		#my $job_step_properties = $job_properties->{'job_steps'};
		#if($job_step_properties)
		#{
		#	# make a lookup of node names to nodes
		#	my $node_name_to_node = { map { $_->{'name'} => $_ } @{$job_step_properties} };
		#	foreach my $label(@{$labels})
		#	{
		#		my $all_exist = 1;
		#		my $all_complete = 1;
		#		my $any_warnings = 0;
		#		my $any_errors = 0;
		#		foreach my $node_name(@{$label_to_node_names->{$label}})
		#		{
		#			my $node = $node_name_to_node->{$node_name};
		#			$all_exist = 0 && last if !$node;
		#			
		#			my $result = $node->{'result'};
		#			$all_complete = $all_complete && ($result && $result =~ /^completed:/);
		#			$any_warnings = $any_warnings || ($result && $result =~ /^(completed|running):warning/);
		#			$any_errors = $any_errors || ($result && $result =~ /^(completed|running):(error|skipped)/);
		#		}
		#		if($all_exist)
		#		{
		#			my $image = "/commander/lib/images/icn16px".($all_complete? "_" : "_running_").($any_errors? "error" : $any_warnings? "warning" : "success").".gif";
		#			$XHTML .= "      <div class='target_label'><img alt=\"$job_properties->{'outcome'}\" src=\"$image\"/><span style=\"position:relative; top:+1px;\">$label</span></div>\n";
		#		}
		#	}
		#}
		#$XHTML .= "    </td>\n";
		
		# steps column
		$XHTML .= "    <td align=\"center\" style=\"white-space:normal; vertical-align:middle;padding-top:1em;padding-bottom:1em;\">\n";
		foreach my $job_step(@{$job_properties->{'job_steps'}})
		{
			$XHTML .= "<span style=\"font-size:0;\"> </span>";
			$XHTML .= "<div style=\"white-space:nowrap;display:inline;\">".get_job_step_status_square($job_properties, $job_step)."</div>";
		}
		$XHTML .= "    </td>\n";

		# warnings/errors column
		$XHTML .= "    <td align=\"left\" style=\"white-space:nowrap;padding-top:1em;padding-bottom:1em;\">";
		if($job_properties->{'job_steps'})
		{
			my @failed_job_steps = ();
			foreach my $job_step (@{$job_properties->{'job_steps'}})
			{
				my $result = $job_step->{'result'};
				if($result =~ /^completed:/ && $result !~ /^completed:(success|skipped|error:(ABORTED|CANCELED))/)
				{
					push(@failed_job_steps, $job_step);
				}
			}
			if($#failed_job_steps >= 0)
			{
				$XHTML .= "<div>";
				foreach my $job_step (@failed_job_steps)
				{
					my $link = get_link_to_job_step($job_properties, $job_step);

					$XHTML .= "<div>";# style=\"display:inline;\">";
					$XHTML .= get_job_step_status_square($job_properties, $job_step);
					$XHTML .= "<span style=\"position:relative;top:1px;\"><a href=\"$link\" style=\"margin-left:0.5em;margin-right:0.5em;\">$job_step->{'name'}</a></span>";
					$XHTML .= "</div>";
				}
				$XHTML .= "</div>";
			}
		}
		if($job_properties->{'aborted_by'})
		{
			$XHTML .= "<div style=\"width:100%;text-align:center;margin-top:5px;margin-bottom:5px;\">Aborted by $job_properties->{'aborted_by'}</div>";
		}
		$XHTML .= "</td>\n";

		# elapsed time column
		$XHTML .= "    <td align=\"center\">".format_elapsed_time($job_properties->{'elapsed_time'} / 1000)."</td>\n";
		
		# start time column
		$XHTML .= "    <td align=\"center\">".format_recent_time($job_properties->{'start_time'})."</td>\n";
	
		# end of the row
		$XHTML .= "  <tr>\n";
	}
	$XHTML .= "</table>";
	
	ec_set_property($ec, "/projects[$ec_project]/Generated/".escape_stream_name($stream)."/Dashboard/ListView", $XHTML, $ec_update);
}

# returns an html fragment representing a single job step
sub get_job_step_status_square
{
	my ($job_properties, $job_step) = @_;
	
	my $result = $job_step->{'result'};

	my $color = '#c3c3c3';
	if($result =~ /^completed:success/)
	{
		$color = '#378d36';
	}
	elsif($result =~ /^completed:error:(CANCELED|ABORTED)/)
	{
		$color = '#c3c3c3';
	}
	elsif($result =~ /^completed:error/)
	{
		$color = '#cc3300';
	}
	elsif($result =~ /^completed:warning/)
	{
		$color = '#ffc800';
	}
	elsif($result =~ /^running/)
	{
		$color = '#98d7eb';
	}

	my $link = get_link_to_job_step($job_properties, $job_step);
	"<a href=\"$link\" style=\"margin:1px; background-image:none;\"><div style=\"display:inline-block; width:8px; height:8px; background-color:$color;\" title=\"$job_step->{'name'}: $result\"></div></a>";
}

# gets an image for the current job status, with a link to the job page
sub get_job_status_image
{
	my ($job_properties) = @_;

	my $XHTML = "<a href=\"../../link/jobDetails/jobs/$job_properties->{'job_id'}\" target=\"_blank\" style=\"background-image:none;\">";
	if($job_properties->{'aborted_by'})
	{
		$XHTML .= "<img alt=\"$job_properties->{'outcome'}\" src=\"/commander/lib/images/icn16px_stopped.gif\"/>";
	}
	elsif($job_properties->{'status'} eq 'runnable')
	{	
		$XHTML .= "<img alt=\"$job_properties->{'outcome'}\" src=\"/commander/lib/images/icn16px_runnable.gif\"/>";
	}
	elsif($job_properties->{'status'} eq 'running')
	{	
		$XHTML .= "<img alt=\"$job_properties->{'outcome'}\" src=\"/commander/lib/images/icn16px_running_$job_properties->{'outcome'}.gif\"/>";
	}
	elsif($job_properties->{'status'} eq 'waiting')
	{
		$XHTML .= "<img alt=\"$job_properties->{'outcome'}\" src=\"/commander/lib/images/icn16px_runnable.gif\"/>";
	}
	elsif($job_properties->{'status'} eq 'completed')
	{
		$XHTML .= "<img alt=\"$job_properties->{'outcome'}\" src=\"/commander/lib/images/icn16px_$job_properties->{'outcome'}.gif\"/>";
	}
	else
	{
		$XHTML .= "$job_properties->{'status'}";
	}
	$XHTML .= "</a>";
	
	$XHTML;
}

# builds the page showing mirrored github commits
sub update_github_view
{
	my ($ec, $ec_project, $stream, $settings, $ec_update) = @_;
	
	if($settings->{'GitHub'} && $settings->{'GitHub'}->{'Enabled'})
	{
		log_print("Generating GitHub view for $stream...");

		my $filters = $settings->{'GitHub'}->{'Filters'};
		my $remote_url = $settings->{'GitHub'}->{'RemoteUrl'};
		
		# read the status property
		my $state = decode_json(ec_try_get_property($ec, "/projects[$ec_project]/Generated/".escape_stream_name($stream)."/GitHub"));
		
		# find all the changes submitted to this branch
		my $change_history = get_change_history($stream, $filters, undef, 250);

		# create the html boilerplate
		my $html = "";	
		$html .= "<link href='/commander/styles/JobsQuickView.css' rel='stylesheet' type='text/css' />\n";
		$html .= "<link href='/commander/lib/styles/ColumnedSections.css' rel='stylesheet' type='text/css' />\n";
		$html .= "<link href='/commander/lib/styles/Header.css' rel='stylesheet' type='text/css' />\n";
		$html .= "<style type=\"text/css\">\n";
		$html .= "  .builds { border-spacing: 1px; border-collapse: collapse; }\n";
		$html .= "  .builds td { white-space:nowrap; padding:0.35em; padding-left:2em; padding-right:2em; border: 1px solid white; }\n";
		$html .= "  .target_label { display:inline-block; padding-left:0.75em; padding-right:0.75em; }\n";
		$html .= "  .target_label img { position: relative; padding-right:0.5em; vertical-align:middle; }\n";
		$html .= "  .change_visible { visibility:visible; display:table-row; }\n";
		$html .= "  .change_hidden { visibility:hidden; display:none; }\n";
		$html .= "</style>\n";
		$html .= "<table class='formBuilder data quickviewTable builds' style='position:absolute;width:100%;'>\n";
		$html .= "  <tr bgcolor=\"#d7d7d7\">\n";
		$html .= "    <td height=\"16\" align=\"center\" style=\"width:1%\"></td>\n";
		$html .= "    <td height=\"16\" align=\"center\" style=\"width:1%\"><b>Changelist</b></td>\n";
		$html .= "    <td height=\"16\" align=\"center\" style=\"width:10%\"><b>Author</b></td>\n";
		$html .= "    <td height=\"16\" align=\"center\" style=\"width:90%\"><b>Description</b></td>\n";
		$html .= "    <td height=\"16\" align=\"center\" style=\"width:1%\"><b>Commit</b></td>\n";
		$html .= "  </tr>\n";
		
		# queries all the recent changes in a path
		my $row_idx = 0;
		my $is_pending = 1;
		foreach my $change (@{$change_history})
		{
			my $change_number = $change->{'number'};
			my $change_state = $state->{'changes'}->{$change_number};
			$is_pending = 0 if $change_state;
			
			# open the row
			my $row_background_color = ((++$row_idx % 2) == 0)? "#f0f0f0" : "#f7f7f7";
			$html .= "  <tr bgcolor='$row_background_color' style=\"height:3em\">\n";

			# get the icon to display
			my $icon;
			if($is_pending)
			{
				$icon = "<img alt=\"Pending\" src=\"/commander/lib/images/icn16px_runnable.gif\"/>";
			}
			elsif(!$change_state)
			{
				$icon = "<img alt=\"Success\" src=\"/commander/lib/images/icn16px_skipped.gif\"/>";
			}
			elsif($change_state->{'success'})
			{
				$icon = "<img alt=\"Success\" src=\"/commander/lib/images/icn16px_".($change_state->{'commit'}? 'success' : 'skipped').".gif\"/>";
			}
			else
			{
				$icon = "<img alt=\"Failed\" src=\"/commander/lib/images/icn16px_error.gif\"/>";
			}
			
			# add the icon, with a link if appropriate
			if($change_state->{'job_id'} && $change_state->{'jobstep_id'})
			{
				$html .= "    <td><a href=\"/commander/link/jobDetails/jobs/$change_state->{'job_id'}\" style=\"background-image:none;\">$icon</a></td>";
			}		
			else
			{
				$html .= "    <td>$icon</td>";
			}
				
			# change number column
			my $p4swarm_url = "https://p4-swarm.epicgames.net/changes/$change_number";
			$html .= "    <td align=\"center\"><a href=\"$p4swarm_url\">$change_number</a></td>\n";

			# author column
			$html .= "    <td align=\"center\"><a href=\"mailto:$change->{'author_email'}\">$change->{'author'}</a></td>\n";

			# description column
			my ($trim_description) = ($change->{'description'} =~ /^([^\n]*)/);
			$html .= "    <td align=\"left\" style=\"white-space:normal\">$trim_description</td>\n";
			
			# commit column
			if($is_pending)
			{
				$html .= "    <td align=\"center\">Pending</td>\n";
			}
			elsif(!$change_state || !$change_state->{'commit'})
			{
				$html .= "    <td align=\"center\">Skipped</td>\n";
			}
			else
			{
				$html .= "    <td align=\"center\"><a href=\"$remote_url/commit/$change_state->{'commit'}\" target=\"_blank\">$change_state->{'commit'}</a></td>\n";
			}
			
			# end of the row
			$html .= "  <tr>\n";
		}
		$html .= "</table>";
		
		ec_set_property($ec, "/projects[$ec_project]/Generated/".escape_stream_name($stream)."/Dashboard/GitHubView", $html, $ec_update);
	}
}
