#!/usre/bin/perl -w

use strict;

my $in_comment = 0;

open(POTFILE, '>menuxml.pot') or die("can't open menuxml.pot: $!");

open(MENUXML, '<../../menu.xml') or die("can't open menu.xml: $!");
while(my $line = <MENUXML>) {
    $in_comment = 1 if(!$in_comment && $line =~ /<!--/);
    $in_comment = 0 if($in_comment && $line =~ /-->/);
    
    next if($in_comment);
    
    if($line =~ /cmd="(.*?)"/) {
        my $cmd = $1;
        if($line =~ /name="(.*?)"/) {
            my $str = $1;
            print POTFILE qq(# cmd=$cmd\nmsgid "$str"\nmsgstr ""\n\n);
        }
    }
}
close(MENUXML);
close(POTFILE);
