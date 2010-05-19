SUBDIRS = \
	images

noinst_DATA = \
	xfdesktop.xml

docdir=$(datadir)/xfce4/doc/am

doc_DATA= \
	xfdesktop.html

EXTRA_DIST = $(noinst_DATA) $(doc_DATA)

if MAINTAINER_MODE
if HAVE_XSLTPROC

xfdesktop.html: $(srcdir)/xfdesktop.xml
	$(XSLTPROC) $(top_srcdir)/doc/xfce-nochunk.xsl $(srcdir)/xfdesktop.xml

endif
endif
