#!/usr/bin/env perl
#====================================================================================
# obj_info.pl
#
# Show memory usage for object files
#
# This file is part of makeESPArduino
# License: LGPL 2.1
# General and full license information is available at:
#    https://github.com/plerup/makeEspArduino
#
# Copyright (c) 2021 Peter Lerup. All rights reserved.
#
#====================================================================================

use strict;

my $elf_size = shift;
my $form = shift == "1" ? "%s\t%s\t%s\t%s\t%s\t%s\n" : "%-38.38s %7s %7s %7s %7s %7s\n";
my $sort_index = shift;
print sprintf($form, "File", "Flash", "RAM", "data", "rodata", "bss");
print "-" x 78, "\n" unless $form =~ /\t/;

my %info;
while (my $obj_file = shift) {
  next unless $obj_file =~ /.+\/([\w\.]+)\.o$/;
  my $name = $1;
  for (my $i = 0; $i < 5; $i++) { $info{$name}[$i] = 0; }
  foreach (split("\n", `$elf_size -A $obj_file`)) {
    $info{$name}[0] += $1 if /(?:\.irom0\.text|\.text|\.text1|\.data|\.rodata)\S*\s+([0-9]+).*/;
    $info{$name}[2] += $1 if /^.data\S*\s+([0-9]+).*/;
    $info{$name}[3] += $1 if /^.rodata\S*\s+([0-9]+).*/;
    $info{$name}[4] += $1 if /^.bss\S*\s+([0-9]+).*/;
  }
  $info{$name}[1] = $info{$name}[2] + $info{$name}[3] + $info{$name}[4];
}
foreach (sort { $info{$b}[$sort_index] <=> $info{$a}[$sort_index] or $info{$b}[0] <=> $info{$a}[0] } keys %info) {
  print sprintf($form, $_, $info{$_}[0], $info{$_}[1], $info{$_}[2], $info{$_}[3], $info{$_}[4]);
}

