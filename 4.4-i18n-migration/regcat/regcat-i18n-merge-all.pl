#!/usr/bin/perl -w

use strict;

my $dest_podir = '../../../../trunk/po';
my @src_pofiles = <*.po>;

foreach my $spo (@src_pofiles) {
    open(DPO, '<'.$dest_podir.'/'.$spo) or do {
        warn("no dest po $dest_podir.'/'.$spo");
        next;
    };
    open(SPO, '<'.$spo) or die("can't open source po $spo: $!");
    open(DPON, '>'.$dest_podir.'/'.$spo.'.new') or die("can't write to $dest_podir/$spo.new: $!");
    
    my %trans = ();
    while(my $line = <SPO>) {
        next if($line !~ /^msgid "(.*?)"$/);
        my $msgid = $1;
        $line = <SPO>;
        next if($line !~ /^msgstr "(.*?)"$/);
        my $msgstr = $1;
        
        $trans{$msgid} = $msgstr if(length($msgstr));
    }
    
    while(my $line = <DPO>) {
       if($line =~ /^msgid\s+"(.*?)"\s*$/) {
           my $msgid = $1;
           print DPON $line;
           $line = <DPO>;
           if(defined($trans{$msgid}) && $line =~ /^msgstr\s+""\s*$/) {
               my $msgstr = $trans{$msgid};
               print DPON qq(msgstr "$msgstr"\n);
           } else {
               print DPON $line;
           }
       } else {
           print DPON $line;
       }
    }
    
    close(DPO);
    close(SPO);
    close(DPON);
}
