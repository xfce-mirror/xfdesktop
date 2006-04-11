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
	<category name="Legacy" toplevel="true" replace="Accesorios" builtin-icon="UTILITY" />
	<category name="Core" toplevel="true" replace="Accesorios" builtin-icon="UTILITY" />
	<category name="Development" toplevel="true" builtin-icon="DEVELOPMENT">
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
	<category name="IDE" replace="Entornos de desarrollo" builtin-icon="DEVELOPMENT" />
	<category name="GUIDesigner" replace="Diseño de interfaces gráficas" builtin-icon="DEVELOPMENT" />
	<category name="Profiling" builtin-icon="DEVELOPMENT" />
	<category name="RevisionControl" replace="Control de revisión" builtin-icon="DEVELOPMENT" />
	<category name="Translation" replace="Traducción" builtin-icon="DEVELOPMENT" />
	<category name="Office" replace="Oficina" toplevel="true" builtin-icon="PRODUCTIVITY">
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
	<category name="Calendar" replace="Calendario" builtin-icon="PRODUCTIVITY" />
	<category name="ContactManagement" replace="Administración de contactos" builtin-icon="PRODUCTIVITY" />
	<category name="Database" replace="Bases de datos" builtin-icon="PRODUCTIVITY" />
	<category name="Dictionary" replace="Diccionario" builtin-icon="PRODUCTIVITY" />
	<category name="Chart" replace="Carta" builtin-icon="PRODUCTIVITY" />
	<category name="Email" builtin-icon="PRODUCTIVITY" />
	<category name="Finance" replace="Financias" builtin-icon="PRODUCTIVITY" />
	<category name="FlowChart" replace="Organigrama" builtin-icon="PRODUCTIVITY" />
	<category name="PDA" builtin-icon="PRODUCTIVITY" />
	<category name="ProjectManagement" replace="Administración de proyectos" builtin-icon="PRODUCTIVITY" />
	<category name="Presentation" replace="Presentación" builtin-icon="PRODUCTIVITY" />
	<category name="Spreadsheet" replace="Hoja de cáculo" builtin-icon="PRODUCTIVITY" />
	<category name="WordProcessor" replace="Procesamiento de texto" builtin-icon="PRODUCTIVITY" />
	<category name="Graphics" replace="Gráficos" toplevel="true" builtin-icon="GRAPHICS">
		<subcategory name="2DGraphics" />
		<subcategory name="3DGraphics" />
		<subcategory name="Scanning" />
		<subcategory name="Photograph" />
		<subcategory name="Viewer" />
	</category>
	<category name="2DGraphics" replace="Gráficos 2-D" builtin-icon="GRAPHICS">
		<subcategory name="VectorGraphics" />
		<subcategory name="RasterGraphics" />
	</category>
	<category name="VectorGraphics" replace="Gráficos Vectoriales" builtin-icon="GRAPHICS" />
	<category name="RasterGraphics" replace="Gráficos Rasterizados" builtin-icon="GRAPHICS" />
	<category name="3DGraphics" replace="Graficos 3-D" builtin-icon="GRAPHICS" />
	<category name="Scanning" replace="Escaneado" builtin-icon="GRAPHICS">
		<subcategory name="OCR" />
	</category>
	<category name="OCR" replace="Reconociento óptico (OCR)" builtin-icon="GRAPHICS" />
	<category name="Photograph" replace="Fotografía" replace="Photography" builtin-icon="GRAPHICS" />
	<category name="Viewer" replace="Visores" builtin-icon="GRAPHICS" />
	<category name="Settings" replace="Configuración" toplevel="true" builtin-icon="SETTINGS">
		<subcategory name="DesktopSettings" />
		<subcategory name="HardwareSettings" />
		<subcategory name="PackageSettings" />
		<subcategory name="Security" />
		<subcategory name="Accessibility" />
		<subcategory name="Screensaver" />
	</category>
	<category name="DesktopSettings" replace="Configuración de escritorio" replace="Desktop Settings" builtin-icon="SETTINGS" />
	<category name="HardwareSettings" replace="Configuración de hardware" builtin-icon="SETTINGS" />
	<category name="PackageSettings" replace="Configuración de paquetes" builtin-icon="SETTINGS" />
	<category name="Network" replace="Internet" toplevel="true" builtin-icon="NETWORK">
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
	<category name="Dialup" replace="Módem" builtin-icon="NETWORK" />
	<category name="InstantMessaging" replace="Mensajería instantánea" builtin-icon="NETWORK" />
	<category name="IRCClient" replace="Clientes IRC" builtin-icon="NETWORK" />
	<category name="FileTransfer" replace="Transferencia de archivos" builtin-icon="NETWORK" />
	<category name="HamRadio" replace="Radio Ham" builtin-icon="NETWORK" />
	<category name="News" replace="Noticias" builtin-icon="NETWORK" />
	<category name="P2P" replace="Peer-to-Peer" builtin-icon="NETWORK" />
	<category name="RemoteAccess" replace="Acceso remoto" builtin-icon="NETWORK" />
	<category name="Telephony" replace="Telefonía" builtin-icon="NETWORK" />
	<category name="WebBrowser" replace="Navegadores web" builtin-icon="NETWORK" />
	<category name="WebDevelopment" replace="Desarrollo web" builtin-icon="NETWORK" /><!-- Put this in DEVELOPMENT-->
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
	<category name="Mixer" replace="Mezcladoes" builtin-icon="MULTIMEDIA" />
	<category name="Sequencer" replace="Secuenciadores" builtin-icon="MULTIMEDIA" />
	<category name="Tuner" replace="Sintonizadores" builtin-icon="MULTIMEDIA" />
	<category name="Video" builtin-icon="MULTIMEDIA">
		<subcategory name="TV" />
		<subcategory name="AudioVideoEditing" />
		<subcategory name="Player" />
		<subcategory name="Recorder" />
		<subcategory name="Music" />
	</category>
	<category name="TV" builtin-icon="MULTIMEDIA" />
	<category name="AudioVideoEditing" replace="Edición" builtin-icon="MULTIMEDIA" />
	<category name="Player" replace="Reproductores multimedia" builtin-icon="MULTIMEDIA" />
	<category name="Recorder" replace="Grabación" builtin-icon="MULTIMEDIA" />
	<category name="DiscBurning" replace="Grabación de CDs y DVDs" builtin-icon="MULTIMEDIA" />
	<category name="Game" toplevel="true" replace="Juegos" builtin-icon="GAME">
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
	<category name="ActionGame" replace="Acción" builtin-icon="GAME" />
	<category name="AdventureGame" replace="Aventura" builtin-icon="GAME" />
	<category name="ArcadeGame" replace="Arcade" builtin-icon="GAME" />
	<category name="BoardGame" replace="Tablero" builtin-icon="GAME" />
	<category name="BlocksGame" replace="Puzles" builtin-icon="GAME" />
	<category name="CardGame" replace="Cartas" builtin-icon="GAME" />
	<category name="KidsGame" replace="Infantiles" builtin-icon="GAME" />
	<category name="LogicGame" replace="Loógicos" builtin-icon="GAME" />
	<category name="RolePlaying" replace="Juegos de rol" builtin-icon="GAME" />
	<category name="Simulation" replace="Simulación" builtin-icon="GAME" />
	<category name="SportsGame" replace="Deportes" builtin-icon="GAME" />
	<category name="StrategyGame" replace="Estrategia" builtin-icon="GAME" />
	<category name="Education" replace="Emulación" toplevel="true">
		<subcategory name="Art" />
		<subcategory name="Construction" />
		<subcategory name="Music" />
		<subcategory name="Languages" />
		<subcategory name="Teaching" />
	</category>
	<category name="Art" replace="Arte" />
	<category name="Construction" replace="Construcción" />
	<category name="Music" replace="Música" />
	<category name="Languages" replace="Idiomas" />
	<category name="Science" replace="Ciencia" toplevel="true">
		<subcategory name="Astronomy" />
		<subcategory name="Biology" />
		<subcategory name="Chemistry" />
		<subcategory name="Geology" />
		<subcategory name="Math" />
		<subcategory name="MedicalSoftware" />
		<subcategory name="Physics" />
	</category>
	<category name="Astronomy" replace="Astronomía" />
	<category name="Biology" replace="Biología" />
	<category name="Chemistry" replace="Química" />
	<category name="Geology" replace="Geología" />
	<category name="Math" replace="Matemáticas" />
	<category name="MedicalSoftware" replace="Medicina" />
	<category name="Physics" replace="Física" />
	<category name="Teaching" replace="Enseñanza" />
	<category name="Amusement" replace="Diversión" builtin-icon="GAME" />
	<category name="Applet" replace="Applets" />
	<category name="Archiving" />
	<category name="Electronics" replace="Electrónica" />
	<category name="Emulator" toplevel="true" replace="Emulación" builtin-icon="SYSTEM" />
	<category name="Engineering" replace="Ingeniería" />
	<category name="FileManager" replace="Gestor de archivos" />
	<category name="Shell" replace="Shells" />
	<category name="Screensaver" replace="Protectores de pantalla" />
	<category name="TerminalEmulator" replace="Terminal Emulators" />
	<category name="TrayIcon" replace="Iconos de la bandeja del sistema" />
	<category name="System" replace="Sistema" toplevel="true" builtin-icon="SYSTEM">
		<subcategory name="FileSystem" />
		<subcategory name="Monitor" />
		<subcategory name="Security" />
		<subcategory name="Accessibility" />
		<subcategory name="TerminalEmulator" />
		<subcategory name="Shell" />
	</category>
	<category name="Filesystem" replace="Sistema de archivos" builtin-icon="SYSTEM" />
	<category name="Monitor" replace="Monitor del sistema" builtin-icon="SYSTEM" />
	<category name="Security" replace="Seguridad" builtin-icon="SYSTEM" />
	<category name="Utility" toplevel="true" replace="Accesorios" builtin-icon="UTILITY">
		<subcategory name="Accessibility" />
		<subcategory name="Calculator" />
		<subcategory name="Clock" />
		<subcategory name="TextEditor" />
		<subcategory name="Archiving" />
		<subcategory name="TerminalEmulator" />
		<subcategory name="FileManager" />
		<subcategory name="Shell" />
	</category>
	<category name="Accessibility" replace="Accesibilidad" builtin-icon="UTILITY" />
	<category name="Calculator" replace="Calculadoras" builtin-icon="UTILITY" />
	<category name="Clock" replace="Relojes" builtin-icon="UTILITY" />
	<category name="TextEditor" replace="Edición de textos" builtin-icon="UTILITY" />
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
	<category name="ConsoleOnly" toplevel="true" replace="Sólo consola" ignore="true">
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
	<category name="WineX" replace="Cedega" toplevel="true" builtin-icon="WINE" />
	<category name="CrossOver" replace="CrossOver" toplevel="true" builtin-icon="WINE" />
</xfce-registered-categories>
