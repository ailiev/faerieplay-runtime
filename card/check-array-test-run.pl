#!/usr/bin/perl -w

use English;

while (<>) {
  chomp;
  next if not /^Command /;
#  print "$ARG\n";
  ($cmd, $idx, $in, $out) = /Command \d+: (\w+) \[(\d+)\] \((.*)\) --> \((.*)\)/;
  $out = "kuku" if not defined $out;
#  print "$cmd $idx $in --> $out\n";

  if ($cmd eq "write") {
    $arr[$idx] = $in;
  }
  elsif ($cmd eq "read") {
    if ($arr[$idx] ne $out) {
      print "Error on read [$idx], expected $arr[$idx], got $out\n";
    }
    else {
      print "Successful read [$idx]\n";
    }
  }
}
