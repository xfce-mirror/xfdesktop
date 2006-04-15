<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE xfce-registered-categories>

<!-- Note: You can copy this file to ~/config/.xfce4/desktop/ for customisation. -->

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
	<category name="Legacy" toplevel="true" replace="Accessories" builtin-icon="UTILITY" />
	<category name="Core" toplevel="true" replace="Accessories" builtin-icon="UTILITY" />
	<category name="Development" toplevel="true" replace="Разработка" builtin-icon="DEVELOPMENT">
		<subcategory name="Building"/>
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
	<category name="Building" replace="Сборка" builtin-icon="DEVELOPMENT" />
	<category name="Debugger" replace="Отладчик" builtin-icon="DEVELOPMENT" />
	<category name="IDE" replace="Среды разработки" builtin-icon="DEVELOPMENT" />
	<category name="GUIDesigner" replace="Дизайн интерфейса" builtin-icon="DEVELOPMENT" />
	<category name="Profiling" builtin-icon="DEVELOPMENT" />
	<category name="RevisionControl" replace="Система контроля версий" builtin-icon="DEVELOPMENT" />
	<category name="Translation" builtin-icon="DEVELOPMENT" />
	<category name="Office" toplevel="true" replace="Офис" builtin-icon="PRODUCTIVITY">
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
	<category name="Calendar" replace="Календарь" builtin-icon="PRODUCTIVITY" />
	<category name="ContactManagement" replace="Управление контактами" builtin-icon="PRODUCTIVITY" />
	<category name="Database" replace="База данных" builtin-icon="PRODUCTIVITY" />
	<category name="Dictionary" replace="Словарь" builtin-icon="PRODUCTIVITY" />
	<category name="Chart" replace="Чертежи" builtin-icon="PRODUCTIVITY" />
	<category name="Email" replace="Электронная почта" builtin-icon="PRODUCTIVITY" />
	<category name="Finance" replace="Финансы" builtin-icon="PRODUCTIVITY" />
	<category name="FlowChart" replace="Диаграммы" builtin-icon="PRODUCTIVITY" />
	<category name="PDA" replace="КПК" builtin-icon="PRODUCTIVITY" />
	<category name="ProjectManagement" replace="Управление проектами" builtin-icon="PRODUCTIVITY" />
	<category name="Presentation" replace="Презентации" builtin-icon="PRODUCTIVITY" />
	<category name="Spreadsheet" replace="Электронные таблицы" builtin-icon="PRODUCTIVITY" />
	<category name="WordProcessor" replace="Текстовый процессор" builtin-icon="PRODUCTIVITY" />
	<category name="Graphics" replace="Графика" toplevel="true" builtin-icon="GRAPHICS">
		<subcategory name="2DGraphics" />
		<subcategory name="3DGraphics" />
		<subcategory name="Scanning" />
		<subcategory name="Photograph" />
		<subcategory name="Viewer" />
	</category>
	<category name="2DGraphics" replace="Двухмерная графика" builtin-icon="GRAPHICS">
		<subcategory name="VectorGraphics" />
		<subcategory name="RasterGraphics" />
	</category>
	<category name="VectorGraphics" replace="Векторная графика" builtin-icon="GRAPHICS" />
	<category name="RasterGraphics" replace="Растровая графика" builtin-icon="GRAPHICS" />
	<category name="3DGraphics" replace="Трехмерная графика" builtin-icon="GRAPHICS" />
	<category name="Scanning" replace="Сканирование" builtin-icon="GRAPHICS">
		<subcategory name="OCR" />
	</category>
	<category name="OCR" replace="Распознавание текста" builtin-icon="GRAPHICS" />
	<category name="Photograph" replace="Фото" builtin-icon="GRAPHICS" />
	<category name="Viewer" replace="Просмотр изображений" builtin-icon="GRAPHICS" />
	<category name="Settings" replace="Настройки" toplevel="true" builtin-icon="SETTINGS">
		<subcategory name="DesktopSettings" />
		<subcategory name="HardwareSettings" />
		<subcategory name="PackageSettings" />
		<subcategory name="Security" />
		<subcategory name="Accessibility" />
		<subcategory name="Screensaver" />
	</category>
	<category name="DesktopSettings" replace="Настройки рабочего стола" builtin-icon="SETTINGS" />
	<category name="HardwareSettings" replace="Настройки оборудования" builtin-icon="SETTINGS" />
	<category name="PackageSettings" replace="Управление пакетами" builtin-icon="SETTINGS" />
	<category name="Network" toplevel="true" replace="Сеть" builtin-icon="NETWORK">
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
	<category name="Dialup" replace="Дозвон по модему" builtin-icon="NETWORK" />
	<category name="InstantMessaging" replace="Обмен сообщениями" builtin-icon="NETWORK" />
	<category name="IRCClient" replace="Клиенты IRC" builtin-icon="NETWORK" />
	<category name="FileTransfer" replace="Передача файлов" builtin-icon="NETWORK" />
	<category name="HamRadio" replace="Любительское радио" builtin-icon="NETWORK" />
	<category name="News" replace="Новости" builtin-icon="NETWORK" />
	<category name="P2P" replace="Файлообменные сети" builtin-icon="NETWORK" />
	<category name="RemoteAccess" replace="Удалённый доступ" builtin-icon="NETWORK" />
	<category name="Telephony" replace="Телефония" builtin-icon="NETWORK" />
	<category name="WebBrowser" replace="Просмотр Веб" builtin-icon="NETWORK" />
	<category name="WebDevelopment" replace="Разработка веб-страниц" builtin-icon="NETWORK" />
	<category name="AudioVideo" replace="Мультимедиа" toplevel="true" builtin-icon="MULTIMEDIA">
		<subcategory name="Database" />
		<subcategory name="HamRadio" />
		<subcategory name="Audio" />
		<subcategory name="Video" />
		<subcategory name="AudioVideoEditing" />
		<subcategory name="Player" />
		<subcategory name="Recorder" />
		<subcategory name="DiscBurning" />
	</category>
	<category name="Audio" replace="Аудио" builtin-icon="MULTIMEDIA">
		<subcategory name="Midi" />
		<subcategory name="Mixer" />
		<subcategory name="Sequencer" />
		<subcategory name="Tuner" />
		<subcategory name="AudioVideoEditing" />
		<subcategory name="Player" />
		<subcategory name="Recorder" />
	</category>
	<category name="Midi" replace="Midi" builtin-icon="MULTIMEDIA" />
	<category name="Mixer" replace="Микшеры" builtin-icon="MULTIMEDIA" />
	<category name="Sequencer" replace="Секвенсеры" builtin-icon="MULTIMEDIA" />
	<category name="Tuner" replace="Тюнеры" builtin-icon="MULTIMEDIA" />
	<category name="Video" replace="Видео" builtin-icon="MULTIMEDIA">
		<subcategory name="TV" />
		<subcategory name="AudioVideoEditing" />
		<subcategory name="Player" />
		<subcategory name="Recorder" />
		<subcategory name="Music" />
	</category>
	<category name="TV" replace="ТВ" builtin-icon="MULTIMEDIA" />
	<category name="AudioVideoEditing" replace="Редактирование" builtin-icon="MULTIMEDIA" />
	<category name="Player" replace="Проигрыватели" builtin-icon="MULTIMEDIA" />
	<category name="Recorder" replace="Запись" builtin-icon="MULTIMEDIA" />
	<category name="DiscBurning" replace="Запись дисков CD и DVD" builtin-icon="MULTIMEDIA" />
	<category name="Game" toplevel="true" replace="Игры" builtin-icon="GAME">
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
	<category name="ActionGame" replace="Экшен" builtin-icon="GAME" />
	<category name="AdventureGame" replace="Приключения" builtin-icon="GAME" />
	<category name="ArcadeGame" replace="Аркадные" builtin-icon="GAME" />
	<category name="BoardGame" replace="Настольные" builtin-icon="GAME" />
	<category name="BlocksGame" replace="Головоломки" builtin-icon="GAME" />
	<category name="CardGame" replace="Карточные" builtin-icon="GAME" />
	<category name="KidsGame" replace="Для детей" builtin-icon="GAME" />
	<category name="LogicGame" replace="Логические" builtin-icon="GAME" />
	<category name="RolePlaying" replace="Ролевые" builtin-icon="GAME" />
	<category name="Simulation" replace="Симуляторы" builtin-icon="GAME" />
	<category name="SportsGame" replace="Спортивные" builtin-icon="GAME" />
	<category name="StrategyGame" replace="Стратегии" builtin-icon="GAME" />
	<category name="Education" replace="Обучающие" toplevel="true">
		<subcategory name="Art" />
		<subcategory name="Construction" />
		<subcategory name="Music" />
		<subcategory name="Languages" />
		<subcategory name="Teaching" />
	</category>
	<category name="Art" replace="Искусство" />
	<category name="Construction" replace="Конструкторы" />
	<category name="Music" replace="Музыка" />
	<category name="Languages" replace="Языки" />
	<category name="Science" replace="Науки" toplevel="true">
		<subcategory name="Astronomy" />
		<subcategory name="Biology" />
		<subcategory name="Chemistry" />
		<subcategory name="Geology" />
		<subcategory name="Math" />
		<subcategory name="MedicalSoftware" />
		<subcategory name="Physics" />
	</category>
	<category name="Astronomy" replace="Астрономия" />
	<category name="Biology" replace="Биология" />
	<category name="Chemistry" replace="Химия" />
	<category name="Geology" replace="Геология" />
	<category name="Math" replace="Математика" />
	<category name="MedicalSoftware" replace="Медицина" />
	<category name="Physics" replace="Физика" />
	<category name="Teaching" replace="Педагогика" />
	<category name="Amusement" replace="Игры" builtin-icon="GAME" />
	<category name="Applet" replace="Апплеты" />
	<category name="Archiving" replace="Архивация" />
	<category name="Electronics" replace="Электроника" />
	<category name="Emulator" toplevel="true" replace="Система" builtin-icon="SYSTEM" />
	<category name="Engineering" replace="Машиностроение" />
	<category name="FileManager" replace="Управление файлами" />
	<category name="Shell" replace="Shell-оболочки" />
	<category name="Screensaver" replace="Хранители экрана" />
	<category name="TerminalEmulator" replace="Эмуляторы терминала" />
	<category name="TrayIcon" replace="Значки в системном лотке" />
	<category name="System" toplevel="true" replace="Система" builtin-icon="SYSTEM">
		<subcategory name="FileSystem" />
		<subcategory name="Monitor" />
		<subcategory name="Security" />
		<subcategory name="Accessibility" />
		<subcategory name="TerminalEmulator" />
		<subcategory name="Shell" />
	</category>
	<category name="Filesystem" replace="Файловая система" builtin-icon="SYSTEM" />
	<category name="Monitor" replace="Монитор" builtin-icon="SYSTEM" />
	<category name="Security" replace="Безопасность" builtin-icon="SYSTEM" />
	<category name="Utility" toplevel="true" replace="Утилиты" builtin-icon="UTILITY">
		<subcategory name="Accessibility" />
		<subcategory name="Calculator" />
		<subcategory name="Clock" />
		<subcategory name="TextEditor" />
		<subcategory name="Archiving" />
		<subcategory name="TerminalEmulator" />
		<subcategory name="FileManager" />
		<subcategory name="Shell" />
	</category>
	<category name="Accessibility" replace="Доступность" builtin-icon="UTILITY" />
	<category name="Calculator" replace="Калькуляторы" builtin-icon="UTILITY" />
	<category name="Clock" replace="Часы" builtin-icon="UTILITY" />
	<category name="TextEditor" replace="Редактирование текста" builtin-icon="UTILITY" />
	<category name="KDE" toplevel="true" replace="KDE" ignore="true">
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
	<category name="GNOME" toplevel="true" replace="GNOME" ignore="true">
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
	<category name="GTK" toplevel="true" replace="GTK" ignore="true">
		<subcategory name="GNOME" />
	</category>
	<category name="Qt" toplevel="true" replace="Qt" ignore="true">
		<subcategory name="KDE" />
	</category>
	<category name="Motif" toplevel="true" replace="Motif" ignore="true">
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
	<category name="Java" toplevel="true" replace="Java" ignore="true">
		<subcategory name="Applet" />
	</category>
	<category name="ConsoleOnly" toplevel="true" replace="Консольные приложения" ignore="true">
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
	<category name="Wine" toplevel="Wine" builtin-icon="WINE" />
	<category name="WineX" replace="WineX" toplevel="true" builtin-icon="WINE" />
	<category name="CrossOver" replace="CrossOver" toplevel="true" builtin-icon="WINE" />
</xfce-registered-categories>
