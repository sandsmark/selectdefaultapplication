#include "widget.h"
#include <QFile>
#include <QDebug>
#include <QDir>
#include <QTreeWidget>
#include <QHBoxLayout>
#include <QGridLayout>

Widget::Widget(QWidget *parent)
    : QWidget(parent)
{
    loadDesktopFiles("/usr/share/applications/");

    m_octetstreamType = m_mimeDb.mimeTypeForName("application/octet-stream");

    QHBoxLayout *mainLayout = new QHBoxLayout;
    setLayout(mainLayout);
    m_mimeList = new QTreeWidget;
    mainLayout->addWidget(m_mimeList);

    QGridLayout *rightLayout = new QGridLayout;
    mainLayout->addLayout(rightLayout);

    m_appList = new QTreeWidget;
    rightLayout->addWidget(m_appList);

    QStringList types = m_mimeSubtypes.uniqueKeys();
    std::sort(types.begin(), types.end());
    for (const QString &type : types)  {
        QTreeWidgetItem *typeItem = new QTreeWidgetItem(QStringList(type));
        QStringList subtypes = m_mimeSubtypes[type].toList();
        std::sort(subtypes.begin(), subtypes.end());
        for (const QString &subtype : subtypes) {
            typeItem->addChild(new QTreeWidgetItem(QStringList(subtype)));
        }
        m_mimeList->addTopLevelItem(typeItem);
    }

    connect(m_mimeList, &QTreeWidget::itemSelectionChanged, this, &Widget::onMimetypeSelected);
}

Widget::~Widget()
{

}

void Widget::onMimetypeSelected()
{
    QList<QTreeWidgetItem*> selectedItems = m_mimeList->selectedItems();
    if (selectedItems.count() != 1) {
        return;
    }

    QTreeWidgetItem *item = selectedItems.first();
    if (!item->parent()) {
        return;
    }

    const QString typeName = item->parent()->text(0) + '/' + item->text(0);
    QMimeType mimetype = m_mimeDb.mimeTypeForName(typeName);
    qDebug() << mimetype.genericIconName() << mimetype.comment() << mimetype.iconName() << mimetype.preferredSuffix() << mimetype.suffixes() << mimetype.globPatterns() << mimetype.filterString();
    QStringList types = mimetype.aliases();
    types.append(typeName);
    QSet<QString> appSet;
    for (const QString &alias : types) {
        if (alias == m_octetstreamType.name()) {
            continue;
        }

        if (m_applications.contains(alias)) {
            appSet.unite(m_applications.values(alias).toSet());
        }
    }

    QStringList apps = appSet.toList();
    std::sort(apps.begin(), apps.end());

    m_appList->clear();
    for (const QString &app : apps) {
//        QTreeWidgetItem *appItem = new QTreeWidgetItem(QStringList(app));
        QTreeWidgetItem *appItem = new QTreeWidgetItem(QStringList(m_applicationNames[app]));
        for (const QString &supportedMime : m_applicationMimetypes.values(app)) {
            QTreeWidgetItem *mimeItem = new QTreeWidgetItem(QStringList(supportedMime));
            appItem->addChild(mimeItem);
        }
        appItem->setExpanded(true);
        m_appList->addTopLevelItem(appItem);
    }
}

void Widget::loadDesktopFiles(const QString &dirPath)
{
    QDir dir(dirPath);

    for (const QFileInfo &file : dir.entryInfoList(QStringList("*.desktop"), QDir::Files)) {
        loadDesktopFile(file);
    }

}

void Widget::loadDesktopFile(const QFileInfo &fileInfo)
{
    QFile file(fileInfo.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Failed to open" << fileInfo.fileName();
        return;
    }
    QStringList mimetypes;
    QString appName;
    while (!file.atEnd()) {
        QString line = file.readLine().simplified();

        if (line.startsWith("MimeType=")) {
            line.remove(0, line.indexOf('=') + 1);
            mimetypes = line.split(';', QString::SkipEmptyParts);
        }
        if (line.startsWith("Name=")) {
            line.remove(0, line.indexOf('=') + 1);
            appName = line;
        }
        if (!mimetypes.isEmpty() && !appName.isEmpty()) {
            break;
        }
    }

    if (!appName.isEmpty()) {
        m_applicationNames.insert(fileInfo.fileName(), appName);
    } else {
        qWarning() << "Missing name" << fileInfo.fileName();
    }

    if (mimetypes.isEmpty()) {
        return;
    }

    for (const QString &readMimeName : mimetypes) {
        // Resolve aliases etc
        const QMimeType mimetype = m_mimeDb.mimeTypeForName(readMimeName.trimmed());
        if (!mimetype.isValid()) {
            continue;
        }

        const QString name = mimetype.name();
        const QStringList parts = name.split('/');
        if (parts.count() != 2) {
            continue;
        }

        const QString type = parts[0].trimmed();
        const QString subtype = parts[1].trimmed();

        m_mimeSubtypes[type].insert(subtype);
        m_applications.insert(name, fileInfo.fileName());
        m_applicationMimetypes.insert(fileInfo.fileName(), name);
    }
}
