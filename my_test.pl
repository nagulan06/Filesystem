#!/usr/bin/perl
use 5.16.0;
use warnings FATAL => 'all';

use IO::Handle;
use Data::Dumper;

sub write_text {
    my ($name, $data) = @_;
    open my $fh, ">", "mnt/$name" or return;
    $fh->say($data);
    close $fh;
}

sub read_text {
    my ($name) = @_;
    open my $fh, "<", "mnt/$name" or return "";
    local $/ = undef;
    my $data = <$fh> || "";
    close $fh;
    $data =~ s/\s*$//;
    return $data;
}

system("mkdir mnt/numbers");
for my $ii (1..50) {
    write_text("numbers/$ii.num", "$ii");
}

my $nn = `ls mnt/numbers | wc -l`;
say $nn;
