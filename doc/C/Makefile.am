SUBDIRS = \
	images

noinst_DATA = \
	xfdesktop.xml

docdir=$(datadir)/xfce4/doc/C

doc_DATA= \
	xfdesktop.html

EXTRA_DIST = $(noinst_DATA) $(doc_DATA)

html: xfdesktop.html

xfdesktop.html: $(srcdir)/xfdesktop.xml
	xsltproc $(top_srcdir)/doc/xfce-nochunk.xsl $(srcdir)/xfdesktop.xml
