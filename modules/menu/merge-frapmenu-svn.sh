#!/bin/bash

die() {
    echo $1
    exit 1
}

[ -d frapmenu.new ] && rm -rf frapmenu.new

svn co http://svn.xfce.org/svn/xfce/libfrap/trunk/libfrap/menu frapmenu.new \
    || die "svn co failed"

rm -rf frapmenu.new/{ChangeLog,docs,README,STATUS,tests}

for i in `find frapmenu.new -type f | grep -v \.svn`; do
    mv $i ${i/frapmenu.new/frapmenu} || die "can't move $i"
done

rm -rf frapmenu.new

perl -i -ne 'next if(/\ttests/ || /\tdocs/); \
             next if(/EXTRA_DIST/ || /README/ || /STATUS/); \
             s/^\ttdb\s+\\/\ttdb/; \
             s:/libfrap/menu/tdb/:/modules/menu/frapmenu/tdb/:g; \
             print;' frapmenu/Makefile.am

perl -i -ne 's:/libfrap/menu/:/modules/menu/frapmenu:; \
             print;' frapmenu/tdb/Makefile.am
