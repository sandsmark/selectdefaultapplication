#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QMimeDatabase>
#include <QMultiHash>

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

	// Hashtable of application names to mimetypes
	// Used because it is difficult to extract the information from m_apps' multiple different hashtables
	// JK its not used
	//QMultiHash<QString,QString> m_supportedMimetypes;
	// Hashtable with keys as parent mime types and values as all children of that mimetype which are encountered
	QMultiHash<QString, QString> m_childMimeTypes;

	// Hashtable of application names to hashtables of mimetypes to .desktop file entries
	QHash<QString, QHash<QString, QString> > m_apps;
	// Hashtable of application names to icons
	QHash<QString, QString> m_applicationIcons;
	//QHash<QString, QString> m_applicationNames;
	//QHash<QString, QString> m_desktopFileNames;

	QHash<QString, QString> m_appIdToDesktopFile;

	QHash<QString, QIcon>
		m_mimeTypeIcons; // for preloading icons, because that's (a bit) slooow
	QHash<QString, QString> m_iconPaths;

	QListWidget *m_applicationList;
	QMimeDatabase m_mimeDb;
	QListWidget *m_mimetypeList;
	QPushButton *m_setDefaultButton;
};

#endif // WIDGET_H
