#!/usr/bin/perl
# $Id: pdnsd_dhcp.pl,v 1.2 2001/03/25 20:01:34 tmm Exp $
##########################################################################
#
# Filename:     pdnsd_dhcp.pl
# Description:  Dynamic DNS-DHCP update script for pdnsd
# Author: 	Mike Stella
# Modified by:  Marko Stolle
# Created:      November 19, 2001
# Last Updated: February 28, 2001
# Email:        fwd2m@gmx.de
#
###########################################################################
#
#  This code is Copyright (c) 1998-2001 by Mike Stella and Marko Stolle
#
#  NO WARRANTY is given for this program.  If it doesn't
#  work on your system, sorry.  If it eats your hard drive, 
#  again, sorry.  It works fine on mine.  Good luck!
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
# 
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
#
###########################################################################
#
# This script reads a dhcpd.leases file and dynamically updates pdnsd with
# hostname and ip information.  
#
# It assumes that your DHCP server recieves hostnames from the
# clients, and that your clients offer their hostnames to the server.
# Some versions of Linux DHCP clients don't do that.  I use ISC's
# DHCPD, found at http://www.isc.org - though others may work just
# fine.
#
# This version of the script updates the pdnsd database. The status
# control socket of pdnsd has to be enabled (psnsd -d -s). 
#
###########################################################################
#
# 02/20/2001 - first working version
# 02/21/2001 - security patches by Thomas Moestl
# 02/22/2001 - re-read dhcpd.leases if ttl has expireds since last update
# 02/24/2001 - try to get domainname if not specified
# 02/28/2001 - randomized temporary filename
#	       added possibility to save some RAM (read below)
#
###########################################################################


# You may save some memory if you use absolute values with sysopen
# in sub update_dns and don't use tmpnam().. 
# Just switch the '#' in front of the 'until sysopen' in the sub 
# update_dns, check the necessary modes on your system using save_ram.pl 
# and add a '#' in front of the following three lines.
# Not using the tmpnam() function may open a security breach on systems
# with not absolute trustworthy local users (Risk: a user may write a 
# script which creates files with the same names as this script and block 
# it that way. Unlikely because the filenames are now even without tmpnam()
# randomized and an attacker has to create a very large number of files.)

use Fcntl;
use strict;
use POSIX qw(tmpnam);

$|=1;

###########################################################################
### Globals - you can change these as needed

# Domain name
# if not changed script will try to get it from the system
my $domain_name        = "domain";

# DHCPD lease file
my $lease_file         = "/var/lib/dhcp/dhcpd.leases";

# path to pdnsd-ctl
my $pdnsd_ctl          = "/usr/local/sbin/pdnsd-ctl";

# owning name server for the newly added records
my $nameserver         = "localhost.";

# TTL (Time To Live) for the new records
my $ttl                = "86400";

# number of seconds to check the lease file for updates
my $update_freq        = 30;

my $debug = 0;

###########################################################################
### Don't mess with anything below unless you REALLY need to modify the
### code.  And if you do, please let me know, I'm always interested in
### in improving this program.

# Make a pid file
`echo $$ > /var/run/pdnsd_update.pid`;

my $logstr;
my $modtime = 0;
my $temp_dir = -d '/tmp' ? '/tmp' : $ENV{TMP} || $ENV{TEMP};

use vars qw (%db);

my $version = "1.03";


###########################################################################
# Main Loop

  # try to find domainname if necessary
  if ($domain_name eq "domain") {
   $domain_name = `dnsdomainname`;
  }
  else {
   $domain_name = "$domain_name\n";
  }

while (1) {

  # check the file's last updated time, if it's been changed, update
  # the DNS and save the time. Update DNS even if there a no changes on
  # the leases file if ttl since last DNS update has expired.  
  # This will ALWAYS run once - on startup, since $modtime starts at zero.

 
  my @stats = stat ($lease_file);


  if (($stats[9] > $modtime) or (time >= $modtime+$ttl)){

	# clear the old hash
	undef %db;

	printf STDERR "updating DNS with dhcpd.leases\n";
	$modtime = time;
	&read_lease_file;
	&update_dns;
  } 

  # wait till next check time
  sleep $update_freq;

} # end main
###########################################################################


### write out the import file
sub update_dns {
	my ($ip, $hostname, $fname);

	do { $fname = tmpnam() }
        until sysopen(DNSFILE, $fname, O_WRONLY|O_CREAT|O_EXCL, 0600);
#	do { $fname = "$temp_dir/d2d".int(rand(time())) }
#	until sysopen(DNSFILE, $fname, 1|64|128, 0600);

	while (($hostname,$ip) = each (%db)) {
		print DNSFILE "$ip $hostname.$domain_name";
	}
	close DNSFILE;

	system ("$pdnsd_ctl source $fname $nameserver $ttl");
	unlink($fname);
}


### reads the lease file & makes a hash of what's in there.
sub read_lease_file {

  unless (open(LEASEFILE,$lease_file)) {
	#`logger -t dns_update.pl error opening dhcpd lease file`;
	print STDERR "Can't open lease file\n";
	return;
  }

  my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime(time);
  my $curdate = sprintf "%02d%02d%02d%02d%02d%20d%20d",
  		($year+1900),($mon+1),$mday,$hour,$min,$sec;

  ## Loop here, reading from LEASEFILE
  while (<LEASEFILE>) {
	my ($ip, $hostname, $mac, $enddate,$endtime);

	if (/^\s*lease/i) {
		
	  # find ip address
	  $_ =~ /^\s*lease\s+(\S+)/;
	  $ip = $1;
	  
	  # do the rest of the block - we're interested in hostname,
	  # mac address, and the lease time
	  while ($_ !~ /^}/) {
	    $_ = <LEASEFILE>;
		# find hostname
		if ($_ =~ /^\s*client/i) {
		  #chomp $_;
		  #chop $_;
		  $_ =~ /\"(.*)\"/;
		  $hostname = $1;
		  
		  # change spaces to dash, remove dots - microsoft
		  # really needs to not do this crap
		  $hostname =~ s/\s+/-/g;
		  $hostname =~ s/\.//g;
		}
		# get the lease end date
		elsif ($_ =~ /^\s*ends/i) {
			$_ =~ m/^\s*ends\s+\d\s+([^;]+);/;
			$enddate = $1;
			$enddate =~ s|[/: ]||g;
		}
	  }
	  # lowercase it - stupid dhcp clients
	  $hostname =~ tr/[A-Z]/[a-z]/;

	  ($debug < 1 ) || print STDERR "$hostname $ip $enddate $curdate\n";
	  
	  # Store hostname/ip in hash - this way we can do easy dupe checking
	  if (($hostname ne "") and ($enddate > $curdate)) {
		$db{$hostname} = $ip;
	  }
	}
  }
  close LEASEFILE;
}

### left around for testing
sub print_db {
  my ($key,$value);

  while (($key,$value) = each (%db)) {
	print "$key - $value\n";
  }
}

