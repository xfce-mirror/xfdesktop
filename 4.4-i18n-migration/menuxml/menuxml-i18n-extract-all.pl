#!/usr/bin/perl -w

use strict;

my @xmlfiles = <../../menu.xml.*>;

my %msgids = ();
open(POTFILE, '<menuxml.pot') or die ("where's the pot? ($!)");
while(my $line = <POTFILE>) {
    next if($line !~ /cmd=(.*)$/);
    my $cmd = $1;
    $line = <POTFILE>;
    next if($line !~ /^msgid "(.*?)"$/);
    my $msgid = $1;
    
    $msgids{$cmd} = $msgid if(length($msgid));
}
close(POTFILE);

foreach my $xf (@xmlfiles) {
    my $lang = $1 if($xf =~ /\.(\w+)$/);
    die("lang is empty for $xf") if(!length($lang));
    
    open(MENUXML, '<'.$xf) or die("can't open $xf: $!");
    open(POFILE, '>'.$lang.'.po') or die("can't write to $lang.po: $!");
    
    my $in_comment = 0;
    while(my $line = <MENUXML>) {
        $in_comment = 1 if(!$in_comment && $line =~ /<!--/);
        $in_comment = 0 if($in_comment && $line =~ /-->/);
        
        next if $in_comment;
        
        if($line =~ /cmd="(.*?)"/) {
            my $cmd = $1;
            
            if(!defined($msgids{$cmd})) {
                warn("no msgid for $cmd in $xf");
            } else {
                my $msgid = $msgids{$cmd};
                
                if($line =~ /name="(.*?)"/) {
                    my $msgstr = $1;
                    print POFILE qq(# cmd=$cmd\nmsgid "$msgid"\nmsgstr "$msgstr"\n\n);
                }
            }
        }
    }
    
    close(MENUXML);
    close(POFILE);
}
