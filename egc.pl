#!/usr/bin/perl -w

use IO::Socket;

# entropy client

unless (@ARGV >= 2) {
    print "Usage: egc.pl <daemon> <command> [<args>]\n";
    print "  daemon: host:portnum or /path/to/unix/socket\n";
    print "  commands:\n";
    print "     get:  returns bits of entropy in pool\n";
    print "     read N: tries to get N bytes of entropy, nonblocking\n";
    print "     readb N: gets N bytes of entropy, blocking\n";
    print "     write N: reads from stdin, adds N bits to entropy pool\n";
    print "     pid: report PID of daemon\n";
    exit(0);
}

$daemon = shift;
$command = shift;

if ($daemon =~ /:/) {
    $s = new IO::Socket::INET('PeerAddr' => $daemon);
} else {
    $s = new IO::Socket::UNIX('Peer' => $daemon);
}
die "couldn't contact daemon: $!" unless $s;

if ($command eq 'get') {
    $msg = pack("C", 0x00);
    $s->syswrite($msg, length($msg));
    my $nread = $s->sysread($buf, 4);
    die "didn't read all 4 bytes in one shot" if ($nread != 4);
    my $count = unpack("N",$buf);
    print " $count bits of entropy in pool\n";
} elsif ($command eq 'read') {
    my $bytes = shift;
    $bytes = 255 if $bytes > 255;
    $msg = pack("CC", 0x01, $bytes);
    $s->syswrite($msg, length($msg));
    my $nread = $s->sysread($buf, 1);
    die unless $nread == 1;
    my $count = unpack("C",$buf);
    $nread = $s->sysread($buf, $count);
    die "didn't get all the entropy" unless $nread == $count;
    print "got $count bytes of entropy: ",unpack("H*",$buf),"\n";
} elsif ($command eq 'readb') {
    my $bytes = shift;
    $bytes = 255 if $bytes > 255;
    $msg = pack("CC", 0x02, $bytes);
    $s->syswrite($msg, length($msg));
    print "retrieving entropy...\n";
    my $entropy = "";
    while($bytes) {
	my $nread = $s->sysread($buf, $bytes);
	last unless $nread;
	$entropy .= $buf;
	$bytes -= $nread;
	print unpack("H*",$buf),"\n";
    }
    print "done\n";
} elsif ($command eq 'write') {
    my $bits = shift;
    my $data = join('',<>);
    print "got ",length($data)," bytes from stdin\n";
    print " calling it $bits bits of entropy\n";
    $msg = pack("CnC", 0x03, $bits, length($data)) . $data;
    print " sending ",unpack("H*",$msg),"\n";
    $s->syswrite($msg, length($msg));
} elsif ($command eq 'pid') {
    $msg = pack("C", 0x04);
    $s->syswrite($msg, length($msg));
    my $nread = $s->sysread($buf, 1);
    die unless $nread == 1;
    my $count = unpack("C", $buf);
    $nread = $s->sysread($buf, $count);
    die "didn't get whole PID string" unless $nread == $count;
    print "pid: $buf\n";
} elsif ($command eq 'bogus') {
    my $msg = pack("CC", 0x93, 0x12);
    print "sending bogus message\n";
    $s->syswrite($msg, length($msg));
    sleep(1);
    my $buf;
    my $n = $s->sysread($buf, 10);
    if ($n) {
	print " still connected\n";
    } else {
	print " dropped connection\n";
    }
} else {
    print "bad command\n";
    exit(0);
}

