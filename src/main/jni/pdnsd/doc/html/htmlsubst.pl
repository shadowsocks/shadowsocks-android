#!/usr/bin/perl -w

# Primitive ad-hoc script for updating pdnsd html doc files.
# Written by Paul Rombouts.

use strict;
use integer;
use POSIX qw(strftime);

my %paramvals=();

while(@ARGV && $ARGV[0]=~/^([^=]*)=(.*)$/) {
    my $param=$1; my $val=$2;
    if($param =~ /^[[:alpha:]]\w*$/) {
	$paramvals{$param}=$val;
    }
    else {warn "Warning: invalid parameter '$param' ignored.\n"}
    shift @ARGV;
}

sub sizeof {
    my($arg)=@_;
    (my $str= $arg) =~ s/\$(?:([[:alpha:]]\w*)\b|\{([[:alpha:]]\w*)\})/
				defined($paramvals{$+})?$paramvals{$+}:defined($ENV{$+})?$ENV{$+}:''/eg;
    my $filename=eval($str);
    (-f $filename) or return '???';
    (((-s $filename)+1023)/1024).'kB';
}

while(<>) {
    s/\$(?:date\b|\{date\})/strftime("%d %b %Y",localtime)/eg;
    s/\$sizeof\(([^()]*)\)/sizeof($1)/eg;
    s/\$(?:([[:alpha:]]\w*)\b|\{([[:alpha:]]\w*)\})/
	defined($paramvals{$+})?$paramvals{$+}:defined($ENV{$+})?$ENV{$+}:''/eg;
    print;
}
