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
	<category name="Legacy" toplevel="true" builtin-icon="UTILITY" />
	<category name="Core" toplevel="true" builtin-icon="UTILITY" />
	<category name="Development" toplevel="true" replace="Vývoj" builtin-icon="DEVELOPMENT">
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
	<category name="RevisionControl" replace="Revizní Kontrola" builtin-icon="DEVELOPMENT" />
	<category name="Translation" replace="Překlad" builtin-icon="DEVELOPMENT" />
	<category name="Office" toplevel="true" replace="Kancelář" builtin-icon="OFFICE">
		<subcategory name="Calendar" />
		<subcategory name="ContactManagement" />
		<subcategory name="Database" />
		<subcategory name="Dictionary" />
		<subcategory name="Chart" />
		<subcategory name="Email" />
		<subcategory name="Finance" />
		<subcategory name="FlowChart" />
		<subcategory name="PDA" />
		<subcategory name="ProjectManagement" />
		<subcategory name="Presentation" />
		<subcategory name="Spreadsheet" />
		<subcategory name="WordProcessor" />
		<subcategory name="Photograph" />
		<subcategory name="Viewer" />
	</category>
	<category name="Calendar" replace="Kalendář" builtin-icon="OFFICE" />
	<category name="ContactManagement" replace="Správce adres" builtin-icon="OFFICE" />
	<category name="Database" replace="Databáze" builtin-icon="OFFICE" />
	<category name="Dictionary" replace="Slovníky" builtin-icon="OFFICE" />
	<category name="Chart" builtin-icon="OFFICE" />
	<category name="Email" replace="Pošta" builtin-icon="OFFICE" />
	<category name="Finance" replace="Finance" builtin-icon="OFFICE" />
	<category name="FlowChart" replace="Flow Chart" builtin-icon="OFFICE" />
	<category name="PDA" builtin-icon="OFFICE" />
	<category name="ProjectManagement" replace="Správa projektů" builtin-icon="OFFICE" />
	<category name="Presentation" replace="Prezentace" builtin-icon="OFFICE" />
	<category name="Spreadsheet" replace="Tabulkové procesory" builtin-icon="OFFICE" />
	<category name="WordProcessor" replace="Textové procesory" builtin-icon="OFFICE" />
	<category name="Graphics" toplevel="true" replace="Grafika" builtin-icon="GRAPHICS">
		<subcategory name="2DGraphics" />
		<subcategory name="3DGraphics" />
		<subcategory name="Scanning" />
		<subcategory name="Photograph" />
		<subcategory name="Viewer" />
	</category>
	<category name="2DGraphics" replace="2D grafika" builtin-icon="GRAPHICS">
		<subcategory name="VectorGraphics" />
		<subcategory name="RasterGraphics" />
	</category>
	<category name="VectorGraphics" replace="Vektorová grafika" builtin-icon="GRAPHICS" />
	<category name="RasterGraphics" replace="Rastrová grafika" builtin-icon="GRAPHICS" />
	<category name="3DGraphics" replace="3D grafika" builtin-icon="GRAPHICS" />
	<category name="Scanning" replace="Skenování" builtin-icon="GRAPHICS">
		<subcategory name="OCR" />
	</category>
	<category name="OCR" builtin-icon="GRAPHICS" />
	<category name="Photograph" replace="Fotografie" builtin-icon="GRAPHICS" />
	<category name="Viewer" replace="Prohlížeče" builtin-icon="GRAPHICS" />
	<category name="Settings" toplevel="true" replace="Nastavení" builtin-icon="SETTINGS">
		<subcategory name="DesktopSettings" />
		<subcategory name="HardwareSettings" />
		<subcategory name="PackageSettings" />
		<subcategory name="Security" />
		<subcategory name="Accessibility" />
		<subcategory name="Screensaver" />
	</category>
	<category name="DesktopSettings" replace="Nastavení plochy" builtin-icon="SETTINGS" />
	<category name="HardwareSettings" replace="Nastavení hardware" builtin-icon="SETTINGS" />
	<category name="PackageSettings" replace="Nastavení balíčků" builtin-icon="SETTINGS" />
	<category name="Network" toplevel="true" replace="Síť" builtin-icon="NETWORK">
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
	<category name="Dialup" builtin-icon="NETWORK" />
	<category name="InstantMessaging" replace="Instant Messaging" builtin-icon="NETWORK" />
	<category name="IRCClient" replace="IRC klienti" builtin-icon="NETWORK" />
	<category name="FileTransfer" replace="Přenos souboru" builtin-icon="NETWORK" />
	<category name="HamRadio" replace="Ham Radio" builtin-icon="NETWORK" />
	<category name="News" replace="Zprávy" builtin-icon="NETWORK" />
	<category name="P2P" replace="P2P sítě" builtin-icon="NETWORK" />
	<category name="RemoteAccess" replace="Vzdálený přístup" builtin-icon="NETWORK" />
	<category name="Telephony" replace="Telefonie" builtin-icon="NETWORK" />
	<category name="WebBrowser" replace="Prohlížeče stránek" builtin-icon="NETWORK" />
	<category name="WebDevelopment" replace="Vývoj stránek" builtin-icon="NETWORK" />
	<category name="AudioVideo" replace="Multimédia" toplevel="true" builtin-icon="MULTIMEDIA">
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
	<category name="Mixer" replace="Zvukový mixér" builtin-icon="MULTIMEDIA" />
	<category name="Sequencer" replace="Sequencers" builtin-icon="MULTIMEDIA" />
	<category name="Tuner" replace="Rádio" builtin-icon="MULTIMEDIA" />
	<category name="Video" replace="Video" builtin-icon="MULTIMEDIA">
		<subcategory name="TV" />
		<subcategory name="AudioVideoEditing" />
		<subcategory name="Player" />
		<subcategory name="Recorder" />
		<subcategory name="Music" />
	</category>
	<category name="TV" replace="Televize" builtin-icon="MULTIMEDIA" />
	<category name="AudioVideoEditing" replace="Editace videa a zvuku" builtin-icon="MULTIMEDIA" />
	<category name="Player" replace="Přehrávače" builtin-icon="MULTIMEDIA" />
	<category name="Recorder" replace="Nahrávání" builtin-icon="MULTIMEDIA" />
	<category name="DiscBurning" replace="Vypalování CD a DVD" builtin-icon="MULTIMEDIA" />
	<category name="Game" toplevel="true" replace="Hry" builtin-icon="GAME">
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
	<category name="ActionGame" replace="Akční hry" builtin-icon="GAME" />
	<category name="AdventureGame" replace="Dobrodružné hry" builtin-icon="GAME" />
	<category name="ArcadeGame" replace="Arkády" builtin-icon="GAME" />
	<category name="BoardGame" replace="Stolní hry" builtin-icon="GAME" />
	<category name="BlocksGame" replace="Puzzle" builtin-icon="GAME" />
	<category name="CardGame" replace="Karetní hry" builtin-icon="GAME" />
	<category name="KidsGame" replace="Hry pro děti" builtin-icon="GAME" />
	<category name="LogicGame" replace="Logické hry" builtin-icon="GAME" />
	<category name="RolePlaying" replace="Role Playing Games" builtin-icon="GAME" />
	<category name="Simulation" replace="Simulátory" builtin-icon="GAME" />
	<category name="SportsGame" replace="Sportovní hry" builtin-icon="GAME" />
	<category name="StrategyGame" replace="Strategické hry" builtin-icon="GAME" />
	<category name="Education" toplevel="true" replace="Vzdělávání">
		<subcategory name="Art" />
		<subcategory name="Construction" />
		<subcategory name="Music" />
		<subcategory name="Languages" />
		<subcategory name="Teaching" />
	</category>
	<category name="Art" replace="Umění"/>
	<category name="Construction" replace="Konstruování"/>
	<category name="Music" replace="Hudba" />
	<category name="Languages" replace="Jazyky" />
	<category name="Science" toplevel="true" replace="Věda">
		<subcategory name="Astronomy" />
		<subcategory name="Biology" />
		<subcategory name="Chemistry" />
		<subcategory name="Geology" />
		<subcategory name="Math" />
		<subcategory name="MedicalSoftware" />
		<subcategory name="Physics" />
	</category>
	<category name="Astronomy" replace="Astronomie" />
	<category name="Biology" replace="Biologie" />
	<category name="Chemistry" replace="Chemie" />
	<category name="Geology" replace="Geologie" />
	<category name="Math" replace="Matematika" />
	<category name="MedicalSoftware" replace="Medicína" />
	<category name="Physics" replace="Fyzika" />
	<category name="Teaching" replace="Vyučování" />
	<category name="Amusement" replace="Games" builtin-icon="GAME" />
	<category name="Applet" replace="Applets" />
	<category name="Archiving" replace="Archivace" builtin-icon="SYSTEM"/>
	<category name="Electronics" replace="Elektronika" builtin-icon="SYSTEM"/>
	<category name="Emulator" toplevel="true" replace="Emulátor" builtin-icon="SYSTEM" />
	<category name="Engineering" />
	<category name="FileManager" replace="Souborové manažery" />
	<category name="Shell" replace="Shell" />
	<category name="Screensaver" replace="Šetřič obrazovky" />
	<category name="TerminalEmulator" replace="Emulátory terminálu" />
	<category name="TrayIcon" replace="System Tray Icons" />
	<category name="System" toplevel="true" replace="Systém" builtin-icon="SYSTEM">
		<subcategory name="FileSystem" />
		<subcategory name="Monitor" />
		<subcategory name="Security" />
		<subcategory name="Accessibility" />
		<subcategory name="TerminalEmulator" />
		<subcategory name="Shell" />
	</category>
	<category name="Filesystem" replace="Souborový systém" builtin-icon="SYSTEM" />
	<category name="Monitor" replace="Monitorování" builtin-icon="SYSTEM" />
	<category name="Security" replace="Bezpečnost" builtin-icon="SYSTEM" />
	<category name="Utility" toplevel="true" replace="Nástroje" builtin-icon="UTILITY">
		<subcategory name="Accessibility" />
		<subcategory name="Calculator" />
		<subcategory name="Clock" />
		<subcategory name="TextEditor" />
		<subcategory name="Archiving" />
		<subcategory name="TerminalEmulator" />
		<subcategory name="FileManager" />
		<subcategory name="Shell" />
	</category>
	<category name="Accessibility" builtin-icon="UTILITY" />
	<category name="Calculator" replace="Kalkulačky" builtin-icon="UTILITY" />
	<category name="Clock" replace="Hodiny" builtin-icon="UTILITY" />
	<category name="TextEditor" replace="Textový editor" builtin-icon="UTILITY" />
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
	<category name="WineX" replace="Wine" toplevel="true" builtin-icon="WINE" />
	<category name="CrossOver" replace="Wine" toplevel="true" builtin-icon="WINE" />
</xfce-registered-categories>
