#!/usr/bin/env perl
#====================================================================================
# find_src.pl
#
# Search for source files and required header file directories
#
# This file is part of makeESPArduino
# License: LGPL 2.1
# General and full license information is available at:
#    https://github.com/plerup/makeEspArduino
#
# Copyright (c) 2016-2020 Peter Lerup. All rights reserved.
#
#====================================================================================

use strict;
use File::Basename;

my %src_files;
my %inc_dirs;
my %user_libs;
my @search_dirs;
my %src_dirs;
my %checked_files;

sub uniq {
  my %seen;
  grep !$seen{$_}++, @_;
}

#--------------------------------------------------------------------

sub find_inc {
  # Recursively find include statements
  my $file_name = shift;
  open(my $f, $file_name) || return;
  $inc_dirs{dirname($file_name)}++;
  while (<$f>) {
    next unless /^\s*\#include\s*[<"]([^>"]+)/;
    my $match = $1;
    next if $checked_files{$match};
    $checked_files{$match}++;
    for (my $i = 0; $i < @search_dirs; $i++) {
      my $inc_file = "$search_dirs[$i]/$match";
      next unless -f $inc_file;
      find_inc($inc_file);
      my $dir = dirname($inc_file);
      if (!$src_dirs{$dir}) {
        # Add all source files in this directory
        # Can not only search for file with same name as sometimes
        # the actual implementation of the header has another name
        foreach my $src (glob("$dir/*.cpp $dir/*.c $dir/*.S")) {
          $src_files{$src}++;
          find_inc($src);
        }
      }
      last;
    }
  }
  close($f);
}

#--------------------------------------------------------------------

my $exclude_match = shift;

# Parameters are within quotes to delay possible wildcard file name expansions
my @libs = split(" ", "@ARGV");

if ($libs[0] =~ /(.+)\/examples\//) {
  # The sketch is an example, add the corresponding src directory to the library list if it exists
  my $src_dir = "$1/src";
  push(@libs, $src_dir) if -d $src_dir;
}

# First find possible explicit library source or achive files from the the specified list
for (my $i = 0; $i < @libs; $i++ ) {
  my $path = $libs[$i];
  if (!-d $path) {
    # File specification
    my $wildcard = $path =~ /\*/;
    if (!-e $path && !$wildcard) {
      print STDERR "* Warning: Ignoring non existing file specification $path\n";
      next;
    }
    $libs[$i] = dirname($path);
    # Mark as known source directory, except for sketch directory
    $src_dirs{$libs[$i]}++ if $i;
    if ($path =~ /\.(a|lib)$/) {
      # Library file
      $user_libs{$path}++;
    } elsif ($wildcard) {
      # Wildcard source files
      foreach my $src (glob($path)) {
        $src_files{$src}++;
      }
    } else {
      # Single source file
      $src_files{$path}++;
    }
  }
}
@libs = uniq(@libs);

# Expand all sub directories of the specified library directories
# Keep the original order, hence stored in array and not hash
# These directories will be included in the search for used header files
my $dir_spec = join(" ", @libs);
foreach (`find $dir_spec -type d 2>/dev/null`) {
  chomp;
  s/\/$//;
  next if /LittleFS\/lib/; # Fix for now
  push(@search_dirs, $_) unless $exclude_match && /$exclude_match/;
}
@search_dirs = uniq(@search_dirs);

# Search for used header files in all the specified source files
my @spec_src = keys %src_files;
foreach (@spec_src) {
  find_inc($_);
}

# Print the result as makefile variable definitions
print "USER_INC_DIRS = ";
# Keep order
foreach (@search_dirs) {
  print "$_ " if $inc_dirs{$_};
}
print "\n";
print "USER_SRC = ", join(" ", sort(keys %src_files)), "\n";
print "USER_LIBS = ", join(" ", keys %user_libs), "\n"
