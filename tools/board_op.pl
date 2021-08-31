#!/usr/bin/env perl
#====================================================================================
# board_op.pl
#
# Performs search operations on the Arduino boards file
#
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


my $file_name = shift;
my $cpu = shift;
my $board_name = shift;
my $op = shift;

my $flash_def_match = $cpu eq "esp32" ? '\.build\.flash_size=(\S+)' : '\.menu\.(?:FlashSize|eesz)\.([^\.]+)=(.+)';
my $lwip_def_match = '\.menu\.(?:LwIPVariant|ip)\.(\w+)=(.+)';

my $boards_file;
local($/);
open($boards_file, $file_name) || die "Failed to open: $file_name\n";
my $board_conf = <$boards_file>;
close($boards_file);

my $result;
if ($op eq "first") {
  $result = $1 if $board_conf =~ /(\w+)\.name=/;
} elsif ($op eq "check") {
  $result = $board_conf =~ /$board_name\.name/;
} elsif ($op eq "first_flash") {
  $result = $1 if $board_conf =~ /$board_name$flash_def_match/;
} elsif ($op eq "first_lwip") {
  $result = $1 if $board_conf =~ /$board_name$lwip_def_match/;
} elsif ($op eq "list_names") {
  print "=== Available boards ===\n";
  foreach (split("\n", $board_conf)) {
    print sprintf("%-20s %s\n", $1, $2) if /^([\w\-]+)\.name=(.+)/;
  }
} elsif ($op eq "list_flash") {
  print "=== Memory configurations for board: $board_name ===\n";
  foreach (split("\n", $board_conf)) {
    print sprintf("%-10s %s\n", $1, $2) if /$board_name$flash_def_match/;
  }
} elsif ($op eq "list_lwip") {
  print "=== lwip configurations for board: $board_name ===\n";
  foreach (split("\n", $board_conf)) {
    print sprintf("%-10s %s\n", $1, $2) if /$board_name$lwip_def_match/;
  }
}

print $result;