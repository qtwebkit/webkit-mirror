#!/usr/bin/perl -w

use CGI;
use HTTP::Date;

my $query = new CGI;
@names = $query->param;
my $expiresInFutureIn304 = $query->param('expires-in-future-in-304') || 0;
my $delay = $query->param('delay') || 0;
my $body = $query->param('body') || 0;

if ($body eq "unique") {
    $body = sprintf "%08X\n", rand(0xffffffff)
}

my $hasStatusCode = 0;
my $hasExpiresHeader = 0;
if ($query->http && $query->http("If-None-Match") eq "match") {
    print "Status: 304\n";
    $hasStatusCode = 1;
    if ($expiresInFutureIn304) {
        print "Expires: " . HTTP::Date::time2str(time() + 100) . "\n";
        $hasExpiresHeader = 1;
    }
}

if ($query->http && $query->param("Range") =~ /bytes=(\d+)-(\d+)/) {

    if ($1 < 6 && $2 < 6) {
        print "Status: 206\n";
    } else {
	print "Status: 416\n";
    }

    $hasStatusCode = 1;
}

foreach (@names) {
    next if ($_ eq "uniqueId");
    next if ($_ eq "delay");
    next if ($_ eq "body");
    next if ($_ eq "Status" and $hasStatusCode);
    next if ($_ eq "Expires" and $hasExpiresHeader);
    print $_ . ": " . $query->param($_) . "\n";
}
print "\n";
if ($delay) {
    # Include some padding so headers and full body are sent separately.
    for (my $i=0; $i < 1024; $i++) {
        print "                                                                                ";
    }
    sleep $delay;
}
print $body if $body;
