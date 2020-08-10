#!/usr/bin/perl

open(SIG, $ARGV[0]) || die "open $ARGV[0]: $!";

$n = sysread(SIG, $buf, 1000);

if($n > 510){
  print STDERR "boot block too large: $n bytes (max 510)\n";
  print STDERR "\x1b[31mFORCE CONTINUE!!!\x1b[0m\n";
  close SIG;
  open(SIG, $ARGV[0]) || die "open $ARGV[0]: $!";
  $n = sysread(SIG, $buf, 510);
  $buf .= "\x55\xAA";
  open(SIG, ">$ARGV[0]") || die "open >$ARGV[0]: $!";
  print SIG $buf;
  close SIG;
  exit 0;
  exit 1;
}

print STDERR "boot block is $n bytes (max 510)\n";

$buf .= "\0" x (510-$n);
$buf .= "\x55\xAA";

open(SIG, ">$ARGV[0]") || die "open >$ARGV[0]: $!";
print SIG $buf;
close SIG;
