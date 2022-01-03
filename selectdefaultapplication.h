#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QMimeDatabase>
#include <QMultiHash>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QLineEdit>
#include <QSet>
#include <QMenu>

class QFileInfo;
class QTreeWidget;
class QListWidget;
class QPushButton;

class SelectDefaultApplication : public QWidget {
	Q_OBJECT

public:
	SelectDefaultApplication(QWidget *parent, bool isVerbose);
	~SelectDefaultApplication();

private slots:
	void onApplicationSelected();
	void onSetDefaultClicked();
	void populateApplicationList(const QString &filter);
	void showHelp();
	void constrictGroup(QAction *action);
	void enableSetDefaultButton();

private:
	void loadDesktopFile(const QFileInfo &fileInfo);
	void setDefault(const QString &appName, const QSet<QString> &mimetypes,
			const QSet<QString> &unselectedMimetypes);
	void loadIcons(const QString &path);
	void addToMimetypeList(QListWidget *list, const QString &mimetypeName, const bool selected);
	void readCurrentDefaultMimetypes();
	bool applicationHasAnyCorrectMimetype(const QString &appName);
	void onApplicationSelectedLogic(bool allowEnable);

	// Hashtable of application names to hashtables of mimetypes to .desktop file entries
	QHash<QString, QHash<QString, QString> > m_apps;
	// Hashtable of application names to icons
	QHash<QString, QString> m_applicationIcons;
	// Multi-hashtable with keys as parent mime types and values as all children of that mimetype which are encountered
	QMultiHash<QString, QString> m_childMimeTypes;

	// Set containing all the mimegroups we saw
	QSet<QString> m_mimegroups;
	// Global variable to match selected mimegroup on
	QString m_filterMimegroup;
	// Multi-hashtable with keys as mimetypes and values as application names
	QHash<QString, QString> m_defaultApps;
	// Multi-hashtable with keys as .desktop files and values as mimetypes, read from mimeapps.list
	// Note this is opposite how they are actually stored. It is done this way so that we can read mimeapps.list before
	// parsing anything else and then as we loop over all .desktop files, fill up the associations between programs and
	// mimetypes. Remains constant after startup
	QMultiHash<QString, QString> m_defaultDesktopEntries;

	bool isVerbose;

	// for preloading icons, because that's (a bit) slooow
	QHash<QString, QIcon> m_mimeTypeIcons;
	QHash<QString, QString> m_iconPaths;

	QMimeDatabase m_mimeDb;

	// UI elements
	QListWidget *m_applicationList;
	QListWidget *m_mimetypeList;
	QListWidget *m_currentDefaultApps;
	QLineEdit *m_searchBox;
	QPushButton *m_groupChooser;
	QMenu *m_mimegroupMenu;
	QPushButton *m_setDefaultButton;
	QToolButton *m_infoButton;
	QLabel *m_middleBanner;
	QLabel *m_rightBanner;
};

#endif // WIDGET_H
