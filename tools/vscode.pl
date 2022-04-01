#!/usr/bin/env perl
#====================================================================================
# vscode.pl
#
# Generates Visual Studio Code properties and task config files
# based on the compile command line and then starts VS Code
#
# This file is part of makeESPArduino
# License: LGPL 2.1
# General and full license information is available at:
#    https://github.com/plerup/makeEspArduino
#
# Copyright (c) 2020 Peter Lerup. All rights reserved.
#
#====================================================================================

use strict;
use Cwd;
use JSON::PP;
use Getopt::Std;
use File::Basename;

sub file_to_string {
  local($/);
  my $f;
  open($f, $_[0]) || return "";
  my $res = <$f>;
  close($f);
  return $res;
}

#--------------------------------------------------------------------

sub string_to_file {
  my $f;
  open($f, ">$_[0]") || return 0;
  print $f $_[1];
  close($f);
  return 1;
}

#--------------------------------------------------------------------

sub find_dir_upwards {
  my $match = $_[0];
  my $dir = Cwd::abs_path(".");
  while ($dir ne "/" && $dir ne $ENV{'HOME'}) {
    my $test = $dir . "/$match";
    return $test if -e $test;
    $dir = dirname($dir);
  }
  return Cwd::abs_path(".") . "/$match";
}

#--------------------------------------------------------------------

sub make_portable {
  # Use variables in paths when possible
  $_[0] =~ s/$_[1]/\$\{workspaceFolder\}/g;
  $_[0]  =~ s/$ENV{'HOME'}/\$\{env:HOME\}/g;
}

#--------------------------------------------------------------------

# Parameters
my %opts;
getopts('n:m:w:d:i:p:', \%opts);
my $name = $opts{n} || "Linux";
my $make_com = $opts{m} || "espmake";
my $workspace_dir = $opts{w};
my $proj_file = $opts{p};
my $cwd = $opts{d} || getcwd;
my $comp_path = shift;
$comp_path = shift if $comp_path eq "ccache";

my $config_dir_name = ".vscode";
$workspace_dir ||= dirname(find_dir_upwards($config_dir_name));
$proj_file ||= (glob("$workspace_dir/*.code-workspace"))[0];
my $config_dir = "$workspace_dir/$config_dir_name";
mkdir($config_dir);

# == C & C++ configuration
my @defines;
my @includes;
my $prop_file_name = "$config_dir/c_cpp_properties.json";
my $prop_json = file_to_string($prop_file_name) || '{"version": 4, "configurations": []}';

# Build this configuration from command line defines and includes
while ($_ = shift) {
  $_ .= shift if ($_ eq "-D");
  if (/-D\s*(\S+)/) {
    # May be a quoted value
    my $def = $1;
    $def =~ s/\"/\\\"/g;
    push(@defines, "\"$def\"")
  }
  push(@includes, "\"" . Cwd::abs_path($1) . "\"") if /-I\s*(\S+)/ && -e $1;
}
# Optional additional include directories
foreach (split(" ", $opts{i})) {
  push(@includes, "\"$_\"");
}

# Build corresponding json
my $def = join(',', @defines);
my $inc = join(',', @includes);
my $this_prop_json = <<"EOT";
{
  "name": "$name",
  "includePath": [$inc],
  "defines": [$def],
  "compilerPath": "$comp_path"
}
EOT
make_portable($this_prop_json, $workspace_dir);

# Insert or replace this configuration
my $json_ref = decode_json($prop_json);
my $configs = $$json_ref{'configurations'};
my $ind = 0;
foreach my $conf (@$configs) {
  last if $$conf{'name'} eq $name;
  $ind++;
}
$$configs[$ind] = decode_json($this_prop_json);
string_to_file($prop_file_name, JSON::PP->new->pretty->encode(\%$json_ref));

# == Add a task with the current name
my $this_task_json = <<"EOT";
{
  "label": "$name",
  "type": "shell",
  "command": "$make_com",
  "options": {"cwd": "$cwd"},
  "problemMatcher": ["\$gcc"],
  "group": "build"
}
EOT
make_portable($this_task_json, $workspace_dir);
my $this_task = decode_json($this_task_json);
my $task_file_name = "$config_dir/tasks.json";
my $task_json = file_to_string($task_file_name) || '{"version": "2.0.0", "tasks": []}';
$json_ref = decode_json($task_json);
my $tasks = $$json_ref{'tasks'};
my $found;
for (my $i = 0; !$found && $i < scalar(@$tasks); $i++) {
  if ($$tasks[$i]{'label'} eq $name) {
    # A task with this name exists, make sure that possible default build setting is kept
    $found = 1;
    $$this_task{'group'} = $$tasks[$i]{'group'};
    $$tasks[$i] = $this_task;
  }
}
push(@$tasks, $this_task) if !$found;
string_to_file($task_file_name, JSON::PP->new->pretty->encode(\%$json_ref));


# Launch Visual Studio Code
$proj_file ||= $workspace_dir;
print "Starting VS Code - $proj_file ...\n";
# Remove all MAKE variables to avoid conflict when building inside VS Code
foreach my $var (keys %ENV) {
  $ENV{$var} = undef if $var =~ /^MAKE/;
}
system("code $proj_file");
