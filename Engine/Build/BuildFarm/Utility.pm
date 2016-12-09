# Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

use strict;
use warnings;
use Data::Dumper;
use File::Copy;
use File::Path;
use File::Find;
use File::Spec;
use List::Util ('min', 'max');
use ElectricCommander();
use JSON;
use Time::Local;
use URI::Escape;

require Exporter;
our @EXPORT = (
		qw(p4_command p4_command_with_input p4_tagged_command p4_parse_spec p4_revert_all_changes p4_get_local_path),
		qw(ec_get_property ec_try_get_property ec_set_property ec_get_response_string ec_get_property_sheet ec_parse_property_sheet ec_flatten_property_sheet ec_create_jobsteps),
		qw(read_json read_file write_file safe_delete_directory safe_delete_file format_time join_paths is_windows quote_argument run_dotnet fail)
	);

# global options for p4 and ec
my $p4_verbose = 0;

# start time for timestamps in the log
my $log_start_time = Time::HiRes::time;

# platform settings
my $path_separator = is_windows()? '\\' : '/';

#### P4 Helper Functions #########################################################################################################################################################################

# get the p4 executable
sub p4_get_executable
{
	is_windows()? 'p4' : '/usr/local/bin/p4';
}

# run a p4 command with the given input lines
sub p4_command
{
	my $arguments = shift;
	my $additional_options = shift || {};
	my $command_line = p4_get_executable()." -s $arguments";

	print "Running $command_line\n" if $p4_verbose;

	open(P4, "$command_line |") or fail("Couldn't open $command_line");

	my $has_errors = 0;
	my @filtered_output = ();
	foreach(<P4>)
	{
		if(m/^info[0-9]*: (.*)/ || m/^text: (.*)/)
		{
			push @filtered_output, $1;
		}
		elsif(!m/^exit: /)
		{
			if(m/^error: (.*)/)
			{
				my $error_text = $1;
				if($additional_options->{'ignore_not_in_client_view'} && m/- file\(s\) not in client view\.$/)
				{
					print "$error_text\n";
					next;
				}
				if($additional_options->{'ignore_no_files_to_reconcile'} && m/- no file\(s\) to reconcile\.$/)
				{
					print "$error_text\n";
					next;
				}
				if($additional_options->{'errors_as_warnings'})
				{
					s/^error:/warning:/;
				}
				$has_errors = 1;
			}
			print "$_";
		}
	}
	
	if($additional_options->{'errors_as_warnings'})
	{
		close P4;
	}
	else
	{
		close P4 or fail("Failed to run $command_line");

		fail("Error executing P4.") if $has_errors;
		fail("P4 terminated with exit code $?") if $?;
	}
	
	@filtered_output;
}

# Run a p4 command with given input
sub p4_command_with_input
{
	my ($arguments, @input) = @_;

	my $command_line = p4_get_executable()." $arguments";
	print "Running $command_line\n" if $p4_verbose;

	open(P4, "| $command_line") or fail("Couldn't open $command_line");
	foreach(@input)
	{
		print P4 "$_\n";
	}
	close P4;
	
	fail("P4 terminated with exit code $?") if $?;
}

# Run a P4 command with -ztag, and parse the output into an array of hashes.
sub p4_tagged_command
{
	my ($command, $additional_options) = @_;

	my @output = p4_command("-ztag $command", $additional_options);
	
	my @objects;
	my %current_object = ();
	foreach(@output)
	{
		my $tag;
		my $value;
		if(m/^((?:\.\.\. )*[a-zA-Z0-9_-]+)$/)
		{
			($tag, $value) = ($1, '');
		}
		elsif(m/^((?:\.\.\. )*[a-zA-Z0-9_-]+) (.*)$/)
		{
			($tag, $value) = ($1, $2);
		}
		else
		{
			print "warning: Unexpected output line which is not in tagged format; skipped: $_\n";
			next;
		}
		if(exists $current_object{$tag})
		{
			push @objects, { %current_object };
			%current_object = ();
		}
		$current_object{$tag} = $value;
	}
	push @objects, { %current_object } if %current_object;

	@objects;
}

# Parse a P4 spec into a hash
sub p4_parse_spec
{
	my %spec = ();
	
	my ($current_key, $current_value);
	foreach(@_)
	{
		if(m/^([a-zA-Z_]+):[ \t]*(.*)$/)
		{
			$spec{$current_key} = $current_value if defined $current_key;
			($current_key, $current_value) = ($1, $2);
		}
		elsif(defined $current_key and m/^\t(.*)$/)
		{
			$current_value .= "\n" if length($current_value) > 0;
			$current_value .= "$1";
		}
		elsif(!m/^#/ && !m/^[\t ]*$/)
		{
			fail("Unexpected line in spec: $_");
		}
	}
	$spec{$current_key} = $current_value if defined $current_key;

	\%spec;
}

# revert all files in a workspace
sub p4_revert_all_changes
{
	my ($workspace_name) = @_;
	
	# check if there are any open files, and revert them
	if(p4_tagged_command("-c$workspace_name opened -m 1"))
	{
		p4_command("-c$workspace_name revert -k //...");
	}

	# enumerate all the pending changelists
	my @pending_changes = p4_tagged_command("-c$workspace_name changes -c$workspace_name -s pending");
	foreach(@pending_changes)
	{
		# delete any shelved files if there are any
		my $pending_change_number = ${%{$_}}{'change'};
		if(grep(/^\/\//, p4_command("-c$workspace_name describe -s -S $pending_change_number")))
		{
			print "Deleting shelved files in changelist $pending_change_number\n";
			p4_command("-c$workspace_name shelve -d -c $pending_change_number");
		}
	
		# delete the changelist
		print "Deleting changelist $pending_change_number\n";
		p4_command("-c$workspace_name change -d $pending_change_number");
	}
}

# gets the local path for a client or depot path
sub p4_get_local_path
{
	my ($workspace, $client_or_depot_path) = @_;
	my @records = p4_tagged_command("-c$workspace->{'name'} where \"$client_or_depot_path\"");
	@records = grep { !exists $_->{'unmap'} } @records;
	fail("p4 where for $client_or_depot_path returned multiple records") if $#records > 0;
	($#records == 0)? $records[0]->{'path'} : undef;
}

# parse a p4 date time string
sub p4_parse_time
{
	($_) = @_;
	/^(\d+)\/(\d+)\/(\d+) (\d+):(\d+):(\d+)$/ && timelocal($6, $5, $4, $3, $2 - 1, $1);
}

#### EC Helper Functions #########################################################################################################################################################################

# prepends the domain name to an EC url
sub ec_get_full_url
{
	my ($relative_url) = @_;
	"https://$ENV{'COMMANDER_SERVER'}$relative_url";
}

# gets an EC property, failing if it's not set
sub ec_get_property
{
	my ($ec, $name) = @_;
	return $ec->getProperty($name)->findvalue('//value')->string_value();
}

# gets an EC property, or returns an undefined string if it doesn't exist
sub ec_try_get_property
{
	my ($ec, $name) = @_;
	my $value = $ec->getProperty("/javascript getProperty('$name')")->findvalue('//value')->string_value();
	($value eq '')? undef : $value;
}

# sets an EC property, or just prints it out if we're running without --ec-update
sub ec_set_property
{
	my ($ec, $name, $value, $ec_update) = @_;
	if($ec_update)
	{
		$ec->setProperty($name, $value);
	}
	else
	{
		print "Skipped setting EC property without --ec-update: $name -> $value\n";
	}
}

# deletes an EC property, or just prints it out if we're running without --ec-update
sub ec_delete_property
{
	my ($ec, $name, $ec_update) = @_;
	if($ec_update)
	{
		$ec->deleteProperty($name);
	}
	else
	{
		print "Skipped deleting EC property without --ec-update: $name\n";
	}
}

# converts an EC response xpath object to a string
sub ec_get_response_string
{
	my $response = shift;
	$response->findnodes_as_string ("/");
}

# parses a property from a property sheet
sub ec_parse_property
{
	my ($property_sheet_xpath, $property_sheet_node, $property_name) = @_;
	
	my $result;
	foreach my $property_node ($property_sheet_xpath->findnodes("property", $property_sheet_node))
	{
		my $name = $property_sheet_xpath->findvalue("./propertyName", $property_node)->string_value();
		if($name eq $property_name)
		{
			$result = $property_sheet_xpath->findvalue("./value", $property_node)->string_value();
			last;
		}
	}
	$result;
}

# reads a property sheet from EC and convert it into a perl hash. arguments are passed directly to the EC getProperties() method.
sub ec_get_property_sheet
{
	my ($ec, @arguments) = @_;

	my $response = $ec->getProperties(@arguments);

	my $result;
	foreach my $property_sheet ($response->findnodes("/responses/response/propertySheet"))
	{
		$result = ec_parse_property_sheet($response, $property_sheet);
		last;
	}
	$result;
}

# parses a property sheet response as a perl hash
sub ec_parse_property_sheet
{
	my ($response, $property_sheet) = @_;
	
	my $result = {};
	foreach my $property ($response->findnodes("property", $property_sheet))
	{
		my $name = $response->findvalue("propertyName", $property)->string_value();
		$result->{$name} = $response->findvalue("value", $property)->string_value();
		
		foreach my $next_property_sheet ($response->findnodes("propertySheet", $property))
		{
			$result->{$name} = ec_parse_property_sheet($response, $next_property_sheet);
		}
	}
	$result;
}

# flattens a property sheet to a single level, with each key set to a slash-separated path
sub ec_flatten_property_sheet
{
	my ($property_sheet, $path, $flat_property_sheet) = @_;

	$path = "" if !defined $path;
	$flat_property_sheet = {} if !defined $flat_property_sheet;
	
	foreach my $key (keys %{$property_sheet})
	{
		my $value = $property_sheet->{$key};
		if(ref $value eq ref {})
		{
			ec_flatten_property_sheet($value, "$path/$key", $flat_property_sheet);
		}
		else
		{
			$flat_property_sheet->{"$path/$key"} = $value;
		}
	}
		
	$flat_property_sheet;
}

# gets information for a job
sub ec_get_job
{
	my ($ec, $job_id) = @_;
	
	my $job_xpath = $ec->getJobDetails($job_id);
	my $job_node = $job_xpath->findnodes("/responses/response/job")->get_node(1);
	
	ec_parse_job($job_xpath, $job_node);
}

# parse details for a job
sub ec_parse_job
{
	my ($job_xpath, $job_node) = @_;

	my $job = {};
	
	# get the job properties
	$job->{'job_id'} = $job_xpath->findvalue("./jobId", $job_node)->string_value();
	$job->{'job_name'} = $job_xpath->findvalue("./jobName", $job_node)->string_value();
	$job->{'job_url'} = "/commander/link/jobDetails/jobs/$job->{'job_id'}";

	# other properties
	$job->{'workspace_dir'} = $job_xpath->findnodes("./workspace/winUNC", $job_node)->get_node(1)->string_value();
	$job->{'properties'} = ec_parse_property_sheet($job_xpath, $job_xpath->findnodes("./propertySheet", $job_node)->get_node(1));
	
	# save the original xpath and source node
	$job->{'source_xpath'} = $job_xpath;
	$job->{'source_node'} = $job_node;

	$job;
}

# gets information for a jobstep
sub ec_get_jobstep
{
	my ($ec, $jobstep_id) = @_;

	my $jobstep_xpath = $ec->getJobStepDetails($jobstep_id);
	my $jobstep_node = $jobstep_xpath->findnodes("/responses/response/jobStep")->get_node(1);
	
	ec_parse_jobstep($jobstep_xpath, $jobstep_node);
}

# parse details for a jobstep
sub ec_parse_jobstep
{
	my ($jobstep_xpath, $jobstep_node) = @_;

	my $jobstep = {};
	
	# get the job properties
	$jobstep->{'job_id'} = $jobstep_xpath->findvalue("./jobId", $jobstep_node)->string_value();
	$jobstep->{'job_name'} = $jobstep_xpath->findvalue("./jobName", $jobstep_node)->string_value();
	$jobstep->{'job_url'} = "/commander/link/jobDetails/jobs/$jobstep->{'job_id'}";

	# get the jobstep properties
	$jobstep->{'jobstep_id'} = $jobstep_xpath->findvalue("./jobStepId", $jobstep_node)->string_value();
	$jobstep->{'jobstep_name'} = $jobstep_xpath->findvalue("./stepName", $jobstep_node)->string_value();
	$jobstep->{'jobstep_url'} = "/commander/link/jobStepDetails/jobSteps/$jobstep->{'jobstep_id'}?stepName=".encode_form_parameter($jobstep->{'jobstep_name'})."&jobId=$jobstep->{'job_id'}&jobName=".encode_form_parameter($jobstep->{'job_name'})."&tabGroup=diagnosticHeader";

	# find a link to the log
	my $workspace_name = $jobstep_xpath->findvalue("./workspace/workspaceName", $jobstep_node)->string_value();
	my $log_file_name = $jobstep_xpath->findvalue("./logFileName", $jobstep_node)->string_value();
	$jobstep->{'jobstep_log_url'} = "/commander/link/workspaceFile/workspaces/".encode_form_parameter($workspace_name)."?jobStepId=$jobstep->{'jobstep_id'}&fileName=".encode_form_parameter($log_file_name);

	# get the status
	$jobstep->{'status'} = $jobstep_xpath->findvalue('./status', $jobstep_node)->string_value();
	$jobstep->{'outcome'} = $jobstep_xpath->findvalue("./outcome", $jobstep_node)->string_value();
	$jobstep->{'error_code'} = $jobstep_xpath->findvalue("./errorCode", $jobstep_node)->string_value();
	
	# get the combined result
	my $result = $jobstep->{'status'};
	if($result eq 'completed')
	{
		$result = $jobstep->{'outcome'} || $result;
		if($result eq "error")
		{
			$result = $jobstep->{'error_code'} || $result;
		}
	}
	$jobstep->{'result'} = $result;
	
	# other properties
	$jobstep->{'resource_name'} = $jobstep_xpath->findvalue("./resourceName", $jobstep_node)->string_value();
	if(is_windows())
	{
		$jobstep->{'workspace_dir'} = $jobstep_xpath->findvalue("./workspace/winUNC", $jobstep_node)->string_value();
	}
	else
	{
		$jobstep->{'workspace_dir'} = $jobstep_xpath->findvalue("./workspace/unix", $jobstep_node)->string_value();
	}
	$jobstep->{'properties'} = ec_parse_property_sheet($jobstep_xpath, $jobstep_xpath->findnodes("./propertySheet", $jobstep_node)->get_node(1));
	
	$jobstep;
}

# gets the overall result for a list of jobsteps
sub ec_get_combined_result
{
	my ($jobsteps) = @_;
	
	# check whether everything succeeded
	my $result = 1;
	foreach(@{$jobsteps})
	{
		my $jobstep_result = $_->{'result'};
		$result = 0 if $jobstep_result eq 'error';
	}
	$result;
}

# creates a list of jobsteps from an array of job steps
sub ec_create_jobsteps
{
	my ($ec, $jobstep_arguments_array, $fake_ec_filename, $ec_update, $additional_options) = @_;
	if($#{$jobstep_arguments_array} >= 0)
	{
		if($ec_update)
		{
			my $batch = $ec->newBatch("serial");
			foreach my $jobstep_arguments (@{$jobstep_arguments_array})
			{
				$batch->createJobStep($jobstep_arguments);
			}
			$batch->submit();
		}
		else
		{
			print "Writing to $fake_ec_filename instead of submitting to EC due to --fake-ec.\n";
			open(my $file_handle, '>', $fake_ec_filename) or fail("Unable to open $fake_ec_filename: $!");
			foreach my $jobstep_arguments (@{$jobstep_arguments_array})
			{
				local $Data::Dumper::Terse = 1;
				print $file_handle Dumper(\$jobstep_arguments);
			}
			close($file_handle);
		}
	}
}

#### GUBP Helper Functions #######################################################################################################################################################################

# escapes a stream name
sub escape_stream_name
{
	my ($stream) = @_;
	
	my $escaped_stream = $stream;
	$escaped_stream =~ s/\//+/g;
	
	$escaped_stream;
}

# finds the definition for a particular node from a job definition
sub find_node_definition
{
	my ($job_definition, $node_name) = @_;
	foreach my $group (@{$job_definition->{'Groups'}})
	{
		foreach my $node (@{$group->{'Nodes'}})
		{
			return $node if $node->{'Name'} eq $node_name;
		}
	}
	undef;
}

# finds the definition for a particular report from a job definition
sub find_report_definition
{
	my ($job_definition, $report_name) = @_;
	foreach my $trigger (@{$job_definition->{'Triggers'}})
	{
		return $trigger if $trigger->{'Name'} eq $report_name;
	}
	foreach my $report (@{$job_definition->{'Reports'}})
	{
		return $report if $report->{'Name'} eq $report_name;
	}
	undef;
}

# finds the definition for a particular report from a job definition
sub find_badge_definition
{
	my ($job_definition, $badge_name) = @_;
	foreach my $badge_definition (@{$job_definition->{'Badges'}})
	{
		return $badge_definition if $badge_definition->{'Name'} eq $badge_name;
	}
	undef;
}

# gets the jobsteps that a trigger or report is dependent on
sub get_dependency_jobsteps
{
	my ($job, $dependency_names) = @_;

	# parse out all the jobsteps
	my $dependency_name_to_jobstep = { map { $_ => undef } @{$dependency_names} };
	foreach my $jobstep_node ($job->{'source_xpath'}->findnodes("//jobStep"))
	{
		my $jobstep_name = $job->{'source_xpath'}->findvalue("./stepName", $jobstep_node)->string_value();
		if(exists $dependency_name_to_jobstep->{$jobstep_name})
		{
			$dependency_name_to_jobstep->{$jobstep_name} = ec_parse_jobstep($job->{'source_xpath'}, $jobstep_node);
		}
	}
	
	# get all the dependency jobsteps. if a jobstep was never created because it's preconditions weren't met, just create a placeholder.
	my $jobsteps = [];
	foreach my $dependency_name (@{$dependency_names})
	{
		my $jobstep = $dependency_name_to_jobstep->{$dependency_name};
		if(!$jobstep)
		{
			$jobstep = { jobstep_name => $dependency_name, result => 'skipped' };
		}
		push(@{$jobsteps}, $jobstep);
	}
	$jobsteps;
}

#### Utility Functions ###########################################################################################################################################################################

# print to the log with a timestamp
sub log_print
{
	my ($message) = @_;

	my $elapsed_milliseconds = (Time::HiRes::time - $log_start_time) * 1000;
	my $elapsed_seconds = $elapsed_milliseconds / 1000;
	my $elapsed_minutes = $elapsed_seconds / 60;
	my $elapsed_hours = $elapsed_minutes / 60;
	
	print sprintf "[%02d:%02d:%02d.%03d] $message\n", $elapsed_hours, $elapsed_minutes % 60, $elapsed_seconds % 60, $elapsed_milliseconds % 1000;
}

# read the job definition from the workspace
sub read_json
{
	my $file_name = shift;
	my $contents = read_file($file_name);
	decode_json($contents);
}

# writes a json dictionary to a file
sub write_json
{
	my ($file, $json) = @_;
	write_file($file, encode_json($json));
}

# reads a text file into a string
sub read_file
{
	my ($file_name) = @_;
	
	local $/;
	open(my $file_handle, '<', $file_name) or fail("Unable to open $file_name: $!");
	my @file_text=<$file_handle>;
	close($file_handle);
	
	join("\n", @file_text);
}

# writes a string into a text file
sub write_file
{
	my ($file_name, $contents) = @_;

	local $/;
	open(my $file_handle, '>', $file_name) or fail("Unable to open $file_name: $!");
	print $file_handle $contents;
	close($file_handle);
}

# recursively copies a directory from one place to another
sub mkdir_recursive
{
	my ($dir) = @_;
	$dir =~ s/[\\\/]*$//;
	
	if(!-d $dir)
	{
		# make sure the parent directory exists
		my $parent_dir = $dir;
		$parent_dir =~ s/[\\\/][^\\\/]+$//;
		mkdir_recursive($parent_dir);
		
		# create the subdirectory
		my $result = mkdir $dir;
		if(!$result)
		{
			fail("Couldn't create $result $dir: $!");
		}
		if(!-d $dir)
		{
			fail("Mkdir succeeded but $dir does not exist");
		}
	}
}

# recursively copies a directory from one place to another
sub copy_recursive
{
	my ($source_dir, $target_dir) = @_;
	
	if(-d $source_dir)
	{
		my @files;
		find(sub { push(@files, $File::Find::name) if -f $File::Find::name; }, $source_dir);
		
		foreach my $source_file(@files)
		{
			my $relative_path = File::Spec->abs2rel($source_file, $source_dir);
			my $target_file = File::Spec->rel2abs($relative_path, $target_dir);
			my ($target_volume, $target_path) = File::Spec->splitpath($target_file);
			my $target_dir = "$target_volume$target_path";
			mkdir_recursive($target_dir);
			copy($source_file, $target_file);
		}
	}
}

# delete a directory, including all read-only files
sub safe_delete_directory
{
	my $directory_name = shift;
	if(-d $directory_name)
	{
		find(\&file_find_make_writeable, $directory_name);
		rmtree($directory_name);
		fail("Couldn't delete directory $directory_name") if -d $directory_name;
	}
}

# callback for the File::Find function, which makes each file writeable
sub file_find_make_writeable
{
	chmod 0777, $File::Find::name;
}

# delete a file, even if it's read only
sub safe_delete_file
{
	my $filename = shift;
	chmod 0777, $filename;
	unlink($filename);
}

# splits a string into a list of entities, removing whitespace and empty entries
sub split_list
{
	my ($items, $separator) = @_;
	
	my $items_array = [];
	foreach(split /$separator/, $items)
	{
		s/^\s*//;
		s/\s*$//;
		push(@{$items_array}, $_) if $_;
	}
	$items_array;
}

# gets a time value representing midnight on the provided day. forces time zone to EST.
sub get_local_midnight
{
	my ($time) = @_;
    my ($sec,$min,$hour,$day,$mon,$year) = localtime($time);
	timelocal(0, 0, 0, $day, $mon, $year);
}

# formats a time in seconds as 0m32s
sub format_time
{
	my $seconds = shift;
	($seconds >= 60)? (sprintf "%dm%02ds", $seconds / 60, $seconds % 60) : $seconds."s";
}

# parse a time as a unix time
sub parse_sql_time
{
	($_) = @_;
	/^(\d+)-(\d+)-(\d+)T(\d+):(\d+):(\d+\.\d+)Z$/ && timegm($6, $5, $4, $3, $2 - 1, $1);
}

# format a date/time string
sub format_date_time
{
	my ($datetime) = @_;
	
	my ($time_seconds, $time_minutes, $time_hours, $date_day, $date_month, $date_year, $date_dow) = localtime($datetime);

	sprintf "%d/%d/%d %d:%02d%s", $date_year + 1900, $date_month + 1, $date_day, (($time_hours + 11) % 12) + 1, $time_minutes, ($time_hours < 12)? "am" : "pm";
}

# format a time string in a simple manner relative to the current date (eg. omitting the date if today, referring to yesterday, omitting the year if this year).
sub format_recent_time
{
	my ($datetime) = @_;
	my ($now_seconds, $now_minutes, $now_hours, $now_day, $now_month, $now_year) = localtime;
	my $midnight_today = timelocal(0, 0, 0, $now_day, $now_month, $now_year);
	my $midnight_yesterday = $midnight_today - (24 * 60 * 60);
	my $this_week = $midnight_today - (6 * 24 * 60 * 60);

	my ($time_seconds, $time_minutes, $time_hours, $date_day, $date_month, $date_year, $date_dow) = localtime($datetime);
	my $time_string = sprintf "%d:%02d %s", (($time_hours + 11) % 12) + 1, $time_minutes, ($time_hours < 12)? "am" : "pm";
	
	if($datetime > $midnight_today)
	{
		$time_string;
	}
	elsif($datetime > $midnight_yesterday)
	{
		"Yesterday at $time_string";
	}
	elsif($datetime > $this_week)
	{
		('Sunday', 'Monday', 'Tuesday', 'Wednesday', 'Thursday', 'Friday', 'Saturday')[$date_dow]." at $time_string";
	}
	else
	{
		($date_month + 1)."/$date_day/".($date_year + 1900)." at $time_string";
	}
}

# format a time in seconds in the style '1h 2m'.
sub format_elapsed_time
{
	my ($time_s) = @_;

	$time_s /= 60;
	my $minutes = $time_s % 60;
	$time_s /= 60;
	my $hours = $time_s;
	
	my $result;
	if($hours >= 1)
	{
		$result = (sprintf "%dh %02dm", $hours, $minutes);
	}
	elsif($minutes >= 1)
	{
		$result = (sprintf "%dm", $minutes);
	}
	else
	{
		$result = "< 1m";
	}
	$result;
}

# parse a time interval, in the format 123h 45m, 2h, or 23m.
sub parse_time_interval
{
	my $interval = 0;
	my $interval_string = $1;
	if($interval_string =~ /^(\d+)\s*[Hh]\s*(.*)$/)
	{
		$interval += $1 * 60 * 60;
		$interval_string = $2;
	}
	if($interval_string =~ /^(\d+)[Mm]\s*(.*)$/)
	{
		$interval += $1 * 60;
		$interval_string = $2;
	}
	($interval_string eq '')? $interval : undef;
}

# encode a string for a form post
sub encode_form_parameter
{
	my ($parameter) = @_;
    $parameter =~ s/([^-A-Za-z0-9_.])/sprintf("%%%02x", ord($1))/seg;
	$parameter;
}

# checks whether we're running on windows
sub is_windows
{
	($^O eq "MSWin32");
}

# joins strings together in a path
sub join_paths
{
	join($path_separator, @_);
}

# quotes a command-line argument if necessary
sub quote_argument
{
	my $argument = shift;
	if($argument =~ / /)
	{
		if($argument =~ /^(-+[A-Za-z_-]+=)(.*)/)
		{
			$argument = "$1\"$2\"";
		}
		else
		{
			$argument = "\"$argument\"";
		}
	}
	$argument;
}

# extracts the name portion of a standard build job, formatted as "Branch - CL - Name (#x)".
sub get_base_job_name
{
	my ($name) = (@_);

	$_ = $name;
	if(s/^.+? - //)
	{
		if(s/^.+? - //)
		{
			s/\s+\(#\d+\)$//;
			$name = $_;
		}
	}

	$name;
}

# runs a .NET application
sub run_dotnet
{
	# run directly on windows, otherwise spawn mono
	if(is_windows())
	{
		system("@_");
	}
	else
	{
		system("/usr/local/bin/mono @_");
	}
	
	# return the exit code
	$?;
}

# Like 'die', but formats in an EC-friendly way and writes to stdout.
sub fail
{
	my $message = shift;
	print "error: $message\n";
	exit 1;
}

1;