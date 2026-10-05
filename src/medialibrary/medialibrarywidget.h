#pragma once

#include <QWidget>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QVector>
#include "mediaitem.h"

class MediaListWidget : public QListWidget {
    Q_OBJECT
public:
    explicit MediaListWidget(QWidget *parent = nullptr);

signals:
    void filesDropped(const QStringList &filePaths);

protected:
    void startDrag(Qt::DropActions supportedActions) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
};

class MediaLibraryWidget : public QWidget {
    Q_OBJECT

public:
    explicit MediaLibraryWidget(QWidget *parent = nullptr);

    void addMediaFile(const QString &filePath);
    void addMediaFiles(const QStringList &filePaths);
    const QVector<MediaItem>& items() const { return m_items; }
    void clearItems();

signals:
    void mediaItemDoubleClicked(const MediaItem &item);
    void addToTimelineRequested(const MediaItem &item, bool separateAudio);
    void previewRequested(const MediaItem &item);
    void addTextRequested();

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    void importFilesDialog();
    void onSearchTextChanged(const QString &text);
    void setCategoryFilter(int categoryIndex); // 0: All, 1: Videos, 2: Audio, 3: Images
    void onItemDoubleClicked(QListWidgetItem *item);
    void showContextMenu(const QPoint &pos);

private:
    void refreshList();
    void updateItemDisplay(QListWidgetItem *listItem, const MediaItem &mediaItem);
    const MediaItem* itemById(const QString &id) const;

    QVector<MediaItem> m_items;
    int m_activeCategory = 0;
    QString m_searchQuery;

    QLineEdit *m_searchEdit = nullptr;
    QPushButton *m_importButton = nullptr;
    QPushButton *m_btnAll = nullptr;
    QPushButton *m_btnVideos = nullptr;
    QPushButton *m_btnAudio = nullptr;
    QPushButton *m_btnImages = nullptr;
    MediaListWidget *m_listWidget = nullptr;
};
