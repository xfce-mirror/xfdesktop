#!/usre/bin/perl -w

use strict;

my $in_comment = 0;

open(POTFILE, '>regcat.pot') or die("can't open regcat.pot: $!");

open(REGXML, '<../../modules/menu/xfce-registered-categories.xml') or die("can't open xfce-registered-categories.xml: $!");
while(my $line = <REGXML>) {
    $in_comment = 1 if(!$in_comment && $line =~ /<!--/);
    $in_comment = 0 if($in_comment && $line =~ /-->/);
    
    next if($in_comment);
    
    if($line =~ /<category .*?name="(.*?)"/) {
        my $name = $1;
        print POTFILE qq(msgid "$name"\nmsgstr ""\n\n);
    }
}
close(REGXML);
close(POTFILE);
