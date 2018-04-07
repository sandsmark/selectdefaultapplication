#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QMimeDatabase>

class QFileInfo;
class QTreeWidget;
class QListWidget;

class Widget : public QWidget
{
    Q_OBJECT

public:
    Widget(QWidget *parent = 0);
    ~Widget();


private slots:
    void onMimetypeSelected();

private:
    void loadDesktopFiles(const QString &dirPath);
    void loadDesktopFile(const QFileInfo &fileInfo);

    QMultiHash<QString, QString> m_supportedMimetypes;
    QHash<QString, QSet<QString>> m_applications;
    QTreeWidget *m_applicationList;
    QMimeDatabase m_mimeDb;
    QMimeType m_octetstreamType;
    QListWidget *m_mimetypeList;
    QHash<QString, QString> m_applicationNames;
};

#endif // WIDGET_H
