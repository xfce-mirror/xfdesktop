htmldir=$(datadir)/xfce4/doc/html

html_DATA= \
	   xfdesktop-using.html \
	   xfdesktop-copyright.html \
	   xfdesktop-background.html \
	   xfdesktop-workspace.html \
	   xfdesktop.html
	   
EXTRA_DIST = $(html_DATA)

html:
	xsltproc $(srcdir)/../xml/xfce.xsl $(srcdir)/../xml/xfdesktop.xml

