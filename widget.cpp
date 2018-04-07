#include "widget.h"
#include <QFile>
#include <QDebug>
#include <QDir>
#include <QTreeWidget>
#include <QListWidget>
#include <QHBoxLayout>
#include <QGridLayout>

Widget::Widget(QWidget *parent)
    : QWidget(parent)
{
    loadDesktopFiles("/usr/share/applications/");

    m_octetstreamType = m_mimeDb.mimeTypeForName("application/octet-stream");

    QHBoxLayout *mainLayout = new QHBoxLayout;
    setLayout(mainLayout);
    m_applicationList = new QTreeWidget;
    mainLayout->addWidget(m_applicationList);

    QGridLayout *rightLayout = new QGridLayout;
    mainLayout->addLayout(rightLayout);

    m_mimetypeList = new QListWidget;
    rightLayout->addWidget(m_mimetypeList);

    QStringList types = m_applications.uniqueKeys();
    std::sort(types.begin(), types.end());
    for (const QString &type : types)  {
        QTreeWidgetItem *typeItem = new QTreeWidgetItem(QStringList(type));
        QStringList applications = m_applications[type].toList();
        std::sort(applications.begin(), applications.end());
        for (const QString &application : applications) {
            QTreeWidgetItem *appItem = new QTreeWidgetItem(QStringList(m_applicationNames[application]));
            appItem->setData(0, Qt::UserRole, application);
            typeItem->addChild(appItem);
        }
        m_applicationList->addTopLevelItem(typeItem);
    }

    m_applicationList->setHeaderHidden(true);

    connect(m_applicationList, &QTreeWidget::itemSelectionChanged, this, &Widget::onMimetypeSelected);
}

Widget::~Widget()
{

}

void Widget::onMimetypeSelected()
{
    QList<QTreeWidgetItem*> selectedItems = m_applicationList->selectedItems();
    if (selectedItems.count() != 1) {
        return;
    }

    const QTreeWidgetItem *item = selectedItems.first();
    if (!item->parent()) {
        return;
    }

    const QString mimetypeGroup = item->parent()->text(0);
    QString application = item->data(0, Qt::UserRole).toString();

    m_mimetypeList->clear();
    for (const QString &supportedMime : m_supportedMimetypes.values(application)) {
        if (!supportedMime.startsWith(mimetypeGroup)) {
            continue;
        }
        const QMimeType mimetype = m_mimeDb.mimeTypeForName(supportedMime);
        QString displayString = mimetype.filterString();
        if (displayString.isEmpty()) {
            displayString = mimetype.comment();
        }
        if (displayString.isEmpty()) {
            displayString = mimetype.name();
        }
        QListWidgetItem *item = new QListWidgetItem(mimetype.filterString());
        m_mimetypeList->addItem(item);
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
    // Ugliest implementation of .desktop file reading ever

    QFile file(fileInfo.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Failed to open" << fileInfo.fileName();
        return;
    }

    QStringList mimetypes;
    QString appName;

    bool inCorrectGroup = false;
    while (!file.atEnd()) {
        QString line = file.readLine().simplified();

        if (line.startsWith('[')) {
            inCorrectGroup = (line == "[Desktop Entry]");
            continue;
        }

        if (!inCorrectGroup) {
            continue;
        }

        if (line.startsWith("MimeType=")) {
            line.remove(0, line.indexOf('=') + 1);
            mimetypes = line.split(';', QString::SkipEmptyParts);
            continue;
        }

        if (line.startsWith("Name=")) {
            line.remove(0, line.indexOf('=') + 1);
            appName = line;
            continue;
        }

        if (line.startsWith("NoDisplay=") && line.contains("true", Qt::CaseInsensitive)) {
            return;
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
        if (m_supportedMimetypes.contains(fileInfo.fileName(), name)) {
            continue;
        }

        const QStringList parts = name.split('/');
        if (parts.count() != 2) {
            continue;
        }

        const QString type = parts[0].trimmed();

        m_applications[type].insert(fileInfo.fileName());
        m_supportedMimetypes.insert(fileInfo.fileName(), name);
    }
}
