#!/usr/bin/perl -w
#
# A Perl script to change the ip addresses of dns servers
# in the pdnsd configuration file.
#
# Written by Paul A. Rombouts
#
# This file Copyright 2002, 2004 Paul A. Rombouts
# It may be distributed under the GNU Public License, version 2, or
# any higher version.  See section COPYING of the GNU Public license
# for conditions under which this file may be redistributed.
#

use strict;

unless(@ARGV) {die "Error: no label specified.\n"}
my $label=shift;
unless(@ARGV) {die "Error: no DNS addresses specified.\n"}
my $dns_str=shift;
my $pdnsd_conf='/etc/pdnsd.conf';
if(@ARGV) {
    $pdnsd_conf=shift;
    if(@ARGV) {warn "Warning: spurious arguments ignored: @ARGV\n"}
}

#unless($label =~ /^\".*\"$/) {$label="\"$label\""}
#unless($dns_str =~ /^\".*\"$/) {$dns_str =~ s/^[\s,]*/\"/; $dns_str =~ s/[\s,]*$/\"/}
#unless($dns_str =~ /\"\s*\,\s*\"/) {$dns_str =~ s/[\s,]+/","/g}

my @lines=();
my $found_section=0;
my $changed=0;
my $ip_patt = qr/^((?:[^#]*?(?:\{|;))*?)(\s*ip\s*=\s*)("?[\w.:]+"?(?:\s*,\s*"?[\w.:]+"?)*)\s*;/;

open(CONFFILE,$pdnsd_conf) or die "Can't open $pdnsd_conf: $!\n";

while(<CONFFILE>) {
    if(/^\s*server\s*\{/) {
	my $sect_beg=$#lines+1;
	my $sect_end;
	my $found_label=0;
	LOOP: {
	    do {
		push @lines,$_;
		if(/^(?:.*(?:\{|;))?\s*label\s*=\s*"?\Q$label\E"?\s*;/) {
		    if($found_label++) {
			warn "Server section with multiple labels found.\n";
			close(CONFFILE);
			exit 2;
		    }
		}
		if(/\}\s*$/) {
		    $sect_end=$#lines;
		    last LOOP;
		}
	    } while(<CONFFILE>);
	}
	unless(defined($sect_end)) {
	    warn "Server section without proper ending found.\n";
	    close(CONFFILE);
	    exit 2;
	}
	if(!$found_label) {next}
	if(!($found_section++)) {
	    my $found_ip=0;
	    for(my $i=$sect_beg; $i<=$sect_end;++$i) {
		if($lines[$i] =~ $ip_patt) {
		    my $matched=''; my $rest;
		    do {
			$rest=$';
			if(!($found_ip++)) {
			    if($3 eq $dns_str) {
				$matched.=$&;
			    }
			    else {
				$matched.="$1$2$dns_str;";
				$changed=1;
			    }
			}
			else {
			    $matched.=$1;
			    $changed=1;
			}
		    } while($rest =~ $ip_patt);
		    $lines[$i] = $matched.$rest;
		}
	    }
	    if(!$found_ip) {
		unless($lines[$sect_end] =~ s/\}\s*$/ ip=$dns_str;\n$&/) {
		    warn "Can't add ip specification to server section labeled $label.\n";
		    close(CONFFILE);
		    exit 2;
		}
		$changed=1;
	    }
        }
        else {
	    splice @lines,$sect_beg;
	    $changed=1;
	}
    }
    else {push @lines,$_}
}

close(CONFFILE) or die "Can't close $pdnsd_conf: $!\n";

if(!$found_section) {
    warn "No server sections labeled $label found.\n";
    exit 2;
}
elsif(!$changed) {
    exit 0;
}

rename($pdnsd_conf,"$pdnsd_conf.save") or die "Can't rename $pdnsd_conf: $!\n";

unless((open(CONFFILE,">$pdnsd_conf") or (warn("Can't open $pdnsd_conf for writing: $!\n"),0)) and 
       (print CONFFILE (@lines) or (warn("Can't write to $pdnsd_conf: $!\n"),0)) and
       (close(CONFFILE) or (warn("Can't close $pdnsd_conf after writing: $!\n"),0))) {
    rename("$pdnsd_conf.save",$pdnsd_conf) or die "Can't rename $pdnsd_conf.save: $!\n";
    exit 3;
}

exit 1;
