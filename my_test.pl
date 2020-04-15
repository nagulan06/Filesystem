#!/usr/bin/perl
use 5.16.0;
use warnings FATAL => 'all';

use Test::Simple tests => 42;
use IO::Handle;
use Data::Dumper;

sub mount {
    system("(make mount 2>> error.log) >> test.log &");
    sleep 1;
}

sub unmount {
    system("(make unmount 2>> error.log) >> test.log");
}

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

sub read_text_slice {
    my ($name, $count, $offset) = @_;
    open my $fh, "<", "mnt/$name" or return "";
    my $data;
    seek $fh, $offset, 0;
    read $fh, $data, $count;
    close $fh;
    return $data;
}

sub write_text_off {
    my ($name, $offset, $data) = @_;
    open my $fh, "+<", "mnt/$name" or return "";
    seek $fh, $offset, 0;
    syswrite $fh, $data;
    close $fh;
}

#system("rm -f data.cow test.log");
system("(make 2>error.log) > test.log");
#system("./cowtool new data.cow");

say "#           == Basic Tests ==";
mount();

my $part1 = 0;

sub p1ok {
    my ($cond, $msg) = @_;
    if ($cond) {
        ++$part1;
    }
    else {
        ok(0, $msg);
    }
}

my $msg0 = "hello, one";
write_text("one.txt", $msg0);
p1ok(-e "mnt/one.txt", "File1 exists.");
p1ok(-f "mnt/one.txt", "File1 is regular file.");
my $msg1 = read_text("one.txt");
say "# '$msg0' eq '$msg1'?";
p1ok($msg0 eq $msg1, "read back data1");

my $msg2 = "hello, two";
write_text("two.txt", $msg2);
p1ok(-e "mnt/two.txt", "File2 exists.");
p1ok(-f "mnt/two.txt", "File2 is regular file.");
my $msg3 = read_text("two.txt");
say "# '$msg0' eq '$msg1'?";
p1ok($msg2 eq $msg3, "Read back data2 correctly.");

my $files = `ls mnt`;
p1ok($files =~ /one\.txt/, "one.txt is in the directory");
p1ok($files =~ /two\.txt/, "two.txt is in the directory");

my $long0 = "=This string is fourty characters long.=" x 50;
write_text("2k.txt", $long0);
my $long1 = read_text("2k.txt");
p1ok($long0 eq $long1, "Read back long correctly.");

my $long2 = read_text_slice("2k.txt", 10, 50);
my $right = "ng is four";
p1ok($long2 eq $right, "Read with offset & length");

unmount();

unless (!-e "mnt/one.txt") {
    die "It looks like your filesystem never mounted";
}

p1ok(!-e "mnt/one.txt", "one.txt doesn't exist after umount");

say "# part1 = $part1";
ok($part1 == 11, "No regressions on the easy stuff.");

$files = `ls mnt`;
ok($files !~ /one\.txt/, "one.txt is not in the directory");
ok($files !~ /two\.txt/, "two.txt is not in the directory");

mount();

$files = `ls mnt`;
ok($files =~ /one\.txt/, "one.txt is in the directory still");
ok($files =~ /two\.txt/, "two.txt is in the directory still");

$msg1 = read_text("one.txt");
say "# '$msg0' eq '$msg1'?";
ok($msg0 eq $msg1, "Read back data1 correctly again.");

$msg3 = read_text("two.txt");
say "# '$msg2' eq '$msg3'?";
ok($msg2 eq $msg3, "Read back data2 correctly again.");

system("rm -f mnt/one.txt");
$files = `ls mnt`;
ok($files !~ /one\.txt/, "deleted one.txt");

system("mv mnt/two.txt mnt/abc.txt");
$files = `ls mnt`;
ok($files !~ /two\.txt/, "moved two.txt");
ok($files =~ /abc\.txt/, "have abc.txt");

my $msg4 = read_text("abc.txt");
say "# '$msg2' eq '$msg4'?";
ok($msg2 eq $msg4, "Read back data after rename.");

say "#           == Less Basic Tests ==";

system("mkdir mnt/numbers");
for my $ii (1..50) {
    write_text("numbers/$ii.num", "$ii");
}

my $nn = `ls mnt/numbers | wc -l`;
ok($nn == 50, "created 50 files");