#!/usr/bin/perl -w

use strict;

my %dic;
my $maxkeylen=0;

while(<>) {
    if(/"(\w+)".*?(\w+)/) {
	my $key=$1; my $val=$2;
	if($dic{$key}) {die "The key \"$key\" does not have a unique value.\n"}
	$dic{$key}=$val;
	if(length($key)>$maxkeylen) {$maxkeylen=length($key)}
    }
    else {die "Can't find key-value pair in following line:\n$_\n"}
}

my $linenr=0;
foreach my $key (sort(keys %dic)) {
    if($linenr++) {print ",\n"}
    printf("\t{%-*s%s}",$maxkeylen+4,"\"$key\",",$dic{$key});
}
print "\n";

exit
