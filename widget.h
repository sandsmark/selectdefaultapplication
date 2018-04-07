#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QMimeDatabase>

class QFileInfo;
class QTreeWidget;

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

    QMultiHash<QString, QString> m_applications;
    QMultiHash<QString, QString> m_applicationMimetypes;
    QHash<QString, QSet<QString>> m_mimeSubtypes;
    QTreeWidget *m_mimeList;
    QMimeDatabase m_mimeDb;
    QMimeType m_octetstreamType;
    QTreeWidget *m_appList;
    QHash<QString, QString> m_applicationNames;
};

#endif // WIDGET_H
