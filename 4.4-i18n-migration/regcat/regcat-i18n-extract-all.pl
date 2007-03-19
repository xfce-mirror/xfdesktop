#!/usr/bin/perl -w

use strict;

my @xmlfiles = <../../modules/menu/xfce-registered-categories.xml.*>;

foreach my $xf (@xmlfiles) {
    my $lang = $1 if($xf =~ /\.(\w+)$/);
    die("lang is empty for $xf") if(!length($lang));
    
    open(REGXML, '<'.$xf) or die("can't open $xf: $!");
    open(POFILE, '>'.$lang.'.po') or die("can't write to $lang.po: $!");
    
    my $in_comment = 0;
    while(my $line = <REGXML>) {
        $in_comment = 1 if(!$in_comment && $line =~ /<!--/);
        $in_comment = 0 if($in_comment && $line =~ /-->/);
        
        next if $in_comment;
        
        if($line =~  /name="(.*?)"/) {
            my $name = $1;
            my $replace = '';
            $replace = $1 if($line =~ /replace="(.*?)"/);
            
            print POFILE qq(msgid "$name"\nmsgstr "$replace"\n\n);
        }
    }
    
    close(REGXML);
    close(POFILE);
}
