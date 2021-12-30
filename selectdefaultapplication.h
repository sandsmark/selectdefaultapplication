#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QMimeDatabase>
#include <QMultiHash>
#include <QSet>

class QFileInfo;
class QTreeWidget;
class QListWidget;
class QPushButton;

class SelectDefaultApplication : public QWidget {
	Q_OBJECT

public:
	SelectDefaultApplication(QWidget *parent = nullptr);
	~SelectDefaultApplication();

private slots:
	void onApplicationSelected();
	void onSetDefaultClicked();

private:
	void loadDesktopFile(const QFileInfo &fileInfo);
	void setDefault(const QString &appName, const QSet<QString> &mimetypes,
			const QSet<QString> &unselectedMimetypes);
	void loadIcons(const QString &path);
	void populateApplicationList(const QString &filter);
	void addToMimetypeList(const QString &mimetypeName, const bool selected);
	QMultiHash<QString, QString> getDefaultDesktopEntries();

	// Hashtable of application names to hashtables of mimetypes to .desktop file entries
	QHash<QString, QHash<QString, QString> > m_apps;
	// Hashtable of application names to icons
	QHash<QString, QString> m_applicationIcons;
	// Multi-hashtable with keys as parent mime types and values as all children of that mimetype which are encountered
	QMultiHash<QString, QString> m_childMimeTypes;

	// Set containing all the mimegroups we saw
	QSet<QString> m_mimegroups;
	// Multi-hashtable with keys as application names and values as mimetypes
	QMultiHash<QString, QString> m_defaultApps;
	// Multi-hashtable with keys as .desktop files and values as mimetypes, read from mimeapps.list
	// Note this is opposite how they are actually stored. It is done this way so that we can read mimeapps.list before
	// parsing anything else and then as we loop over all .desktop files, fill up the associations between programs and
	// mimetypes. Remains constant after startup
	QMultiHash<QString, QString> m_defaultDesktopEntries;

	QHash<QString, QString> m_appIdToDesktopFile;

	// for preloading icons, because that's (a bit) slooow
	QHash<QString, QIcon> m_mimeTypeIcons;
	QHash<QString, QString> m_iconPaths;

	QListWidget *m_applicationList;
	QMimeDatabase m_mimeDb;
	QListWidget *m_mimetypeList;
	QPushButton *m_setDefaultButton;
};

#endif // WIDGET_H
