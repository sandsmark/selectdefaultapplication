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

	// Check that we shit with multiple .desktop files, but some nodisplay files
	for (const QString &appId : m_supportedMimetypes.keys()) {
		if (!m_desktopFileNames.contains(appId)) {
			qWarning()
				<< appId
				<< "does not have an associated desktop file!";
			continue;
		}

		if (m_applicationNames[appId].isEmpty()) {
			qWarning() << "Missing name" << appId;
			m_applicationNames[appId] = appId;
		}
	}

	// Preload up front, so it doesn't get sluggish when selecting applications
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

	for (const QString &mimetypeName : m_supportedMimetypes.values()) {
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

	QHBoxLayout *mainLayout = new QHBoxLayout;
	setLayout(mainLayout);
	m_applicationList = new QTreeWidget;
	mainLayout->addWidget(m_applicationList);

	QGridLayout *rightLayout = new QGridLayout;
	mainLayout->addLayout(rightLayout);

	m_setDefaultButton = new QPushButton(
		tr("Set as default application for these file types"));
	m_setDefaultButton->setEnabled(false);

	m_mimetypeList = new QListWidget;
	m_mimetypeList->setSelectionMode(QAbstractItemView::MultiSelection);

	rightLayout->addWidget(m_mimetypeList);
	rightLayout->addWidget(m_setDefaultButton);

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

	connect(m_applicationList, &QTreeWidget::itemSelectionChanged, this,
		&SelectDefaultApplication::onMimetypeSelected);
	connect(m_setDefaultButton, &QPushButton::clicked, this,
		&SelectDefaultApplication::onSetDefaultClicked);
}

SelectDefaultApplication::~SelectDefaultApplication()
{
}

void SelectDefaultApplication::onMimetypeSelected()
{
	m_setDefaultButton->setEnabled(false);
	m_mimetypeList->clear();

	QList<QTreeWidgetItem *> selectedItems =
		m_applicationList->selectedItems();
	if (selectedItems.count() != 1) {
		return;
	}

	const QTreeWidgetItem *item = selectedItems.first();
	if (!item->parent()) {
		return;
	}

	const QString mimetypeGroup = item->parent()->text(0);
	const QString application = item->data(0, Qt::UserRole).toString();

	QStringList supported = m_supportedMimetypes.values(application);

	// E. g. kwrite and kate only indicate support for "text/plain", but
	// they're nice for things like c source files.
	QSet<QString> secondary;
	const QStringList currentSupported =
		m_supportedMimetypes.values(application);
	for (const QString &mimetype : currentSupported) {
		for (const QString &child : m_childMimeTypes.values(mimetype)) {
			supported.append(child);
			secondary.insert(child);
		}
	}
	supported.removeDuplicates();

	for (const QString &supportedMime : supported) {
		if (!supportedMime.startsWith(mimetypeGroup)) {
			continue;
		}
		const QMimeType mimetype =
			m_mimeDb.mimeTypeForName(supportedMime);
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
		item->setIcon(m_mimeTypeIcons[supportedMime]);
		m_mimetypeList->addItem(item);
		item->setSelected(!secondary.contains(mimeName));
	}

	m_setDefaultButton->setEnabled(m_mimetypeList->count() > 0);
}

void SelectDefaultApplication::onSetDefaultClicked()
{
	QList<QTreeWidgetItem *> selectedItems =
		m_applicationList->selectedItems();
	if (selectedItems.count() != 1) {
		return;
	}

	const QTreeWidgetItem *item = selectedItems.first();
	if (!item->parent()) {
		return;
	}

	const QString application = item->data(0, Qt::UserRole).toString();
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

	QStringList mimetypes;
	QString appName;
	QString appId = fileInfo.fileName();
	QString iconName;

	bool noDisplay = false;

	while (!file.atEnd()) {
		QString line = file.readLine().simplified();

		if (line.startsWith('[')) {
			if (line == "[Desktop Entry]") {
				continue;
			}
			// Multiple groups may not have the same name, and [Desktop Entry] must be the first group. So we are done otherwise
			break;
		}

		if (line.startsWith("MimeType")) {
			line.remove(0, line.indexOf('=') + 1);
			mimetypes = line.split(';', Qt::SkipEmptyParts);
			continue;
		}

		if (line.startsWith("Name") && !line.contains('[')) {
			line.remove(0, line.indexOf('=') + 1);
			appName = line;
			continue;
		}

		if (line.startsWith("Icon")) {
			line.remove(0, line.indexOf('=') + 1);
			iconName = line;
			continue;
		}

		if (line.startsWith("Exec")) {
			line.remove(0, line.indexOf('=') + 1);
			if (line.isEmpty()) {
				continue;
			}
			QStringList parts = line.split(' ');
			if (parts.first() == "env" && parts.count() > 2) {
				line = parts[2];
			}

			appId = line;
			continue;
		}

		if (line.startsWith("NoDisplay=") &&
		    line.contains("true", Qt::CaseInsensitive)) {
			noDisplay = true;
		}
	}

	if (!iconName.isEmpty() && m_applicationIcons[appId].isEmpty()) {
		m_applicationIcons[appId] = iconName;
	}

	// If an application has a .desktop file without NoDisplay use that, otherwise
	// use one of the ones with NoDisplay anyways
	if (!noDisplay || !m_desktopFileNames.contains(appId)) {
		m_desktopFileNames[appId] = fileInfo.fileName();
	}

	if (!appName.isEmpty() && m_applicationNames[appId].isEmpty()) {
		m_applicationNames[appId] = appName;
	}

	if (mimetypes.isEmpty()) {
		return;
	}

	const QMimeType octetStream =
		m_mimeDb.mimeTypeForName("application/octet-stream");
	for (const QString &readMimeName : mimetypes) {
		// Resolve aliases etc
		const QMimeType mimetype =
			m_mimeDb.mimeTypeForName(readMimeName.trimmed());
		if (!mimetype.isValid()) {
			continue;
		}

		const QString mimetypeName = mimetype.name();
		for (const QString &parent : mimetype.parentMimeTypes()) {
			if (parent == "application/octet-stream") {
				break;
			}
			m_childMimeTypes.insert(parent, mimetypeName);
		}
		if (m_supportedMimetypes.contains(appId, mimetypeName)) {
			continue;
		}

		const QStringList parts = mimetypeName.split('/');
		if (parts.count() != 2) {
			continue;
		}

		const QString type = parts[0].trimmed();

		m_applications[type].insert(appId);
		m_supportedMimetypes.insert(appId, mimetypeName);
	}
}

void SelectDefaultApplication::setDefault(const QString &appName, const QSet<QString> &mimetypes,
			const QSet<QString> &unselectedMimetypes)
{
	QString desktopFile = m_desktopFileNames.value(appName);
	if (desktopFile.isEmpty()) {
		qWarning() << "invalid" << appName;
		return;
	}

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

			const QString mimetype =
				m_mimeDb.mimeTypeForName(line.split('=')
								 .first()
								 .trimmed())
					.name();
			if (!mimetypes.contains(mimetype) &&
			    !unselectedMimetypes.contains(mimetype)) {
				existingAssociations.append(line);
			}
		}

		file.close();
	} else {
		qDebug() << "Unable to open file for reading";
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
				   m_desktopFileNames[appName] + '\n')
				   .toUtf8());
	}

	return;
}

void SelectDefaultApplication::loadIcons(const QString &path)
{
	QFileInfo fi(path);
	if (!fi.exists() || !fi.isDir()) {
		return;
	}
	// TODO: avoid hardcoding
	QStringList imageTypes({ "*.svg", "*.svgz", "*.png", "*.xpm" });
	QDirIterator it(path, imageTypes, QDir::Files,
			QDirIterator::Subdirectories);

	while (it.hasNext()) {
		it.next();
		fi = it.fileInfo();

		const QString name = fi.completeBaseName();
		if (m_iconPaths.contains(name)) {
			continue;
		}
		m_iconPaths[name] = fi.filePath();
	}
}
