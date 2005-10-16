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
	<category name="Legacy" toplevel="true" replace="Akcesoria" builtin-icon="UTILITY" />
	<category name="Core" toplevel="true" replace="Akcesoria" builtin-icon="UTILITY" />
	<category name="Development" replace="Programowanie" toplevel="true" builtin-icon="DEVELOPMENT">
		<subcategory name="Building" replace="Budowanie" />
		<subcategory name="Debugger" />
		<subcategory name="IDE" />
		<subcategory name="GUIDesigner" replace="Tworzenie GUI" />
		<subcategory name="Profiling" replace="Profilowanie" />
		<subcategory name="RevisionControl" replace="Kontrola wersji" />
		<subcategory name="Translation" replace="Tłumaczenie" />
		<subcategory name="Database" replace="Baza danych" />
		<subcategory name="ProjectManagement" replace="Zarządzanie Projektem" />
		<subcategory name="WebDevelopment" replace="Tworzenie stron www" />
	</category>
	<category name="Building" replace="Budowanie" builtin-icon="DEVELOPMENT" />
	<category name="Debugger" replace="Debugowanie" builtin-icon="DEVELOPMENT" />
	<category name="IDE" replace="Środowiska" builtin-icon="DEVELOPMENT" />
	<category name="GUIDesigner" replace="Tworzenie GUI" builtin-icon="DEVELOPMENT" />
	<category name="Profiling" replace="Profilowanie" builtin-icon="DEVELOPMENT" />
	<category name="RevisionControl" replace="Kontrola wersji" builtin-icon="DEVELOPMENT" />
	<category name="Translation" replace="Tłumaczenie" builtin-icon="DEVELOPMENT" />
	<category name="Office" replace="Biuro" toplevel="true" builtin-icon="PRODUCTIVITY">
		<subcategory name="Calendar" replace="Kalendarz" />
		<subcategory name="ContactManagement" replace="Zarządzanie Treścią" />
		<subcategory name="Database" replace="Baza danych" />
		<subcategory name="Dictionary" replace="Słownik" />
		<subcategory name="Chart" replace="Wykres" />
		<subcategory name="Email" replace="Email" />
		<subcategory name="Finance" replace="Finanse" />
		<subcategory name="FlowChart" replace="Arkusz" />
		<subcategory name="PDA" />
		<subcategory name="Project Management" replace="Zarządzanie Projektem" />
		<subcategory name="Presentation" replace="Prezentacje" />
		<subcategory name="Spreadsheet" replace="Arkusz Kalkulacyjny" />
		<subcategory name="WordProcessor" replace="Edytor Tekstu" />
		<subcategory name="Photograph" replace="Zdjęcie" />
		<subcategory name="Viewer" replace="Przeglądarka" />
	</category>
	<category name="Calendar" replace="Kalendarz" builtin-icon="PRODUCTIVITY" />
	<category name="ContactManagement" replace="Zarządzanie treścią" builtin-icon="PRODUCTIVITY" />
	<category name="Database" replace="Baza danych" builtin-icon="PRODUCTIVITY" />
	<category name="Dictionary" replace="Słownik" builtin-icon="PRODUCTIVITY" />
	<category name="Chart" replace="Wykres" builtin-icon="PRODUCTIVITY" />
	<category name="Email" replace="Email" builtin-icon="PRODUCTIVITY" />
	<category name="Finance" replace="Finanse" builtin-icon="PRODUCTIVITY" />
	<category name="FlowChart" replace="Arkusz" builtin-icon="PRODUCTIVITY" />
	<category name="PDA" builtin-icon="PRODUCTIVITY" />
	<category name="ProjectManagement" replace="Zarządzanie Projektem" builtin-icon="PRODUCTIVITY" />
	<category name="Presentation" replace="Prezentacja" builtin-icon="PRODUCTIVITY" />
	<category name="Spreadsheet" replace="Arkusz Kalkulacyjny" builtin-icon="PRODUCTIVITY" />
	<category name="WordProcessor" replace="Edytor Tekstu" builtin-icon="PRODUCTIVITY" />
	<category name="Graphics" replace="Grafika" toplevel="true" builtin-icon="GRAPHICS">
		<subcategory name="2DGraphics" replace="Grafika 2D" />
		<subcategory name="3DGraphics" replace="Grafika 3D" />
		<subcategory name="Scanning" replace="Skanowanie" />
		<subcategory name="Photograph" replace="Zdjęcia" />
		<subcategory name="Viewer" replace="Przeglądarki" />
	</category>
	<category name="2DGraphics" replace="Grafika 2D" builtin-icon="GRAPHICS">
		<subcategory name="VectorGraphics" replace="Grafika Wektorowa" />
		<subcategory name="RasterGraphics" replace="Grafika Rastrowa" />
	</category>
	<category name="VectorGraphics" replace="Grafika wektorowa" builtin-icon="GRAPHICS" />
	<category name="RasterGraphics" replace="Grafika rastrowa" builtin-icon="GRAPHICS" />
	<category name="3DGraphics" replace="Grafika 3D" builtin-icon="GRAPHICS" />
	<category name="Scanning" replace="Skanowanie" builtin-icon="GRAPHICS">
		<subcategory name="OCR" />
	</category>
	<category name="OCR" builtin-icon="GRAPHICS" />
	<category name="Photograph" replace="Zdjęcia" builtin-icon="GRAPHICS" />
	<category name="Viewer" replace="Przeglądarki" builtin-icon="GRAPHICS" />
	<category name="Settings" replace="Ustawienia" toplevel="true" builtin-icon="SETTINGS">
		<subcategory name="DesktopSettings" replace="Ustawienia pulpitu" />
		<subcategory name="HardwareSettings" replace="Ustawienia sprzętowe" />
		<subcategory name="PackageSettings" replace="Ustawienia pakietów" />
		<subcategory name="Security" replace="Bezpieczeństwo" />
		<subcategory name="Accessibility" replace="Dostępność" />
		<subcategory name="Screensaver" replace="Wygaszacz Ekranu" />
	</category>
	<category name="DesktopSettings" replace="Ustawienia pulpitu" builtin-icon="SETTINGS" />
	<category name="HardwareSettings" replace="Ustawienia sprzętowe" builtin-icon="SETTINGS" />
	<category name="PackageSettings" replace="Ustawienia pakietów" builtin-icon="SETTINGS" />
	<category name="Network" toplevel="true" replace="Sieć" builtin-icon="NETWORK">
		<subcategory name="Email" />
		<subcategory name="Dialup" />
		<subcategory name="InstantMessaging" replace="Komunikatory" />
		<subcategory name="IRCClient" replace="Klienty IRC" />
		<subcategory name="FileTransfer" replace="Wymiana plików" />
		<subcategory name="HamRadio" />
		<subcategory name="News" replace="Wiadomości" />
		<subcategory name="P2P" />
		<subcategory name="RemoteAccess" replace="Zdalny dostęp" />
		<subcategory name="Telephony" replace="Telefony" />
		<subcategory name="WebBrowser" replace="Przeglądarki www" />
		<subcategory name="WebDevelopment" replace="Tworzenie stron www" />
	</category>
	<category name="Dialup" builtin-icon="NETWORK" />
	<category name="InstantMessaging" replace="Komunikatory" builtin-icon="NETWORK" />
	<category name="IRCClient" replace="Klienty IRC" builtin-icon="NETWORK" />
	<category name="FileTransfer" replace="Wymiana plików" builtin-icon="NETWORK" />
	<category name="HamRadio" replace="Ham Radio" builtin-icon="NETWORK" />
	<category name="News" replace="Wiadomości" builtin-icon="NETWORK" />
	<category name="P2P" replace="Peer-to-Peer" builtin-icon="NETWORK" />
	<category name="RemoteAccess" replace="Zdalny dostęp" builtin-icon="NETWORK" />
	<category name="Telephony" replace="Telefony" builtin-icon="NETWORK" />
	<category name="WebBrowser" replace="Przeglądarki www" builtin-icon="NETWORK" />
	<category name="WebDevelopment" replace="Tworzenie stron www" builtin-icon="NETWORK" />
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
	<category name="Mixer" replace="Mikser Dźwięku" builtin-icon="MULTIMEDIA" />
	<category name="Sequencer" replace="Sekwencery" builtin-icon="MULTIMEDIA" />
	<category name="Tuner" replace="Tunery" builtin-icon="MULTIMEDIA" />
	<category name="Video" builtin-icon="MULTIMEDIA">
		<subcategory name="TV" />
		<subcategory name="AudioVideoEditing" />
		<subcategory name="Player" />
		<subcategory name="Recorder" />
		<subcategory name="Music" />
	</category>
	<category name="TV" builtin-icon="MULTIMEDIA" />
	<category name="AudioVideoEditing" replace="Edycja" builtin-icon="MULTIMEDIA" />
	<category name="Player" replace="Odtwarzacze" builtin-icon="MULTIMEDIA" />
	<category name="Recorder" replace="Nagrywanie" builtin-icon="MULTIMEDIA" />
	<category name="DiscBurning" replace="Nagrywanie CD i DVD" builtin-icon="MULTIMEDIA" />
	<category name="Game" toplevel="true" replace="Gry" builtin-icon="GAME">
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
	<category name="ActionGame" replace="Akcja" builtin-icon="GAME" />
	<category name="AdventureGame" replace="Przygodówki" builtin-icon="GAME" />
	<category name="ArcadeGame" replace="Arcade" builtin-icon="GAME" />
	<category name="BoardGame" replace="Planszowe" builtin-icon="GAME" />
	<category name="BlocksGame" replace="Puzzle" builtin-icon="GAME" />
	<category name="CardGame" replace="Karciane" builtin-icon="GAME" />
	<category name="KidsGame" replace="Dla dzieci" builtin-icon="GAME" />
	<category name="LogicGame" replace="Logiczne" builtin-icon="GAME" />
	<category name="RolePlaying" replace="RPG" builtin-icon="GAME" />
	<category name="Simulation" replace="Symulacje" builtin-icon="GAME" />
	<category name="SportsGame" replace="Sportowe" builtin-icon="GAME" />
	<category name="StrategyGame" replace="Strategiczne" builtin-icon="GAME" />
	<category name="Education" replace="Edukacja" toplevel="true">
		<subcategory name="Art" />
		<subcategory name="Construction" />
		<subcategory name="Music" />
		<subcategory name="Languages" />
		<subcategory name="Teaching" />
	</category>
	<category name="Art" replace="Sztuka" />
	<category name="Construction" replace="Konstrukcje" />
	<category name="Music" replace="Muzyka" />
	<category name="Languages" replace="Języki" />
	<category name="Science" replace="Nauka" toplevel="true">
		<subcategory name="Astronomy" />
		<subcategory name="Biology" />
		<subcategory name="Chemistry" />
		<subcategory name="Geology" />
		<subcategory name="Math" />
		<subcategory name="MedicalSoftware" />
		<subcategory name="Physics" />
	</category>
	<category name="Astronomy" replace="Astronomia" />
	<category name="Biology" replace="Biologia" />
	<category name="Chemistry" replace="Chemia" />
	<category name="Geology" replace="Geologia" />
	<category name="Math" replace="Matematyka" />
	<category name="MedicalSoftware" replace="Medycyna" />
	<category name="Physics" replace="Fizyka" />
	<category name="Teaching" replace="Nauczanie" />
	<category name="Amusement" replace="Gry" builtin-icon="GAME" />
	<category name="Applet" replace="Applety" />
	<category name="Archiving" replace="Archiwizacja" />
	<category name="Electronics" replace="Elektronika" />
	<category name="Emulator" toplevel="true" replace="System" builtin-icon="SYSTEM" />
	<category name="Engineering" replace="Inżynieria" />
	<category name="FileManager" replace="Zarządzanie Plikami" />
	<category name="Shell" replace="Konsole" />
	<category name="Screensaver" replace="Wygaszacz Ekranu" />
	<category name="TerminalEmulator" replace="Terminale" />
	<category name="TrayIcon" replace="System Tray Icons" />
	<category name="System" toplevel="true" builtin-icon="SYSTEM">
		<subcategory name="FileSystem" />
		<subcategory name="Monitor" />
		<subcategory name="Security" />
		<subcategory name="Accessibility" />
		<subcategory name="TerminalEmulator" />
		<subcategory name="Shell" />
	</category>
	<category name="Filesystem" replace="System plików" builtin-icon="SYSTEM" />
	<category name="Monitor" builtin-icon="SYSTEM" />
	<category name="Security" replace="Bezpieczeństwo" builtin-icon="SYSTEM" />
	<category name="Utility" toplevel="true" replace="Akcesoria" builtin-icon="UTILITY">
		<subcategory name="Accessibility" />
		<subcategory name="Calculator" />
		<subcategory name="Clock" />
		<subcategory name="TextEditor" />
		<subcategory name="Archiving" />
		<subcategory name="TerminalEmulator" />
		<subcategory name="FileManager" />
		<subcategory name="Shell" />
	</category>
	<category name="Accessibility" replace="Dostępność" builtin-icon="UTILITY" />
	<category name="Calculator" replace="Kalkulatory" builtin-icon="UTILITY" />
	<category name="Clock" replace="Zegary" builtin-icon="UTILITY" />
	<category name="TextEditor" replace="Edycja tekstu" builtin-icon="UTILITY" />
	<category name="KDE" toplevel="true" ignore="true">
		<subcategory name="Development" replace="Programowanie" />
		<subcategory name="Office" replace="Biuro" />
		<subcategory name="Graphics" replace="Grafika" />
		<subcategory name="Settings" replace="Ustawienia" />
		<subcategory name="Network" replace="Sieć" />
		<subcategory name="AudioVideo" replace="Multimedia" />
		<subcategory name="Game" replace="Gry" />
		<subcategory name="Education" replace="Edukacja" />
		<subcategory name="Science" replace="Nauka" />
		<subcategory name="System" replace="System" />
		<subcategory name="Utility" replace="Narzędzia" />
	</category>
	<category name="GNOME" toplevel="true" ignore="true">
		<subcategory name="Development" replace="Programowanie" />
		<subcategory name="Office" replace="Biuro" />
		<subcategory name="Graphics" replace="Grafika" />
		<subcategory name="Settings" replace="Ustawienia" />
		<subcategory name="Network" replace="Sieć" />
		<subcategory name="AudioVideo" replace="Multimedia" />
		<subcategory name="Game" replace="Gry" />
		<subcategory name="Education" replace="Edukacja" />
		<subcategory name="Science" replace="Nauka" />
		<subcategory name="System" replace="System" />
		<subcategory name="Utility" replace="Narzędzia" />
	</category>
	<category name="GTK" toplevel="true" ignore="true">
		<subcategory name="GNOME" />
	</category>
	<category name="Qt" toplevel="true" ignore="true">
		<subcategory name="KDE" />
	</category>
	<category name="Motif" toplevel="true" ignore="true">
		<subcategory name="Development" replace="Programowanie" />
		<subcategory name="Office" replace="Biuro" />
		<subcategory name="Graphics" replace="Grafika" />
		<subcategory name="Settings" replace="Ustawienia" />
		<subcategory name="Network" replace="Sieć" />
		<subcategory name="AudioVideo" replace="Multimedia" />
		<subcategory name="Game" replace="Gry" />
		<subcategory name="Education" replace="Edukacja" />
		<subcategory name="Science" replace="Nauka" />
		<subcategory name="System" replace="System" />
		<subcategory name="Utility" replace="Narzędzia" />
	</category>
	<category name="Java" toplevel="true" ignore="true">
		<subcategory name="Applet" />
	</category>
	<category name="ConsoleOnly" toplevel="true" replace="Console" ignore="true">
		<subcategory name="Development" replace="Programowanie" />
		<subcategory name="Office" replace="Biuro" />
		<subcategory name="Graphics" replace="Grafika" />
		<subcategory name="Settings" replace="Ustawienia" />
		<subcategory name="Network" replace="Sieć" />
		<subcategory name="AudioVideo" replace="Multimedia" />
		<subcategory name="Game" replace="Gry" />
		<subcategory name="Education" replace="Edukacja" />
		<subcategory name="Science" replace="Nauka" />
		<subcategory name="System" replace="System" />
		<subcategory name="Utility" replace="Narzędzia" />
	</category>
	<category name="Wine" toplevel="true" builtin-icon="WINE" />
	<category name="WineX" toplevel="true" builtin-icon="WINE" />
</xfce-registered-categories>
