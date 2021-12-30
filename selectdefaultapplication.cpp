#include "selectdefaultapplication.h"
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardPaths>
#include <QTreeWidget>

SelectDefaultApplication::SelectDefaultApplication(QWidget *parent) : QWidget(parent)
{
	readCurrentDefaultMimetypes();
	for (const QString &dirPath : QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation)) {
		qDebug() << "Loading applications from" << dirPath;
		QDir applicationsDir(dirPath);

		for (const QFileInfo &file : applicationsDir.entryInfoList(QStringList("*.desktop"))) {
			loadDesktopFile(file);
		}
	}

	// Preload icons up front, so it doesn't get sluggish when selecting applications
	// supporting a lot
	const QIcon unknownIcon = QIcon::fromTheme("unknown");

	// TODO: check if QT_QPA_PLATFORMTHEME is set to plasma or sandsmark,
	// if so just use the functioning QIcon::fromTheme()
	// We do this manually because non-Plasma-platforms icon loading is extremely
	// slow (I blame GTK and its crappy icon cache)
	for (const QString &searchPath : (QIcon::themeSearchPaths() + QIcon::fallbackSearchPaths())) {
		loadIcons(searchPath + QIcon::themeName());
		loadIcons(searchPath);
	}

	// Set m_mimeTypeIcons[mimetypeName] to an appropriate icon
	for (const QHash<QString, QString> &application_associations : m_apps.values()) {
		for (const QString &mimetypeName : application_associations.keys()) {
			if (m_mimeTypeIcons.contains(mimetypeName)) {
				continue;
			}
			const QMimeType mimetype = m_mimeDb.mimeTypeForName(mimetypeName);

			QString iconName = mimetype.iconName();
			QIcon icon(m_iconPaths.value(iconName));
			if (!icon.isNull()) {
				m_mimeTypeIcons[mimetypeName] = icon;
				continue;
			}
			icon = QIcon(m_iconPaths.value(mimetype.genericIconName()));
			if (!icon.isNull()) {
				m_mimeTypeIcons[mimetypeName] = icon;
				continue;
			}
			int split = iconName.lastIndexOf('+');
			if (split != -1) {
				iconName.truncate(split);
				icon = QIcon(m_iconPaths.value(iconName));
				if (!icon.isNull()) {
					m_mimeTypeIcons[mimetypeName] = icon;
					continue;
				}
			}
			split = iconName.lastIndexOf('-');
			if (split != -1) {
				iconName.truncate(split);
				icon = QIcon(m_iconPaths.value(iconName));
				if (!icon.isNull()) {
					m_mimeTypeIcons[mimetypeName] = icon;
					continue;
				}
			}
			icon = QIcon(m_iconPaths.value(mimetype.genericIconName()));
			if (!icon.isNull()) {
				m_mimeTypeIcons[mimetypeName] = icon;
				continue;
			}

			m_mimeTypeIcons[mimetypeName] = unknownIcon;
		}
	}

	// The rest of this constructor sets up the GUI
	// Left section
	m_applicationList = new QListWidget;
	m_applicationList->setSelectionMode(QAbstractItemView::SingleSelection);
	populateApplicationList("");

	m_searchBox = new QLineEdit;
	m_searchBox->setPlaceholderText(tr("Search for Application"));

	m_groupChooser = new QPushButton;
	m_groupChooser->setText(tr("All"));

	m_mimegroupMenu = new QMenu(m_groupChooser);
	m_mimegroupMenu->addAction(tr("All"));
	QStringList sorted_mimegroups = m_mimegroups.values();
	sorted_mimegroups.sort();
	for (const QString &mimegroup : sorted_mimegroups) {
		m_mimegroupMenu->addAction(mimegroup);
	}
	m_groupChooser->setMenu(m_mimegroupMenu);

	QHBoxLayout *filterHolder = new QHBoxLayout;
	filterHolder->addWidget(m_searchBox);
	filterHolder->addWidget(m_groupChooser);

	QVBoxLayout *leftLayout = new QVBoxLayout;
	leftLayout->addLayout(filterHolder);
	leftLayout->addWidget(m_applicationList);

	// Middle section
	m_middleBanner = new QLabel(tr("Select an application to see its defaults."));

	m_mimetypeList = new QListWidget;
	m_mimetypeList->setSelectionMode(QAbstractItemView::MultiSelection);

	m_setDefaultButton = new QPushButton();
	m_setDefaultButton->setText(tr("Set as default application for these file types"));
	m_setDefaultButton->setEnabled(false);

	QVBoxLayout *middleLayout = new QVBoxLayout;
	middleLayout->addWidget(m_middleBanner);
	middleLayout->addWidget(m_mimetypeList);
	middleLayout->addWidget(m_setDefaultButton);

	// Right section
	m_rightBanner = new QLabel("");

	m_infoButton = new QToolButton();
	m_infoButton->setText("?");

	QHBoxLayout *infoHolder = new QHBoxLayout;
	infoHolder->addWidget(m_rightBanner);
	infoHolder->addWidget(m_infoButton);

	m_currentDefaultApps = new QListWidget;
	m_currentDefaultApps->setSelectionMode(QAbstractItemView::NoSelection);

	QVBoxLayout *rightLayout = new QVBoxLayout;
	rightLayout->addLayout(infoHolder);
	rightLayout->addWidget(m_currentDefaultApps);

	// Main layout and connections
	QHBoxLayout *mainLayout = new QHBoxLayout;
	setLayout(mainLayout);
	mainLayout->addLayout(leftLayout);
	mainLayout->addLayout(middleLayout);
	mainLayout->addLayout(rightLayout);

	connect(m_applicationList, &QListWidget::itemSelectionChanged, this,
		&SelectDefaultApplication::onApplicationSelected);
	connect(m_setDefaultButton, &QPushButton::clicked, this, &SelectDefaultApplication::onSetDefaultClicked);
	connect(m_infoButton, &QToolButton::clicked, this, &SelectDefaultApplication::showHelp);
	connect(m_searchBox, &QLineEdit::textChanged, this, &SelectDefaultApplication::populateApplicationList);
	connect(m_mimegroupMenu, &QMenu::triggered, this, &SelectDefaultApplication::constrictGroup);
}

SelectDefaultApplication::~SelectDefaultApplication()
{
}

/**
 *  Populates the middle and right side of the screen.
 *  Selects all the mimetypes that application can natively support for the middle, and all currently selected for right
 *  Filters mimetypes based on if they start with m_filterMimegroup
 */
void SelectDefaultApplication::onApplicationSelected()
{
	m_setDefaultButton->setEnabled(false);
	m_mimetypeList->clear();

	QList<QListWidgetItem *> selectedItems = m_applicationList->selectedItems();
	if (selectedItems.count() != 1) {
		return;
	}

	const QListWidgetItem *item = selectedItems.first();
	const QString appName = item->data(0).toString();

	// Set banners and right widget
	m_middleBanner->setText(appName + tr(" can open these filetypes:"));
	m_rightBanner->setText(tr("Configured mimetypes ") + appName + tr(" will open:"));
	m_currentDefaultApps->clear();
	for (QString &mimetype : m_defaultApps.values(appName)) {
		addToMimetypeList(m_currentDefaultApps, mimetype, false);
	}

	const QStringList officiallySupported = m_apps.value(appName).keys();

	// TODO allow the user to check different mimetype groups to see only applications that affect those groups, and here remove mimetypes not in that group
	//if (!supportedMime.startsWith(mimetypeGroup)) { continue; }

	// E. g. kwrite and kate only indicate support for "text/plain", but they're nice for things like C source files.
	QSet<QString> impliedSupported;
	for (const QString &mimetype : officiallySupported) {
		for (const QString &child : m_childMimeTypes.values(mimetype)) {
			impliedSupported.insert(child);
		}
	}

	for (const QString &mimetype : officiallySupported) {
		if (mimetype.startsWith(m_filterMimegroup)) {
			addToMimetypeList(m_mimetypeList, mimetype, true);
		}
	}
	for (const QString &mimetype : impliedSupported) {
		if (mimetype.startsWith(m_filterMimegroup)) {
			addToMimetypeList(m_mimetypeList, mimetype, false);
		}
	}

	m_setDefaultButton->setEnabled(m_mimetypeList->count() > 0);
}
void SelectDefaultApplication::addToMimetypeList(QListWidget *list, const QString &mimeDirtyName, const bool selected)
{
	// I didn't believe this was necessary, I tested, it is necessary. application/x-pkcs12 showed up here but is converted to application/pkcs12
	const QMimeType mimetype = m_mimeDb.mimeTypeForName(mimeDirtyName);
	const QString mimeName = mimetype.name();

	QString name = mimetype.filterString().trimmed();
	if (name.isEmpty()) {
		name = mimetype.comment().trimmed();
	}
	if (name.isEmpty()) {
		name = mimeName;
	} else {
		name += '\n' + mimeName;
	}
	QListWidgetItem *item = new QListWidgetItem(name);
	item->setData(Qt::UserRole, mimeName);
	item->setIcon(m_mimeTypeIcons[mimeDirtyName]);
	list->addItem(item);
	item->setSelected(selected);
}

void SelectDefaultApplication::onSetDefaultClicked()
{
	QList<QListWidgetItem *> selectedItems = m_applicationList->selectedItems();
	if (selectedItems.count() != 1) {
		return;
	}

	const QListWidgetItem *item = selectedItems.first();

	const QString application = item->data(0).toString();
	if (application.isEmpty()) {
		return;
	}

	QSet<QString> unselected;
	QSet<QString> selected;
	for (int i = 0; i < m_mimetypeList->count(); i++) {
		QListWidgetItem *item = m_mimetypeList->item(i);
		const QString name = item->data(Qt::UserRole).toString();
		if (item->isSelected()) {
			selected.insert(name);
		} else {
			unselected.insert(name);
		}
	}

	setDefault(application, selected, unselected);
}

void SelectDefaultApplication::loadDesktopFile(const QFileInfo &fileInfo)
{
	// Ugliest implementation of .desktop file reading ever

	QFile file(fileInfo.absoluteFilePath());
	if (!file.open(QIODevice::ReadOnly)) {
		qWarning() << "Error: Failed to open" << fileInfo.fileName();
		return;
	}

	// The filename of the desktop file
	const QString &appFile = fileInfo.fileName();
	// The mimetypes the application can support
	QStringList mimetypes;
	// The name of the application in its desktop entry
	// Used as the primary key with which associations are made
	QString appName;
	// The name of the icon as given in the desktop entry
	QString appIcon;

	while (!file.atEnd()) {
		// Removes all runs of whitespace, but won't make `Name=` and `Name =` the same
		QString line = file.readLine().simplified();

		if (line.startsWith('[')) {
			if (line == "[Desktop Entry]") {
				continue;
			}
			// Multiple groups may not have the same name, and [Desktop Entry] must be the first group. So we are done otherwise
			break;
		} else {
			// Trim the strings because the '=' can be padded with spaces because FreeDesktop is stupid, even though not a single desktop file on my computer uses that
			const QString key = line.section('=', 0, 0).trimmed();
			const QString value = line.section('=', 1).trimmed();

			if (key == "Name") {
				appName = value;
			} else if (key == "MimeType") {
				mimetypes = value.split(';', Qt::SkipEmptyParts);
			} else if (key == "Icon") {
				appIcon = value;
			}
			// Else ignore the key
		}
	}

	if (!appIcon.isEmpty() && m_applicationIcons[appFile].isEmpty()) {
		m_applicationIcons[appName] = appIcon;
	}
	// Even if mimetypes is empty, set the icon in case a different one isn't
	if (mimetypes.isEmpty()) {
		return;
	}

	// Note that this program is the one that can edit some files from the defaults, if it is
	if (m_defaultDesktopEntries.contains(appFile)) {
		for (QString &mimetype : m_defaultDesktopEntries.values(appFile)) {
			m_defaultApps.insert(appName, mimetype);
		}
	}

	for (const QString &readMimeName : mimetypes) {
		// Resolve aliases etc
		const QMimeType mimetype = m_mimeDb.mimeTypeForName(readMimeName.trimmed());
		if (!mimetype.isValid()) {
			// TODO This happens a TON. Why?
			//qDebug() << "In file " << appName << " mimetype " << readMimeName << " is invalid. Ignoring...";
			continue;
		}
		const QString mimetypeName = mimetype.name();

		// Create a database of mimetypes this application is a child of
		// So applications that can edit parent mimetypes can also have associations formed to their child mimetypes
		// Unless the parent is 'application/octet-stream' because I guess a lot of stuff has that as its parent
		// Example: Kate editing text/plain can edit C source code
		for (const QString &parent : mimetype.parentMimeTypes()) {
			if (parent == "application/octet-stream") {
				break;
			}
			m_childMimeTypes.insert(parent, mimetypeName);
		}

		if (mimetypeName.count('/') != 1) {
			qDebug() << "Warning: encountered mimetype " << mimetypeName
				 << " without exactly 1 '/' character in " << appFile
				 << " Unsure what to do, skipping...";
			continue;
		}
		// Now that we've checked this, we can get the mimegroup and add it to the global list
		const QString mimegroup = mimetypeName.section('/', 0, 0);
		m_mimegroups.insert(mimegroup);

		// Indexing in Qt creates a default element if one doesn't exist, so we don't need to explicitely check if m_apps[appName] exists
		// If we've already got an association for this app from a different desktop file, don't overwrite it because we read highest-priority .desktops first
		if (m_apps[appName].contains(mimetypeName)) {
			// Annoyingly, some apps like KDE mobile apps add associations for *the same exact file type* through two different aliases, so this gets spammed a lot.
			qDebug() << "Debug: " << appName << " already handles " << mimetypeName << " with "
				 << m_apps[appName][mimetypeName] << " so " << appFile << "will be ignored";
			continue;
		}
		m_apps[appName][mimetypeName] = appFile;
	}
}

void SelectDefaultApplication::setDefault(const QString &appName, const QSet<QString> &mimetypes,
					  const QSet<QString> &unselectedMimetypes)
{
	const QString filePath =
		QDir(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)).absoluteFilePath("mimeapps.list");
	QFile file(filePath);

	// Read in existing mimeapps.list, skipping the lines for the mimetypes we're updating
	QList<QByteArray> existingContent;
	QList<QByteArray> existingAssociations;
	if (file.open(QIODevice::ReadOnly)) {
		bool inCorrectGroup = false;
		while (!file.atEnd()) {
			const QByteArray line = file.readLine().trimmed();

			if (line.isEmpty()) {
				continue;
			}

			if (line.startsWith('[')) {
				inCorrectGroup = (line == "[Default Applications]");
				if (!inCorrectGroup) {
					existingContent.append(line);
				}
				continue;
			}

			if (!inCorrectGroup) {
				existingContent.append(line);
				continue;
			}

			if (!line.contains('=')) {
				existingAssociations.append(line);
				continue;
			}

			const QString mimetype = m_mimeDb.mimeTypeForName(line.split('=').first().trimmed()).name();
			if (!mimetypes.contains(mimetype) && !unselectedMimetypes.contains(mimetype)) {
				existingAssociations.append(line);
			}

			// Ensure that if a mimetype is unselected but set as default for a different application, we don't remove its entry from configuration
			if (unselectedMimetypes.contains(mimetype)) {
				const QString handlingAppFile = line.split('=')[1];
				const QString appFile = m_apps[appName][mimetype];
				if (appFile != handlingAppFile) {
					existingAssociations.append(line);
				}
			}
		}

		file.close();
	} else {
		qWarning() << "Unable to open file for reading" << file.errorString();
		// TODO If we can't open the file for reading, we better stop before opening for writing and deleting it
	}

	if (!file.open(QIODevice::WriteOnly)) {
		QMessageBox::warning(this, tr("Failed to store settings"), file.errorString());
		return;
	}

	for (const QByteArray &line : existingContent) {
		file.write(line + '\n');
	}
	file.write("\n[Default Applications]\n");
	for (const QByteArray &line : existingAssociations) {
		file.write(line + '\n');
	}

	for (const QString &mimetype : mimetypes) {
		file.write(QString(mimetype + '=' + m_apps[appName][mimetype] + '\n').toUtf8());
	}
}

void SelectDefaultApplication::readCurrentDefaultMimetypes()
{
	const QString filePath =
		QDir(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)).absoluteFilePath("mimeapps.list");
	QFile file(filePath);

	// Read in existing mimeapps.list, skipping the lines for the mimetypes we're updating
	if (file.open(QIODevice::ReadOnly)) {
		bool inCorrectGroup = false;
		while (!file.atEnd()) {
			const QByteArray line = file.readLine().trimmed();

			if (line.isEmpty()) {
				continue;
			}

			if (line.startsWith('[')) {
				inCorrectGroup = (line == "[Default Applications]");
				continue;
			}

			if (!inCorrectGroup) {
				continue;
			}

			if (!line.contains('=')) {
				continue;
			}

			const QString mimetype = m_mimeDb.mimeTypeForName(line.split('=').first().trimmed()).name();
			const QString appFile = line.split('=')[1];
			m_defaultDesktopEntries.insert(appFile, mimetype);
		}

		file.close();
	} else {
		qWarning() << "Unable to open file for reading" << file.errorString();
		// TODO If we can't open the file for reading, we better stop before opening for writing and deleting it
	}
}

void SelectDefaultApplication::populateApplicationList(const QString &filter)
{
	// Clear the list in case we are updating it (i.e. performing a search)
	m_applicationList->clear();

	// Filter entries based on the filter string
	QStringList applications = m_apps.keys().filter(filter, Qt::CaseInsensitive);

	// Sort the remaining applications
	// TODO If this is a performance issue, we can keep a seperate array pre-sorted
	std::sort(applications.begin(), applications.end());

	// Add each application to the left panel
	for (const QString &appName : applications) {
		QListWidgetItem *app = new QListWidgetItem(appName);
		app->setIcon(QIcon::fromTheme(m_applicationIcons[appName]));
		m_applicationList->addItem(app);
		app->setSelected(false);
	}
}

void SelectDefaultApplication::loadIcons(const QString &path)
{
	QFileInfo icon_file(path);
	if (!icon_file.exists() || !icon_file.isDir()) {
		return;
	}
	// TODO: avoid hardcoding
	QStringList imageTypes({ "*.svg", "*.svgz", "*.png", "*.xpm" });
	QDirIterator iter(path, imageTypes, QDir::Files, QDirIterator::Subdirectories);

	while (iter.hasNext()) {
		iter.next();
		icon_file = iter.fileInfo();

		const QString name = icon_file.completeBaseName();
		if (m_iconPaths.contains(name)) {
			continue;
		}
		m_iconPaths[name] = icon_file.filePath();
	}
}

void SelectDefaultApplication::constrictGroup(QAction *action)
{
	m_filterMimegroup = (action->text() == tr("All")) ? "" : action->text();
	qDebug() << action->text() << "Compared with" << m_filterMimegroup;
	m_searchBox->clear();
}

void SelectDefaultApplication::showHelp()
{
	QMessageBox *dialog = new QMessageBox(this);
	dialog->setText(tr(
		"To use this program, select any applications on the left panel.\n"
		"Then select or deselect any mimetypes in the center that you want this application to open. Most of the time, you can leave this at the defaults; it will choose all the mimetypes the application has explicit support for.\n"
		"Finally, press at the bottom of the screen to make the highlighted mimetypes open with the selected application by default.\n"
		"You can see your changes on the right. Any application that you have configured will report it there.\n"
		"---------------------------------------------------------------------------------------\n"
		"Explanation of how this works: A FreeDesktop has Desktop Entries (.desktop files) in several locations; /usr/share/applications/, /usr/local/share/applications/, and ~/.local/share/applications/ by default.\n"
		"These desktop entries tell application launchers how to run programs, including the tool 'xdg-open' which is the standard tool to open files and URLs.\n"
		"xdg-open reads Desktop Entries in an unpredictable order in order to determine what application to handle that file; it uses the `MimeTypes` key present in a Desktop Entry to determine this.\n"
		"There is also a user configuration file, `~/.config/mimeapps.list`, which it reads first and gives higher precedence.\n"
		"This program parses all the application files located on the system, as well as the `mimeapps.list`, to determine what programs exist and which are set as defaults.\n"
		"Then, when you click to 'set as default for these filetypes', it reads `mimeapps.list`, and sets the keys you have highlighted to the new values.\n"));
	dialog->exec();
}