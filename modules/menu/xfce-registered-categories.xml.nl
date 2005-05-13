<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE xfce-registered-categories>

<!-- Note: You can copy this file to ~/.config/xfce4/desktop/ for customisation. -->

<!--
	Here is how this file works:

	<xfce-registered-categories> - root element, required.

	<category>
		name:  Required.  Corresponds to an official category from the
		  freedesktop.org menu spec.
		replace: Optional.  Name that is displayed as the menu name for items
		  in the category (useful, e.g., for translations).
		icon: icon to be displayed for this category (actually, for the menu
		  item referenced by this category's 'replace' attribute)
		toplevel: Optional.  Marks categories as being "allowed" to be toplevel
		  menus.  Note: if a category is unrooted, it will be promoted to
		  toplevel regardless of the value of this attribute. (default: false)
		ignore: Optional.  This category will be ignored when organising menu
		  items. (default: false)
		hide: Optional.  Items referencing this category will not appear in any
			menu. (default: false)

	<subcategory> - sub-element of <category>, specifies a subcategory relation
		name: Required.  Corresponds to an official category from the
		  freedesktop.org menu spec.
	  Example:
	  <category name="Graphics">
		  <subcategory name="RasterGraphics">
	  </category>
	  Note that even if subcategories are definied, it is possible to collapse
	  all subcategories into a flat one-level menu by specifying style="simple"
	  in the <include> tag in your menu.xml file.
-->

<xfce-registered-categories>
	<category name="Legacy" toplevel="true" replace="Accessoires" builtin-icon="UTILITY" />
	<category name="Core" toplevel="true" replace="Accessoires" builtin-icon="UTILITY" />
	<category name="Development" toplevel="true" replace="Programmeren" builtin-icon="DEVELOPMENT">
		<subcategory name="Building" />
		<subcategory name="Debugger" />
		<subcategory name="IDE" />
		<subcategory name="GUIDesigner" />
		<subcategory name="Profiling" />
		<subcategory name="RevisionControl" />
		<subcategory name="Translation" />
		<subcategory name="Database" />
		<subcategory name="ProjectManagement" />
		<subcategory name="WebDevelopment" />
	</category>
	<category name="Building" builtin-icon="DEVELOPMENT" />
	<category name="Debugger" replace="Debugging" builtin-icon="DEVELOPMENT" />
	<category name="IDE" replace="Environments" builtin-icon="DEVELOPMENT" />
	<category name="GUIDesigner" replace="GUI Designers" builtin-icon="DEVELOPMENT" />
	<category name="Profiling" builtin-icon="DEVELOPMENT" />
	<category name="RevisionControl" replace="Versiebeheer" builtin-icon="DEVELOPMENT" />
	<category name="Translation" replace="Vertalen" builtin-icon="DEVELOPMENT" />
	<category name="Office" replace="Kantoor" toplevel="true" builtin-icon="PRODUCTIVITY">
		<subcategory name="Calendar" />
		<subcategory name="ContactManagement" />
		<subcategory name="Database" />
		<subcategory name="Dictionary" />
		<subcategory name="Chart" />
		<subcategory name="Email" />
		<subcategory name="Finance" />
		<subcategory name="FlowChart" />
		<subcategory name="PDA" />
		<subcategory name="Project Management" />
		<subcategory name="Presentation" />
		<subcategory name="Spreadsheet" />
		<subcategory name="WordProcessor" />
		<subcategory name="Photograph" />
		<subcategory name="Viewer" />
	</category>
	<category name="Calendar" replace="Agenda" builtin-icon="PRODUCTIVITY" />
	<category name="ContactManagement" replace="Contact Management" builtin-icon="PRODUCTIVITY" />
	<category name="Database" builtin-icon="PRODUCTIVITY" />
	<category name="Dictionary" replace="Woordenboek" builtin-icon="PRODUCTIVITY" />
	<category name="Chart" replace="Grafieken" builtin-icon="PRODUCTIVITY" />
	<category name="Email" builtin-icon="PRODUCTIVITY" />
	<category name="Finance" replace="Financieel" builtin-icon="PRODUCTIVITY" />
	<category name="FlowChart" replace="Flow Chart" builtin-icon="PRODUCTIVITY" />
	<category name="PDA" builtin-icon="PRODUCTIVITY" />
	<category name="ProjectManagement" replace="Project Management" builtin-icon="PRODUCTIVITY" />
	<category name="Presentation" replace="Presentatie" builtin-icon="PRODUCTIVITY" />
	<category name="Spreadsheet" builtin-icon="PRODUCTIVITY" />
	<category name="WordProcessor" replace="Tekstverwerking" builtin-icon="PRODUCTIVITY" />
	<category name="Graphics" replace="Grafisch" toplevel="true" builtin-icon="GRAPHICS">
		<subcategory name="2DGraphics" />
		<subcategory name="3DGraphics" />
		<subcategory name="Scanning" />
		<subcategory name="Photograph" />
		<subcategory name="Viewer" />
	</category>
	<category name="2DGraphics" replace="2-D Graphics" builtin-icon="GRAPHICS">
		<subcategory name="VectorGraphics" />
		<subcategory name="RasterGraphics" />
	</category>
	<category name="VectorGraphics" replace="Vector Graphics" builtin-icon="GRAPHICS" />
	<category name="RasterGraphics" replace="Raster Graphics" builtin-icon="GRAPHICS" />
	<category name="3DGraphics" replace="3-D Graphics" builtin-icon="GRAPHICS" />
	<category name="Scanning" builtin-icon="GRAPHICS">
		<subcategory name="OCR" />
	</category>
	<category name="OCR" replace="Tekstherkenning" builtin-icon="GRAPHICS" />
	<category name="Photograph" replace="Fotografie" builtin-icon="GRAPHICS" />
	<category name="Viewer" replace="Viewers" builtin-icon="GRAPHICS" />
	<category name="Settings" replace="Instellingen" toplevel="true" builtin-icon="SETTINGS">
		<subcategory name="DesktopSettings" />
		<subcategory name="HardwareSettings" />
		<subcategory name="PackageSettings" />
		<subcategory name="Security" />
		<subcategory name="Accessibility" />
		<subcategory name="Screensaver" />
	</category>
	<category name="DesktopSettings" replace="Desktop-instelingen" builtin-icon="SETTINGS" />
	<category name="HardwareSettings" replace="Hardware-instellingen" builtin-icon="SETTINGS" />
	<category name="PackageSettings" replace="Paket-instellingen" builtin-icon="SETTINGS" />
	<category name="Network" toplevel="true" replace="Netwerk" builtin-icon="NETWORK">
		<subcategory name="Email" />
		<subcategory name="Dialup" />
		<subcategory name="InstantMessaging" />
		<subcategory name="IRCClient" />
		<subcategory name="FileTransfer" />
		<subcategory name="HamRadio" />
		<subcategory name="News" />
		<subcategory name="P2P" />
		<subcategory name="RemoteAccess" />
		<subcategory name="Telephony" />
		<subcategory name="WebBrowser" />
		<subcategory name="WebDevelopment" />
	</category>
	<category name="Dialup" replace="Inbellen" builtin-icon="NETWORK" />
	<category name="InstantMessaging" replace="Instant Messaging" builtin-icon="NETWORK" />
	<category name="IRCClient" replace="IRC Clients" builtin-icon="NETWORK" />
	<category name="FileTransfer" replace="Bestandsoverdracht" builtin-icon="NETWORK" />
	<category name="HamRadio" replace="Ham Radio" builtin-icon="NETWORK" />
	<category name="News" replace="Nieuws" builtin-icon="NETWORK" />
	<category name="P2P" replace="Peer-to-Peer" builtin-icon="NETWORK" />
	<category name="RemoteAccess" replace="Remote Access" builtin-icon="NETWORK" />
	<category name="Telephony" replace="Telefoon" builtin-icon="NETWORK" />
	<category name="WebBrowser" replace="Internet" builtin-icon="NETWORK" />
	<category name="WebDevelopment" replace="Web Development" builtin-icon="NETWORK" />
	<category name="AudioVideo" replace="Multimedia" toplevel="true" builtin-icon="MULTIMEDIA">
		<subcategory name="Database" />
		<subcategory name="HamRadio" />
		<subcategory name="Audio" />
		<subcategory name="Video" />
		<subcategory name="AudioVideoEditing" />
		<subcategory name="Player" />
		<subcategory name="Recorder" />
		<subcategory name="DiscBurning" />
	</category>
	<category name="Audio" builtin-icon="MULTIMEDIA">
		<subcategory name="Midi" />
		<subcategory name="Mixer" />
		<subcategory name="Sequencer" />
		<subcategory name="Tuner" />
		<subcategory name="AudioVideoEditing" />
		<subcategory name="Player" />
		<subcategory name="Recorder" />
	</category>
	<category name="Midi"  builtin-icon="MULTIMEDIA" />
	<category name="Mixer" replace="Sound Mixers" builtin-icon="MULTIMEDIA" />
	<category name="Sequencer" replace="Sequencers" builtin-icon="MULTIMEDIA" />
	<category name="Tuner" replace="Tuners" builtin-icon="MULTIMEDIA" />
	<category name="Video" builtin-icon="MULTIMEDIA">
		<subcategory name="TV" />
		<subcategory name="AudioVideoEditing" />
		<subcategory name="Player" />
		<subcategory name="Recorder" />
		<subcategory name="Music" />
	</category>
	<category name="TV" builtin-icon="MULTIMEDIA" />
	<category name="AudioVideoEditing" replace="Editing" builtin-icon="MULTIMEDIA" />
	<category name="Player" replace="Media Players" builtin-icon="MULTIMEDIA" />
	<category name="Recorder" replace="Recording" builtin-icon="MULTIMEDIA" />
	<category name="DiscBurning" replace="CD en DVD Branden" builtin-icon="MULTIMEDIA" />
	<category name="Game" toplevel="true" replace="Spellen" builtin-icon="GAME">
		<subcategory name="ActionGame" />
		<subcategory name="AdventureGame" />
		<subcategory name="ArcadeGame" />
		<subcategory name="BoardGame" />
		<subcategory name="BlocksGame" />
		<subcategory name="CardGame" />
		<subcategory name="KidsGame" />
		<subcategory name="LogicGame" />
		<subcategory name="RolePlaying" />
		<subcategory name="Simulation" />
		<subcategory name="SportsGame" />
		<subcategory name="StrategyGame" />
	</category>
	<category name="ActionGame" replace="Action" builtin-icon="GAME" />
	<category name="AdventureGame" replace="Adventure" builtin-icon="GAME" />
	<category name="ArcadeGame" replace="Arcade" builtin-icon="GAME" />
	<category name="BoardGame" replace="Bordspellen" builtin-icon="GAME" />
	<category name="BlocksGame" replace="Puzzels" builtin-icon="GAME" />
	<category name="CardGame" replace="Kaartspellen" builtin-icon="GAME" />
	<category name="KidsGame" replace="Kinderspellen" builtin-icon="GAME" />
	<category name="LogicGame" replace="Logica" builtin-icon="GAME" />
	<category name="RolePlaying" replace="Role Playing" builtin-icon="GAME" />
	<category name="Simulation" replace="Simulatie" builtin-icon="GAME" />
	<category name="SportsGame" replace="Sport" builtin-icon="GAME" />
	<category name="StrategyGame" replace="Strategie" builtin-icon="GAME" />
	<category name="Education" replace="Educatief" toplevel="true">
		<subcategory name="Art" />
		<subcategory name="Construction" />
		<subcategory name="Music" />
		<subcategory name="Languages" />
		<subcategory name="Teaching" />
	</category>
	<category name="Art" replace="Kunst"/>
	<category name="Construction" replace="Bouw"/>
	<category name="Music" replace="Muziek"/>
	<category name="Languages" replace="Talen"/>
	<category name="Science" toplevel="true" replace="Wetenschap">
		<subcategory name="Astronomy" />
		<subcategory name="Biology" />
		<subcategory name="Chemistry" />
		<subcategory name="Geology" />
		<subcategory name="Math" />
		<subcategory name="MedicalSoftware" />
		<subcategory name="Physics" />
	</category>
	<category name="Astronomy" replace="Astronomie"/>
	<category name="Biology" replace="Biologie"/>
	<category name="Chemistry" replace="Scheikunde"/>
	<category name="Geology" replace="Geologie"/>
	<category name="Math" replace="Wiskunde"/>
	<category name="MedicalSoftware" replace="Geneeskunde" />
	<category name="Physics" replace="Natuurkunde"/>
	<category name="Teaching" replace="Lesgeven"/>
	<category name="Amusement" replace="Spellen" builtin-icon="GAME" />
	<category name="Applet" replace="Applets" />
	<category name="Archiving" replace="Archiveren"/>
	<category name="Electronics" replace="Electro"/>
	<category name="Emulator" toplevel="true" replace="Systeem" builtin-icon="SYSTEM" />
	<category name="Engineering" replace="Techniek"/>
	<category name="FileManager" replace="Bestandsbeheer" />
	<category name="Shell" replace="Shells" />
	<category name="Screensaver" />
	<category name="TerminalEmulator" replace="Terminals" />
	<category name="TrayIcon" replace="System Tray Pictogrammen" />
	<category name="System" replace="Systeem" toplevel="true" builtin-icon="SYSTEM">
		<subcategory name="FileSystem" />
		<subcategory name="Monitor" />
		<subcategory name="Security" />
		<subcategory name="Accessibility" />
		<subcategory name="TerminalEmulator" />
		<subcategory name="Shell" />
	</category>
	<category name="Filesystem" replace="Bestandssysteem" builtin-icon="SYSTEM" />
	<category name="Monitor" builtin-icon="SYSTEM" />
	<category name="Security" replace="Beveiliging" builtin-icon="SYSTEM" />
	<category name="Utility" toplevel="true" replace="Accessoires" builtin-icon="UTILITY">
		<subcategory name="Accessibility" />
		<subcategory name="Calculator" />
		<subcategory name="Clock" />
		<subcategory name="TextEditor" />
		<subcategory name="Archiving" />
		<subcategory name="TerminalEmulator" />
		<subcategory name="FileManager" />
		<subcategory name="Shell" />
	</category>
	<category name="Accessibility" replace="Toegankelijkheid" builtin-icon="UTILITY" />
	<category name="Calculator" replace="Rekenmachines" builtin-icon="UTILITY" />
	<category name="Clock" replace="Datum en Tijd" builtin-icon="UTILITY" />
	<category name="TextEditor" replace="Tekst Editors" builtin-icon="UTILITY" />
	<category name="KDE" toplevel="true" ignore="true">
		<subcategory name="Development" />
		<subcategory name="Office" />
		<subcategory name="Graphics" />
		<subcategory name="Settings" />
		<subcategory name="Network" />
		<subcategory name="AudioVideo" />
		<subcategory name="Game" />
		<subcategory name="Education" />
		<subcategory name="Science" />
		<subcategory name="System" />
		<subcategory name="Utility" />
	</category>
	<category name="GNOME" toplevel="true" ignore="true">
		<subcategory name="Development" />
		<subcategory name="Office" />
		<subcategory name="Graphics" />
		<subcategory name="Settings" />
		<subcategory name="Network" />
		<subcategory name="AudioVideo" />
		<subcategory name="Game" />
		<subcategory name="Education" />
		<subcategory name="Science" />
		<subcategory name="System" />
		<subcategory name="Utility" />
	</category>
	<category name="GTK" toplevel="true" ignore="true">
		<subcategory name="GNOME" />
	</category>
	<category name="Qt" toplevel="true" ignore="true">
		<subcategory name="KDE" />
	</category>
	<category name="Motif" toplevel="true" ignore="true">
		<subcategory name="Development" />
		<subcategory name="Office" />
		<subcategory name="Graphics" />
		<subcategory name="Settings" />
		<subcategory name="Network" />
		<subcategory name="AudioVideo" />
		<subcategory name="Game" />
		<subcategory name="Education" />
		<subcategory name="Science" />
		<subcategory name="System" />
		<subcategory name="Utility" />
	</category>
	<category name="Java" toplevel="true" ignore="true">
		<subcategory name="Applet" />
	</category>
	<category name="ConsoleOnly" toplevel="true" replace="Console" ignore="true">
		<subcategory name="Development" />
		<subcategory name="Office" />
		<subcategory name="Graphics" />
		<subcategory name="Settings" />
		<subcategory name="Network" />
		<subcategory name="AudioVideo" />
		<subcategory name="Game" />
		<subcategory name="Education" />
		<subcategory name="Science" />
		<subcategory name="System" />
		<subcategory name="Utility" />
	</category>
	<category name="Wine" toplevel="true" builtin-icon="WINE" />
	<category name="WineX" toplevel="true" builtin-icon="WINE" />
</xfce-registered-categories>
