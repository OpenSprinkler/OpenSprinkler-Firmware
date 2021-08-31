#!/usr/bin/env perl
#====================================================================================
# crash_tool.pl
#
# Analyzes crash dumps for esp8266 and esp32
# Completely based on the work in these two repos:
#  https://github.com/me-no-dev/EspExceptionDecoder
#  https://github.com/littleyoda/EspStackTraceDecoder
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
use File::Find;
use Term::ANSIColor qw(:constants);
local $Term::ANSIColor::AUTORESET = 1;

my $max_width = `tput cols`;

my ($esp_root, $elf_file_name) = @ARGV;

my $addr2line;
finddepth(sub { $addr2line = $File::Find::name if (/addr2line$/); }, $esp_root);
die("Failed to locate addr2line\n") unless $addr2line;

my @exceptions = (
"Illegal instruction",
"SYSCALL instruction",
"InstructionFetchError: Processor internal physical address or data error during instruction fetch",
"LoadStoreError: Processor internal physical address or data error during load or store",
"Level1Interrupt: Level-1 interrupt as indicated by set level-1 bits in the INTERRUPT register",
"Alloca: MOVSP instruction, if caller's registers are not in the register file",
"IntegerDivideByZero: QUOS, QUOU, REMS, or REMU divisor operand is zero",
"reserved",
"Privileged: Attempt to execute a privileged operation when CRING ? 0",
"LoadStoreAlignmentCause: Load or store to an unaligned address",
"reserved",
"reserved",
"InstrPIFDataError: PIF data error during instruction fetch",
"LoadStorePIFDataError: Synchronous PIF data error during LoadStore access",
"InstrPIFAddrError: PIF address error during instruction fetch",
"LoadStorePIFAddrError: Synchronous PIF address error during LoadStore access",
"InstTLBMiss: Error during Instruction TLB refill",
"InstTLBMultiHit: Multiple instruction TLB entries matched",
"InstFetchPrivilege: An instruction fetch referenced a virtual address at a ring level less than CRING",
"reserved",
"InstFetchProhibited: An instruction fetch referenced a page mapped with an attribute that does not permit instruction fetch",
"reserved",
"reserved",
"reserved",
"LoadStoreTLBMiss: Error during TLB refill for a load or store",
"LoadStoreTLBMultiHit: Multiple TLB entries matched for a load or store",
"LoadStorePrivilege: A load or store referenced a virtual address at a ring level less than CRING",
"reserved",
"LoadProhibited: A load referenced a page mapped with an attribute that does not permit loads",
"StoreProhibited: A store referenced a page mapped with an attribute that does not permit stores"
);

print BOLD GREEN "Paste your stack trace here!\n\n";
my @addr;
my $reason;
while (<STDIN>) {
  last if /<<<stack/;
  $reason = "$_$exceptions[$1]" if /Exception \((\d+)\):/;
  $reason = "$1" if /Guru Meditation Error: (.+)/;
  while (/(40[0-2][0-9a-f]{5})/g) {
    push(@addr, $1);
  }
  last if /<<<stack|Backtrace:/;
}
print "\n";
print BOLD RED "$reason\n\n";
print BOLD GREEN "=== Stack trace ===\n\n";
my $com = "$addr2line -aipfC -e $elf_file_name " . join(" ", @addr);
foreach (split(/\n/, `$com`)) {
  next unless /\S+:\s+(.+) at (\S+)/;
  print BOLD BLUE $1, "\n";
  my $path = $2;
  $path = ".." . substr($path, -($max_width-4)) if length($path) > $max_width-2;
  print "  $path\n";
}
print "\n";


