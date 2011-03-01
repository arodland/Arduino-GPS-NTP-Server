#!/usr/bin/perl
use strict;
use warnings;

while (<>) {
  print $1 / 1000, "\t" if /PPS: (.*)/;
  print $1 / 1000, "\t" if /PPS filtered: (.*)/;
  print $1 / 16, "\t" if /PLL: (.*)/;
  print $1 / 16, "\n" if /tickadj = (.*)/;
}
