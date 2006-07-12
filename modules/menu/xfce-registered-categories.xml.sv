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
	<category name="Legacy" toplevel="true" replace="Tillbehör" builtin-icon="ACCESSORIES" />
	<category name="Core" toplevel="true" replace="Tillbehör" builtin-icon="ACCESSORIES" />
	<category name="Development" toplevel="true" replace="Utveckling" builtin-icon="DEVELOPMENT">
		<subcategory name="Building" />
		<subcategory name="Debugger"  replace="Felsökning" />
		<subcategory name="IDE" replace="Utvecklingsmiljöer" />
		<subcategory name="GUIDesigner" replace="Användargränssnitt" />
		<subcategory name="Profiling" replace="Profilering" />
		<subcategory name="RevisionControl" replace="Revisionskontroll" />
		<subcategory name="Translation" replace="Översättning" />
		<subcategory name="Database" replace="Databas" />
		<subcategory name="ProjectManagement" replace="Projekthantering" />
		<subcategory name="WebDevelopment" replace="Webbutveckling" />
	</category>
	<category name="Building" builtin-icon="DEVELOPMENT" />
	<category name="Debugger" replace="Felsökning" builtin-icon="DEVELOPMENT" />
	<category name="IDE" replace="Utvecklingsmiljöer" builtin-icon="DEVELOPMENT" />
	<category name="GUIDesigner" replace="Användargränssnitt" builtin-icon="DEVELOPMENT" />
	<category name="Profiling" replace="Profilering" builtin-icon="DEVELOPMENT" />
	<category name="RevisionControl" replace="Revisionskontroll" builtin-icon="DEVELOPMENT" />
	<category name="Translation" replace="Översättning" builtin-icon="DEVELOPMENT" />
	<category name="Office" toplevel="true" replace="Kontor" builtin-icon="OFFICE">
		<subcategory name="Calendar" replace="Kalender" />
		<subcategory name="ContactManagement" replace="Kontakthantering" />
		<subcategory name="Database" replace="Databas" />
		<subcategory name="Dictionary" replace="Ordlistor" />
		<subcategory name="Chart" />
		<subcategory name="Email" replace="E-post" />
		<subcategory name="Finance" replace="Finans" />
		<subcategory name="FlowChart" />
		<subcategory name="PDA" />
		<subcategory name="ProjectManagement" replace="Projekthantering" />
		<subcategory name="Presentation" />
		<subcategory name="Spreadsheet" replace="Kalkylprogram" />
		<subcategory name="WordProcessor" replace="Ordbehandlare" />
		<subcategory name="Photograph" replace="Fotografier" />
		<subcategory name="Viewer" replace="Visare" />
	</category>
	<category name="Calendar" replace="Kalender" builtin-icon="OFFICE" />
	<category name="ContactManagement" replace="Kontakthantering" builtin-icon="OFFICE" />
	<category name="Database" replace="Databas" builtin-icon="OFFICE" />
	<category name="Dictionary" replace="Ordlistor" builtin-icon="OFFICE" />
	<category name="Chart" builtin-icon="OFFICE" />
	<category name="Email" replace="E-post" builtin-icon="OFFICE" />
	<category name="Finance" replace="Finans" builtin-icon="OFFICE" />
	<category name="FlowChart" replace="Flow Chart" builtin-icon="OFFICE" />
	<category name="PDA" builtin-icon="OFFICE" />
	<category name="ProjectManagement" replace="Projekthantering" builtin-icon="OFFICE" />
	<category name="Presentation" builtin-icon="OFFICE" />
	<category name="Spreadsheet" replace="Kalkylprogram" builtin-icon="OFFICE" />
	<category name="WordProcessor" replace="Ordbehandlare" builtin-icon="OFFICE" />
	<category name="Graphics" toplevel="true" replace="Grafik" builtin-icon="GRAPHICS">
		<subcategory name="2DGraphics" replace="2d-grafik" />
		<subcategory name="3DGraphics" replace="3d-grafik" />
		<subcategory name="Scanning" replace="Skanna" />
		<subcategory name="Photograph" replace="Fotografier" />
		<subcategory name="Viewer" replace="Visare" />
	</category>
	<category name="2DGraphics" replace="2d-grafik" builtin-icon="GRAPHICS">
		<subcategory name="VectorGraphics" replace="Vektorgrafik" />
		<subcategory name="RasterGraphics" replace="Bitmappsgrafik" />
	</category>
	<category name="VectorGraphics" replace="Vektorgrafik" builtin-icon="GRAPHICS" />
	<category name="RasterGraphics" replace="Bitmappsgrafik" builtin-icon="GRAPHICS" />
	<category name="3DGraphics" replace="3d-grafik" builtin-icon="GRAPHICS" />
	<category name="Scanning" replace="Skanna" builtin-icon="GRAPHICS">
		<subcategory name="OCR" />
	</category>
	<category name="OCR" builtin-icon="GRAPHICS" />
	<category name="Photograph" replace="Fotografier" builtin-icon="GRAPHICS" />
	<category name="Viewer" replace="Visare" builtin-icon="GRAPHICS" />
	<category name="Settings" replace="Inställningar" toplevel="true" builtin-icon="SETTINGS">
		<subcategory name="DesktopSettings" replace="Inställningar för skrivbordet" />
		<subcategory name="HardwareSettings" replace="Inställningar för hårdvara" />
		<subcategory name="PackageSettings" replace="Inställningar för paket" />
		<subcategory name="Security" replace="Säkerhet" />
		<subcategory name="Accessibility" replace="Hjälpmedelsfunktioner" />
		<subcategory name="Screensaver" replace="Skärmsläckare" />
	</category>
	<category name="DesktopSettings" replace="Inställningar för skrivbordet" builtin-icon="SETTINGS" />
	<category name="HardwareSettings" replace="Inställningar för hårdvara" builtin-icon="SETTINGS" />
	<category name="PackageSettings" replace="Inställningar för paket" builtin-icon="SETTINGS" />
	<category name="Network" toplevel="true" replace="Nätverk" builtin-icon="NETWORK">
		<subcategory name="Email" replace="E-post" />
		<subcategory name="Dialup" replace="Uppringt internet" />
		<subcategory name="InstantMessaging" />
		<subcategory name="IRCClient" replace="IRC-klienter" />
		<subcategory name="FileTransfer" replace="Filöverföring" />
		<subcategory name="HamRadio" />
		<subcategory name="News" replace="Nyheter" />
		<subcategory name="P2P" replace="Peer-to-peer" />
		<subcategory name="RemoteAccess" replace="Fjärråtkomst" />
		<subcategory name="Telephony" replace="Telefoni" />
		<subcategory name="WebBrowser" replace="Webbläsare" />
		<subcategory name="WebDevelopment" replace="Webbutveckling" />
	</category>
	<category name="Dialup" replace="Uppringt internet" builtin-icon="NETWORK" />
	<category name="InstantMessaging" replace="Instant Messaging" builtin-icon="NETWORK" />
	<category name="IRCClient" replace="IRC-klienter" builtin-icon="NETWORK" />
	<category name="FileTransfer" replace="Filöverföring" builtin-icon="NETWORK" />
	<category name="HamRadio" replace="Ham Radio" builtin-icon="NETWORK" />
	<category name="News" replace="Nyheter" builtin-icon="NETWORK" />
	<category name="P2P" replace="Peer-to-Peer" builtin-icon="NETWORK" />
	<category name="RemoteAccess" replace="Fjärråtkomst" builtin-icon="NETWORK" />
	<category name="Telephony" replace="Telefoni" builtin-icon="NETWORK" />
	<category name="WebBrowser" replace="Webbläsare" builtin-icon="NETWORK" />
	<category name="WebDevelopment" replace="Webbutveckling" builtin-icon="NETWORK" />
	<category name="AudioVideo" replace="Multimedia" toplevel="true" builtin-icon="MULTIMEDIA">
		<subcategory name="Database" replace="Databas" />
		<subcategory name="HamRadio" replace="Ham Radio" />
		<subcategory name="Audio" replace="Ljud" />
		<subcategory name="Video" replace="Video" />
		<subcategory name="AudioVideoEditing" replace="Ljud- och videoredigering" />
		<subcategory name="Player" replace="Mediaspelare" />
		<subcategory name="Recorder" replace="Inspelare" />
		<subcategory name="DiscBurning" replace="Bränn CD och DVD" />
	</category>
	<category name="Audio" builtin-icon="MULTIMEDIA">
		<subcategory name="Midi" />
		<subcategory name="Mixer" />
		<subcategory name="Sequencer" />
		<subcategory name="Tuner" />
		<subcategory name="AudioVideoEditing" replace="Ljud- och videoredigering" />
		<subcategory name="Player" replace="Mediaspelare" />
		<subcategory name="Recorder" replace="Inspelning" />
	</category>
	<category name="Midi"  builtin-icon="MULTIMEDIA" />
	<category name="Mixer" replace="Sound Mixers" builtin-icon="MULTIMEDIA" />
	<category name="Sequencer" replace="Sequencers" builtin-icon="MULTIMEDIA" />
	<category name="Tuner" replace="Tuners" builtin-icon="MULTIMEDIA" />
	<category name="Video" builtin-icon="MULTIMEDIA">
		<subcategory name="TV" />
		<subcategory name="AudioVideoEditing" replace="Ljud- och videoredigering" />
		<subcategory name="Player" replace="Spelare" />
		<subcategory name="Recorder" replace="Inspelning" />
		<subcategory name="Music" replace="Musik" />
	</category>
	<category name="TV" builtin-icon="MULTIMEDIA" />
	<category name="AudioVideoEditing" replace="Ljud- och videoredigering" builtin-icon="MULTIMEDIA" />
	<category name="Player" replace="Mediaspelare" builtin-icon="MULTIMEDIA" />
	<category name="Recorder" replace="Inspelning" builtin-icon="MULTIMEDIA" />
	<category name="DiscBurning" replace="Bränn CD och DVD" builtin-icon="MULTIMEDIA" />
	<category name="Game" toplevel="true" replace="Spel" builtin-icon="GAME">
		<subcategory name="ActionGame" replace="Actionspel" />
		<subcategory name="AdventureGame" replace="Äventyrsspel" />
		<subcategory name="ArcadeGame" replace="Arkadspel" />
		<subcategory name="BoardGame" replace="Brädspel" />
		<subcategory name="BlocksGame" replace="Pusselspel" />
		<subcategory name="CardGame" replace="Kortspel" />
		<subcategory name="KidsGame" replace="Barnspel" />
		<subcategory name="LogicGame" replace="Logikspel" />
		<subcategory name="RolePlaying" replace="Rollspel" />
		<subcategory name="Simulation" replace="Simulatorer" />
		<subcategory name="SportsGame" replace="Sportspel" />
		<subcategory name="StrategyGame" replace="Strategispel" />
	</category>
	<category name="ActionGame" replace="Actionspel" builtin-icon="GAME" />
	<category name="AdventureGame" replace="Äventyrsspel" builtin-icon="GAME" />
	<category name="ArcadeGame" replace="Arkadspel" builtin-icon="GAME" />
	<category name="BoardGame" replace="Brädspel" builtin-icon="GAME" />
	<category name="BlocksGame" replace="Pusselspel" builtin-icon="GAME" />
	<category name="CardGame" replace="Kortspel" builtin-icon="GAME" />
	<category name="KidsGame" replace="Barnspel" builtin-icon="GAME" />
	<category name="LogicGame" replace="Logikspel" builtin-icon="GAME" />
	<category name="RolePlaying" replace="Rollspel" builtin-icon="GAME" />
	<category name="Simulation" replace="Simulatorer" builtin-icon="GAME" />
	<category name="SportsGame" replace="Sportspel" builtin-icon="GAME" />
	<category name="StrategyGame" replace="Strategispel" builtin-icon="GAME" />
	<category name="Education" replace="Utbildning" toplevel="true">
		<subcategory name="Art" replace="Konst" />
		<subcategory name="Construction" replace="Konstruering" />
		<subcategory name="Music" replace="Musik" />
		<subcategory name="Languages" replace="Språk" />
		<subcategory name="Teaching" replace="Undervisning" />
	</category>
	<category name="Art" replace="Konst" />
	<category name="Construction" replace="Konstruering" />
	<category name="Music" replace="Musik" />
	<category name="Languages" replace="Språk" />
	<category name="Science" replace="Vetenskap" toplevel="true">
		<subcategory name="Astronomy" replace="Astronomi" />
		<subcategory name="Biology" replace="Biologi" />
		<subcategory name="Chemistry" replace="Kemi" />
		<subcategory name="Geology" replace="Geologi" />
		<subcategory name="Math" replace="Matematik" />
		<subcategory name="MedicalSoftware" replace="Medicin" />
		<subcategory name="Physics" replace="Fysik" />
	</category>
	<category name="Astronomy" replace="Astronomi" />
	<category name="Biology" replace="Biologi" />
	<category name="Chemistry" replace="Kemi" />
	<category name="Geology" replace="Geologi" />
	<category name="Math" replace="Matematik" />
	<category name="MedicalSoftware" replace="Medicin" />
	<category name="Physics" replace="Fysik" />
	<category name="Teaching" replace="Undervisning" />
	<category name="Amusement" replace="Spel" builtin-icon="GAME" />
	<category name="Applet" replace="Appletar" />
	<category name="Archiving" replace="Arkivering" />
	<category name="Electronics" replace="Elektronik" />
	<category name="Emulator" toplevel="true" replace="Emulatorer" builtin-icon="SYSTEM" />
	<category name="Engineering" replace="Ingenjör" />
	<category name="FileManager" replace="Filhantering" />
	<category name="Shell" replace="Skal" />
	<category name="Screensaver" replace="Skärmsläckare" />
	<category name="TerminalEmulator" replace="Terminalemulatorer" />
	<category name="TrayIcon" replace="Ikoner i notifieringsfältet" />
	<category name="System" toplevel="true" builtin-icon="SYSTEM">
		<subcategory name="FileSystem" replace="Filsystem" />
		<subcategory name="Monitor" />
		<subcategory name="Security" replace="Säkerhet" />
		<subcategory name="Accessibility" replace="Hjälpmedelsfunktioner" />
		<subcategory name="TerminalEmulator" replace="Terminalemulator" />
		<subcategory name="Shell" replace="Skal" />
	</category>
	<category name="Filesystem" replace="FIlsystem" builtin-icon="SYSTEM" />
	<category name="Monitor" replace="Monitor" builtin-icon="SYSTEM" />
	<category name="Security" replace="Säkerhet" builtin-icon="SYSTEM" />
	<category name="Utility" replace="Tillbehör" toplevel="true" builtin-icon="ACCESSORIES">
		<subcategory name="Accessibility" replace="Hjälpmedelsfunktioner" />
		<subcategory name="Calculator" replace="Miniräknare" />
		<subcategory name="Clock" replace="Klockor" />
		<subcategory name="TextEditor" replace="Textredigerare" />
		<subcategory name="Archiving" replace="Arkivering" />
		<subcategory name="TerminalEmulator" replace="Terminalemulator" />
		<subcategory name="FileManager" replace="Filhanterare" />
		<subcategory name="Shell" replace="Skal" />
	</category>
	<category name="Accessibility" replace="Hjälpmedelsfunktioner" builtin-icon="UTILITY" />
	<category name="Calculator" replace="Miniräknare" builtin-icon="ACCESSORIES" />
	<category name="Clock" replace="Klockor" builtin-icon="ACCESSORIES" />
	<category name="TextEditor" replace="Textredigerare" builtin-icon="ACCESSORIES" />
	<category name="KDE" toplevel="true" ignore="true">
		<subcategory name="Development" replace="Utveckling" />
		<subcategory name="Office" replace="Kontor" />
		<subcategory name="Graphics" replace="Grafik" />
		<subcategory name="Settings" replace="Inställningar" />
		<subcategory name="Network" replace="Nätverk" />
		<subcategory name="AudioVideo" replace="Multimedia" />
		<subcategory name="Game" replace="Spel" />
		<subcategory name="Education" replace="Utbildning" />
		<subcategory name="Science" replace="Vetenskap" />
		<subcategory name="System" replace="System" />
		<subcategory name="Utility" replace="Tillbehör" />
	</category>
	<category name="GNOME" toplevel="true" ignore="true">
		<subcategory name="Development" replace="Utveckling" />
		<subcategory name="Office" replace="Kontor" />
		<subcategory name="Graphics" replace="Grafik" />
		<subcategory name="Settings" replace="Inställningar" />
		<subcategory name="Network" replace="Nätverk" />
		<subcategory name="AudioVideo" replace="Multimedia" />
		<subcategory name="Game" replace="Spel" />
		<subcategory name="Education" replace="Utbildning" />
		<subcategory name="Science" replace="Vetenskap" />
		<subcategory name="System" replace="System" />
		<subcategory name="Utility" replace="Tillbehör" />
	</category>
	<category name="GTK" toplevel="true" ignore="true">
		<subcategory name="GNOME" />
	</category>
	<category name="Qt" toplevel="true" ignore="true">
		<subcategory name="KDE" />
	</category>
	<category name="Motif" toplevel="true" ignore="true">
		<subcategory name="Development" replace="Utveckling" />
		<subcategory name="Office" replace="Kontor" />
		<subcategory name="Graphics" replace="Grafik" />
		<subcategory name="Settings"  replace="Inställningar" />
		<subcategory name="Network" replace="Nätverk" />
		<subcategory name="AudioVideo" replace="Multimedia" />
		<subcategory name="Game" replace="Spel" />
		<subcategory name="Education" replace="Utbildning" />
		<subcategory name="Science" replace="Vetenskap" />
		<subcategory name="System" replace="System" />
		<subcategory name="Utility" replace="Tillbehör" />
	</category>
	<category name="Java" toplevel="true" ignore="true">
		<subcategory name="Applet" />
	</category>
	<category name="ConsoleOnly" toplevel="true" replace="Konsol" ignore="true">
		<subcategory name="Development" replace="Utveckling" />
		<subcategory name="Office" replace="Kontor" />
		<subcategory name="Graphics" replace="Grafik" />
		<subcategory name="Settings" replace="Inställningar" />
		<subcategory name="Network" replace="Nätverk" />
		<subcategory name="AudioVideo" replace="Multimedia" />
		<subcategory name="Game" replace="Spel" />
		<subcategory name="Education" replace="Utbildning" />
		<subcategory name="Science" replace="Vetenskap" />
		<subcategory name="System" replace="System" />
		<subcategory name="Utility" replace="Tillbehör" />
	</category>
	<category name="Wine" toplevel="true" builtin-icon="WINE" />
	<category name="WineX" replace="Wine" toplevel="true" builtin-icon="WINE" />
	<category name="CrossOver" replace="Wine" toplevel="true" builtin-icon="WINE" />
</xfce-registered-categories>
