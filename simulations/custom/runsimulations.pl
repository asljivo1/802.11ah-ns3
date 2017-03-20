#!/usr/bin/perl

use strict;


my $START_PORT = 7000;

opendir my $dir, "." or die "Cannot open directory: $!";
my @files;

if(scalar @ARGV == 0) {
	@files = readdir $dir;
}
else {
	@files = @ARGV;
}

closedir $dir;

my %arguments;

readArgs("common.conf", \%arguments);


my $count = 0;
for my $f (@files) {

	if($f =~ /^.*?\.conf$/ && $f ne "common.conf") {
		print "Starting $f\n";
		my %args = (%arguments);
		readArgs($f, \%args);
		
		$args{"--VisualizerPort"} = $START_PORT + $count;

		run($f, \%args);

		$count++;
	}
}


sub readArgs {
	my $file = @_[0];
	my $args = @_[1];

	open my $info, $file or die "Could not open $file: $!";
	while(my $line = <$info>) {
		if($line =~ /(.*?)=(.*)/) {
			#print "$1 -> $2\n";
			$args->{$1} = $2;
		}
	}
	close $info;
}

sub run {
	my $file = @_[0];
	my $args = @_[1];
	
	#print join("\n", map { $_ . ": " . $args->{$_} } keys %{ $args });

	my $argsString = join(" ", map { $_ . "=" . $args->{$_} } keys %{ $args });
	$argsString =~ s/\"/\\\"/g;

	my $logFile = $file . ".log";
	my $exec = '(cd ../ns-3/ && ./waf --run "scratch/ahsimulation/ahsimulation ' . $argsString . '") 1> ' . "/proj/wall2-ilabt-iminds-be/ns3ah/" . $logFile . ' 2>&1 &';

	print "$exec\n";

	system($exec);
	# allow some time to finish the ./waf build json building
	sleep 1
}
