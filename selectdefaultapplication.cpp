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
	for (const QString &dirPath : QStandardPaths::standardLocations(
		     QStandardPaths::ApplicationsLocation)) {
		qDebug() << "Loading applications from" << dirPath;
		QDir applicationsDir(dirPath);

		for (const QFileInfo &file :
		     applicationsDir.entryInfoList(QStringList("*.desktop"))) {
			loadDesktopFile(file);
		}
	}

/*
What?
*/
	// Check that we shit with multiple .desktop files, but some nodisplay files
//	for (const QString &appId : m_supportedMimetypes.keys()) {
/*
This is impossible now that app ids don't come from random shit in their Exec key

		if (!m_desktopFileNames.contains(appId)) {
			qWarning()
				<< appId
				<< "does not have an associated desktop file!";
			continue;
		}
*/

/*
This should be impossible, but do more thinking
*/
/*
		if (m_applicationNames[appId].isEmpty()) {
			qWarning() << "Missing name" << appId;
			m_applicationNames[appId] = appId;
		}
*/
//	}

	// Preload icons up front, so it doesn't get sluggish when selecting applications
	// supporting a lot
	const QIcon unknownIcon = QIcon::fromTheme("unknown");

	// TODO: check if QT_QPA_PLATFORMTHEME is set to plasma or sandsmark,
	// if so just use the functioning QIcon::fromTheme()
	// We do this manually because non-Plasma-platforms icon loading is extremely
	// slow (I blame GTK and its crappy icon cache)
	for (const QString &searchPath :
	     (QIcon::themeSearchPaths() + QIcon::fallbackSearchPaths())) {
		loadIcons(searchPath + QIcon::themeName());
		loadIcons(searchPath);
	}

	// Set m_mimeTypeIcons[mimetypeName] to an appropriate icon
	for (const QHash<QString,QString> &application_associations : m_apps.values()) {
	for (const QString &mimetypeName : application_associations.keys()) {
		if (m_mimeTypeIcons.contains(mimetypeName)) {
			continue;
		}
		const QMimeType mimetype =
			m_mimeDb.mimeTypeForName(mimetypeName);

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

	m_applicationList = new QListWidget;
	m_applicationList->setSelectionMode(QAbstractItemView::SingleSelection);
// TODO allow user to search for applications
	populateApplicationList("");

	m_setDefaultButton = new QPushButton(
		tr("Set as default application for these file types"));
	m_setDefaultButton->setEnabled(false);

	m_mimetypeList = new QListWidget;
	m_mimetypeList->setSelectionMode(QAbstractItemView::MultiSelection);

	QGridLayout *rightLayout = new QGridLayout;
	rightLayout->addWidget(m_mimetypeList);
	rightLayout->addWidget(m_setDefaultButton);

	QHBoxLayout *mainLayout = new QHBoxLayout;
	setLayout(mainLayout);
	mainLayout->addWidget(m_applicationList);
	mainLayout->addLayout(rightLayout);

// Populate the left listlayout with applications
// Moved to up with its initialization in the refactor
/*
	QStringList types = m_applications.keys();
	std::sort(types.begin(), types.end());
	for (const QString &type : types) {
		QTreeWidgetItem *typeItem =
			new QTreeWidgetItem(QStringList(type));
		QStringList applications = m_applications[type].values();
		std::sort(applications.begin(), applications.end(),
			  [=](const QString &a, const QString &b) {
				  return m_applicationNames[a] <
					 m_applicationNames[b];
			  });
		for (const QString &application : applications) {
			QTreeWidgetItem *appItem = new QTreeWidgetItem(
				QStringList(m_applicationNames[application]));
			appItem->setData(0, Qt::UserRole, application);
			appItem->setIcon(
				0, QIcon::fromTheme(
					   m_applicationIcons[application]));
			typeItem->addChild(appItem);
		}
		m_applicationList->addTopLevelItem(typeItem);
	}
	m_applicationList->setHeaderHidden(true);
*/

	connect(m_applicationList, &QListWidget::itemSelectionChanged, this,
		&SelectDefaultApplication::onApplicationSelected);
	connect(m_setDefaultButton, &QPushButton::clicked, this,
		&SelectDefaultApplication::onSetDefaultClicked);
}

SelectDefaultApplication::~SelectDefaultApplication()
{
}

/*
Actually is called when you select an APPLICATION
Holy shit this is negative quality code

Populates the right side of the screen. Selects all the mimetypes that application can natively support
TODO distinguish between mimetypes the application currently is the default of, mimetypes the application natively supports, children of the application's supported types
Currently only distinguishes between the latter two

void SelectDefaultApplication::onMimetypeSelected()
*/
void SelectDefaultApplication::onApplicationSelected()
{
	m_setDefaultButton->setEnabled(false);
	m_mimetypeList->clear();

	QList<QListWidgetItem *> selectedItems =
		m_applicationList->selectedItems();
	if (selectedItems.count() != 1) {
		return;
	}

	const QListWidgetItem *item = selectedItems.first();

	//const QString mimetypeGroup = item->parent()->text(0);
	//const QString application = item->data(0, Qt::UserRole).toString();
	const QString application = item->data(0).toString();

	const QStringList officiallySupported =
		m_apps.value(application).keys();
	// E. g. kwrite and kate only indicate support for "text/plain", but
	// they're nice for things like c source files.
	QSet<QString> impliedSupported;
	for (const QString &mimetype : officiallySupported) {
		for (const QString &child : m_childMimeTypes.values(mimetype)) {
			impliedSupported.insert(child);
		}
	}

/*
TODO allow the user to check different mimetype groups to see only applications that affect those groups, and only associations in those groups

		if (!supportedMime.startsWith(mimetypeGroup)) {
			continue;
		}
*/
	for (const QString &mimetype : officiallySupported) {
		addToMimetypeList(mimetype, true);
	}
	for (const QString &mimetype : impliedSupported) {
		addToMimetypeList(mimetype, false);
	}

	m_setDefaultButton->setEnabled(m_mimetypeList->count() > 0);
}
void SelectDefaultApplication::addToMimetypeList(const QString &mimetypeDirtyName, const bool selected) {
		const QMimeType mimetype =
			m_mimeDb.mimeTypeForName(mimetypeDirtyName);
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
		item->setIcon(m_mimeTypeIcons[mimetypeDirtyName]);
		m_mimetypeList->addItem(item);
		item->setSelected(selected);

}

void SelectDefaultApplication::onSetDefaultClicked()
{
	QList<QListWidgetItem *> selectedItems =
		m_applicationList->selectedItems();
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
		qDebug() << "Failed to open" << fileInfo.fileName();
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

/*
Not used anymore
	bool noDisplay = false;
*/

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

/*
This has lots of problems, starting with the fact that `Exec` may not be the name of the program and not getting better from there
I assume it is done for a reason, probably Okular having 10_000_000_000 desktop files for itself.
Though the original values actually don't seem like any would cause problems based on the inserted debug print below.
Even if this can't be completely removed, it is much better to use the Name key for this or something

		if (line.startsWith("Exec")) {
			line.remove(0, line.indexOf('=') + 1);
			if (line.isEmpty()) {
				continue;
			}
			QStringList parts = line.split(' ');
			if (parts.first() == "env" && parts.count() > 2) {
				line = parts[2];
			}
qDebug() << "Updating appId for " << appId << " to " << line;

			appId = line;
			continue;
		}
*/

/*
I don't think we should ignore entries that have NoDisplay at all. The specification says regarding NoDisplay entries:
NoDisplay ... can be useful to e.g. associate this application with MIME types ... without having a menu entry for it
If a different desktop file has the same id somehow? then intended behavior should be to add the NoDisplay one's mimetypes to the Display one's, not ignore the NoDisplay one

		if (line.startsWith("NoDisplay=") &&
		    line.contains("true", Qt::CaseInsensitive)) {
			noDisplay = true;
		}
*/

	if (!appIcon.isEmpty() && m_applicationIcons[appFile].isEmpty()) {
		m_applicationIcons[appName] = appIcon;
	}
	// Even if mimetypes is empty, set the icon in case a different one isn't
	if (mimetypes.isEmpty()) {
		return;
	}


/*
See previous comment
	// If an application has a .desktop file without NoDisplay use that, otherwise
	// use one of the ones with NoDisplay anyways
	if (!noDisplay || !m_desktopFileNames.contains(appId)) {
		m_desktopFileNames[appId] = fileInfo.fileName();
	}
*/

//	if (!appName.isEmpty() && m_applicationNames[appId].isEmpty()) {
/*
See how often collisions occur
*/
/*
for (QString otherAppId : m_applicationNames.keys()) {
	if (m_applicationNames[otherAppId] == appName) {
		qDebug() << "Apps " << appId << " and " << otherAppId << " share name " << appName;
	}
}
*/
/*
Based on this, it seems necessary to group mimetypes by application name, rather than id.
A refactor is required
*/
//		m_applicationNames[appId] = appName;
//	}

/*
Apparently compilers these days literally cannot tell when a variable is not used or something, when compiling with -Wall -Werror on
What the fuck

	const QMimeType octetStream =
		m_mimeDb.mimeTypeForName("application/octet-stream");
*/
	for (const QString &readMimeName : mimetypes) {
		// Resolve aliases etc
		const QMimeType mimetype =
			m_mimeDb.mimeTypeForName(readMimeName.trimmed());
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

/*
use m_apps instead
		if (m_supportedMimetypes.contains(appName, mimetypeName)) {
			// TODO check to make sure the appFile's are distinct (in case the user copied to .local/share/applications to override it for example) and print the appFiles of both conflicting definitions
			continue;
		}
*/

		if (mimetypeName.count('/') != 1) {
			qDebug() << "Warning: encountered mimetype " << mimetypeName << " without exactly 1 '/' character in " << appFile << " Unsure what to do, skipping...";
			continue;
		}

/*
TODO use type, I think this can still be deleted though. It needs to go elsewhere
		const QString type = parts[0].trimmed();
*/
		// Indexing in Qt creates a default element if one doesn't exist, so we don't need to explicitely check if m_apps[appName] exists
		// If we've already got an association for this app from a different desktop file, don't overwrite it because we read highest-priority .desktops first
		if (m_apps[appName].contains(mimetypeName)) {
			// Annoyingly, some apps like KDE mobile apps add associations for *the same exact file type* through two different aliases, so this gets spammed a lot.
			qDebug() << "Info: " << appName << " already handles " << mimetypeName << " with " << m_apps[appName][mimetypeName] << " so " << appFile << "will be ignored";
			continue;
		}
		m_apps[appName][mimetypeName] = appFile;
/*
Info is contained in m_apps, so use it elsewhere instead of this and remove this
*/
		//m_supportedMimetypes.insert(appName, mimetypeName);
	}
}

void SelectDefaultApplication::setDefault(const QString &appName, const QSet<QString> &mimetypes,
			const QSet<QString> &unselectedMimetypes)
{
/*
TODO we will need to associate both a mimetype and an appName (which will really be an appName then and not appId) to index a desktopFile, but for now appName is just the desktopFile

	QString desktopFile = m_desktopFileNames.value(appName);
*/
	const QString filePath = QDir(QStandardPaths::writableLocation(
					      QStandardPaths::ConfigLocation))
					 .absoluteFilePath("mimeapps.list");
	QFile file(filePath);

	// Read in existing mimeapps.list, skipping the lines for the mimetypes we're
	// updating
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
				inCorrectGroup =
					(line == "[Default Applications]");
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

/*
Doesn't appear to validate that it is a mimetype, which isn't good practice I think
Nevermind behaves correctly
*/
			const QString mimetype =
				m_mimeDb.mimeTypeForName(line.split('=')
								 .first()
								 .trimmed())
					.name();
			if (!mimetypes.contains(mimetype) &&
			    !unselectedMimetypes.contains(mimetype)) {
				existingAssociations.append(line);
			}
/*
I'm pretty sure if unselectedMimetypes contains mimetype but .second().trimmed()).name() isn't equal to desktopFile then we should also add it to existingAssociations

Later...: Yeah I tested it and this is a bug
*/
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
		qDebug() << "Unable to open file for reading";
/*
If we can't open the file for reading, we better stop before opening for writing and deleting it
Unless we check explicitely and the file isn't there at all
*/
	}

	if (!file.open(QIODevice::WriteOnly)) {
		QMessageBox::warning(this, tr("Failed to store settings"),
				     file.errorString());
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
		file.write(QString(mimetype + '=' +
				   m_apps[appName][mimetype] + '\n')
				   .toUtf8());
	}
}

void SelectDefaultApplication::populateApplicationList(const QString &filter) {
	m_applicationList->clear();

	QStringList applications = m_apps.keys().filter(filter);
	std::sort(applications.begin(), applications.end());
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
	QDirIterator iter(path, imageTypes, QDir::Files,
			QDirIterator::Subdirectories);

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
