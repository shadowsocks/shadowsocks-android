#!/usr/bin/perl -w
#
# A Perl script for converting pdnsd html documentation to a man page.
#
# Written by Paul A. Rombouts
#
# This file Copyright 2004 Paul A. Rombouts
# It may be distributed under the GNU Public License, version 2, or
# any higher version.  See section COPYING of the GNU Public license
# for conditions under which this file may be redistributed.
#

use strict;
use POSIX qw(strftime);


while(<>) {
    if(/<h[1-4]>[^<]*configuration file/i) {
       last;
    }
}

exit unless defined($_);

while(<>) {
    if(/<h[1-4]>[^<]*layout/i) {
       last;
    }
}

exit unless defined($_);

(my $myname=$0) =~ s{.*/}{};

print <<ENDOFHEADER;
.\\\" Generated automatically from the html documentation by $myname
.\\\" 
.\\\" Manpage for pdnsd.conf (pdnsd configuration file)
.\\\" 
.\\\" Copyright (C) 2000, 2001 Thomas Moestl
.\\\" Copyright (C) 2003, 2004, 2005, 2006, 2007 Paul A. Rombouts
.\\\" 
.\\\" This manual is a part of the pdnsd package, and may be distributed in
.\\\" original or modified  form  under  terms  of  the  GNU  General  Public
.\\\" License,  as  published by the Free Software Foundation; either version
.\\\" 3, or (at your option) any later version.
.\\\" You can find a copy of the GNU GPL in the file COPYING in the source
.\\\" or documentation directory.
.\\\" 
ENDOFHEADER


print ".TH PDNSD.CONF 5 \"",strftime("%b %Y",localtime),"\" \"pdnsd \@fullversion\@\"\n";
print <<ENDOFHEADER2;
.SH NAME
pdnsd.conf \\- The configuration file for pdnsd
.hw config
.SH DESCRIPTION
.PP
This manual page describes the layout of the
.BR pdnsd (8)
configuration file and the available configuration options.
The default location of the file is \@sysconfdir\@/pdnsd.conf. This may be changed
with the \\fB-c\\fP command line option.
An example pdnsd.conf comes with the pdnsd distribution in the documentation directory
or in \@sysconfdir\@/pdnsd.conf.sample.
.SH \"FILE FORMAT\"
.PP
ENDOFHEADER2

my $taggedparagraph=0;
my $displayed=0;

while(<>) {
    if(/<h[1-4]>.*\bpdnsd-ctl\b/) {
       last;
    }
    s{^\s*((?:<[^<>]+>)*?)<h[1-4]>[\d.]*\s*(.*)</h[1-4]>((?:<[^<>]+>)*?)(?:<br>)?\s*$}{.SS $1$2$3\n}i;
    if(s{^\s*<tr>\s*}{.TP\n}i) {$taggedparagraph=1}
    if(m{^\s*</tr>}i) {$taggedparagraph=0}
    s{^\s*((?:<[^<>]+>)*?)<b>(.*)</b>((?:<[^<>]+>)*?)(?:<br>)?\s*$}{.B $1$2$3\n}i  if $taggedparagraph;
    s{^\s*((?:<[^<>]+>)*?or(?:<[^<>]+>)*?)(?:<br>)?\s*$}{$1\n.PD 0\n.TP\n.PD\n}i  if $taggedparagraph;
    if(s{^\s*<pre>}{.DS L\n}i) {$displayed=1}
    s{^\t}{        } if $displayed;
    if(s{</pre>\s*$}{\n.DE\n\n}i) {$displayed=0}
    elsif(!$displayed) {s{^\s*}{}}
    s{^\s*<li>}{.IP\n\\(bu }i;
    s{<li>}{\n.IP\n\\(bu }i;
    s{<ul>}{\n}i;
    s{</ul>}{\n}i;
    s{<b>}{\\fB}ig;
    s{</b>}{\\fP}ig;
    s{<(i|em)>}{\\fI}ig;
    s{</(i|em)>}{\\fP}ig;
    unless(s{^\s*(<[^<>]+>)*(<br>|<p>)(<[^<>]+>)*\s*$}{\n}i) {
	s{<p\b[^<>]*>(.*)</p>}{\n$1\n}i;
	s{^\s*<br>}{.br\n}i;
	s{<br>\s*<br>\s*$}{\n\n}i;
	s{<br>\s*$}{\n.br\n}i;
	s{<br>}{\n.br\n}i;
	s{^\s*(<[^<>]+>)*\s*$}{};
    }
    s{<[^<>]+>}{}g;
    s{&lt;}{<}ig;
    s{&gt;}{>}ig;
    s{&quot;}{"}ig;
    s{&nbsp;}{\\ }ig;
    s{/var/cache/pdnsd\b}{\@cachedir\@}g;
    s{(?<![-\w\\])-[-\w]*}{(my $s=$&) =~ s{-}{\\-}g;$s}ge;
    s{\bpdnsd-ctl\b}{pdnsd\\-ctl}g;
    s{\blist-rrtypes\b}{list\\-rrtypes}g;
    print;
}

print <<ENDOFTRAILER;
.SH \"VERSION\"
.PP
This man page is correct for version \@fullversion\@ of pdnsd.
.SH \"SEE ALSO\"
.PP
.BR pdnsd (8),
.BR pdnsd\\-ctl (8)
.PP
More documentation is available in the \\fBdoc/\\fP subdirectory of the source,
or in \\fB/usr/share/doc/pdnsd/\\fP if you are using a binary package.

.SH AUTHORS

\\fBpdnsd\\fP was originally written by Thomas Moestl
.UR
<tmoestl\@gmx.net>
.UE
and was extensively revised by Paul A. Rombouts
.UR
<p.a.rombouts\@home.nl>
.UE
(for versions 1.1.8b1\\-par and later).
.PP
Several others have contributed to \\fBpdnsd\\fP; see files in the source or
\\fB/usr/share/doc/pdnsd/\\fP directory.
.PP
This man page was automatically generated from the html documentation for \\fBpdnsd\\fP,
using a customized Perl script written by Paul A. Rombouts.
ENDOFTRAILER

if(defined($_)) {
    while(<>) {
	if(/last\s+revised/i) {
            s{^\s*}{};
	    s{<[^<>]+>}{}g;
	    s{&lt;}{<}ig;
	    s{&gt;}{>}ig;
	    s{&quot;}{"}ig;
	    s{&nbsp;}{\\ }ig;
	    print ".PP\n";
            print;
            last;
	}
    }
}
exit;
