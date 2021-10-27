#!/usr/bin/env perl
#====================================================================================
# mem_use.pl
#
# Shows summary of flash and RAM memory
#
# This file is part of makeESPArduino
# License: LGPL 2.1
# General and full license information is available at:
#    https://github.com/plerup/makeEspArduino
#
# Copyright (c) 2016-2021 Peter Lerup. All rights reserved.
#
#====================================================================================

use strict;

my $flash_sections = shift;
my $ram_sections = shift;
my $flash_tot = 0;
my $ram_tot = 0;
while (<>) {
  $flash_tot += $1 if /$flash_sections/;
  $ram_tot += $1 if /$ram_sections/;
}
print "\nMemory summary\n";
print sprintf("  %-6s %6d bytes\n" x 2 ."\n", "RAM:", $ram_tot, "Flash:", $flash_tot);
