#include "medialibrarywidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFileDialog>
#include <QMenu>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QDrag>
#include <QUrl>
#include <QDesktopServices>
#include <QIcon>
#include <QPainter>
#include <QJsonObject>
#include <QJsonDocument>

MediaListWidget::MediaListWidget(QWidget *parent)
    : QListWidget(parent)
{
    setAcceptDrops(true);
    setDragEnabled(true);
}

void MediaListWidget::startDrag(Qt::DropActions supportedActions)
{
    QListWidgetItem *item = currentItem();
    if (!item) return;

    QString filePath = item->data(Qt::UserRole + 1).toString();
    if (filePath.isEmpty()) {
        filePath = item->data(Qt::UserRole).toString();
    }
    if (filePath.isEmpty()) return;

    QDrag *drag = new QDrag(this);
    QMimeData *mimeData = new QMimeData();
    mimeData->setUrls({QUrl::fromLocalFile(filePath)});
    mimeData->setText(filePath);
    drag->setMimeData(mimeData);

    QPixmap pix = item->icon().pixmap(70, 42);
    if (!pix.isNull()) {
        drag->setPixmap(pix);
        drag->setHotSpot(QPoint(35, 21));
    }

    drag->exec(supportedActions, Qt::CopyAction);
}

void MediaListWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    } else {
        QListWidget::dragEnterEvent(event);
    }
}

void MediaListWidget::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    } else {
        QListWidget::dragMoveEvent(event);
    }
}

void MediaListWidget::dropEvent(QDropEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        QStringList paths;
        for (const QUrl &url : event->mimeData()->urls()) {
            if (url.isLocalFile()) {
                paths.append(url.toLocalFile());
            }
        }
        if (!paths.isEmpty()) {
            emit filesDropped(paths);
            event->acceptProposedAction();
            return;
        }
    }
    QListWidget::dropEvent(event);
}

MediaLibraryWidget::MediaLibraryWidget(QWidget *parent)
    : QWidget(parent)
{
    setAcceptDrops(true);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(6);

    // Header label
    QLabel *headerLabel = new QLabel("Recursos del Proyecto", this);
    headerLabel->setStyleSheet("font-weight: bold; font-size: 13px; color: #c9d1d9;");
    mainLayout->addWidget(headerLabel);

    // Action buttons (Import and Add Text)
    QHBoxLayout *topBtnsLayout = new QHBoxLayout();
    m_importButton = new QPushButton("+ Importar Medios", this);
    m_importButton->setCursor(Qt::PointingHandCursor);
    m_importButton->setStyleSheet(
        "QPushButton {"
        "  background-color: #238636; color: white; border-radius: 4px; padding: 6px 10px; font-weight: bold;"
        "}"
        "QPushButton:hover { background-color: #2ea043; }"
        "QPushButton:pressed { background-color: #1a6b2c; }"
    );
    connect(m_importButton, &QPushButton::clicked, this, &MediaLibraryWidget::importFilesDialog);
    topBtnsLayout->addWidget(m_importButton);

    QPushButton *addTextBtn = new QPushButton("🔤 + Texto", this);
    addTextBtn->setToolTip("Crear un nuevo cuadro de texto en la línea de tiempo");
    addTextBtn->setCursor(Qt::PointingHandCursor);
    addTextBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #d29922; color: #0d1117; border-radius: 4px; padding: 6px 10px; font-weight: bold;"
        "}"
        "QPushButton:hover { background-color: #e3b341; }"
        "QPushButton:pressed { background-color: #b08018; }"
    );
    connect(addTextBtn, &QPushButton::clicked, this, &MediaLibraryWidget::addTextRequested);
    topBtnsLayout->addWidget(addTextBtn);

    mainLayout->addLayout(topBtnsLayout);

    // Search bar
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("Buscar por nombre...");
    m_searchEdit->setStyleSheet(
        "QLineEdit {"
        "  background-color: #0d1117; color: #c9d1d9; border: 1px solid #30363d; border-radius: 4px; padding: 5px 8px;"
        "}"
        "QLineEdit:focus { border-color: #58a6ff; }"
    );
    connect(m_searchEdit, &QLineEdit::textChanged, this, &MediaLibraryWidget::onSearchTextChanged);
    mainLayout->addWidget(m_searchEdit);

    // Category filter tabs
    QHBoxLayout *filterLayout = new QHBoxLayout();
    filterLayout->setSpacing(4);

    auto createFilterBtn = [this](const QString &text, int catIdx) {
        QPushButton *btn = new QPushButton(text, this);
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(
            "QPushButton {"
            "  background-color: #21262d; color: #8b949e; border: 1px solid #30363d; border-radius: 3px; padding: 4px 6px; font-size: 11px;"
            "}"
            "QPushButton:hover { background-color: #30363d; color: #c9d1d9; }"
            "QPushButton:checked { background-color: #388bfd; color: white; border-color: #58a6ff; font-weight: bold; }"
        );
        connect(btn, &QPushButton::clicked, this, [this, catIdx]() {
            setCategoryFilter(catIdx);
        });
        return btn;
    };

    m_btnAll = createFilterBtn("Todos", 0);
    m_btnVideos = createFilterBtn("Videos", 1);
    m_btnAudio = createFilterBtn("Audio", 2);
    m_btnImages = createFilterBtn("Imágenes", 3);

    m_btnAll->setChecked(true);
    filterLayout->addWidget(m_btnAll);
    filterLayout->addWidget(m_btnVideos);
    filterLayout->addWidget(m_btnAudio);
    filterLayout->addWidget(m_btnImages);
    mainLayout->addLayout(filterLayout);

    // List Widget for media items
    m_listWidget = new MediaListWidget(this);
    m_listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_listWidget->setDragEnabled(true);
    m_listWidget->setIconSize(QSize(70, 42));
    m_listWidget->setStyleSheet(
        "QListWidget {"
        "  background-color: #0d1117; border: 1px solid #30363d; border-radius: 4px; color: #c9d1d9;"
        "}"
        "QListWidget::item {"
        "  padding: 6px; border-bottom: 1px solid #161b22; border-radius: 4px; margin: 2px;"
        "}"
        "QListWidget::item:hover {"
        "  background-color: #161b22;"
        "}"
        "QListWidget::item:selected {"
        "  background-color: #1f293d; border: 1px solid #388bfd;"
        "}"
    );
    m_listWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_listWidget, &QListWidget::itemDoubleClicked, this, &MediaLibraryWidget::onItemDoubleClicked);
    connect(m_listWidget, &QListWidget::customContextMenuRequested, this, &MediaLibraryWidget::showContextMenu);
    connect(m_listWidget, &MediaListWidget::filesDropped, this, &MediaLibraryWidget::addMediaFiles);

    mainLayout->addWidget(m_listWidget, 1);
}

void MediaLibraryWidget::importFilesDialog()
{
    QString allFilter = "Todos los multimedia compatibles (*." + MediaItem::allSupportedExtensions().join(" *.") + ");;";
    QString videoFilter = "Videos (*." + MediaItem::supportedVideoExtensions().join(" *.") + ");;";
    QString audioFilter = "Audios (*." + MediaItem::supportedAudioExtensions().join(" *.") + ");;";
    QString imageFilter = "Imágenes (*." + MediaItem::supportedImageExtensions().join(" *.") + ");;";
    QString filter = allFilter + videoFilter + audioFilter + imageFilter + "Todos los archivos (*.*)";

    const QStringList files = QFileDialog::getOpenFileNames(
        this,
        "Importar Archivos Multimedia",
        QDir::homePath(),
        filter
    );

    if (!files.isEmpty()) {
        addMediaFiles(files);
    }
}

void MediaLibraryWidget::addMediaFile(const QString &filePath)
{
    // Avoid duplicate paths
    for (const MediaItem &it : m_items) {
        if (it.filePath() == filePath) return;
    }

    MediaItem item(filePath);
    m_items.append(item);
    refreshList();
}

void MediaLibraryWidget::addMediaFiles(const QStringList &filePaths)
{
    const QStringList supportedExts = MediaItem::allSupportedExtensions();

    QStringList expandedPaths;
    for (const QString &fp : filePaths) {
        QFileInfo fi(fp);
        if (fi.isDir()) {
            QDir dir(fp);
            const QStringList entries = dir.entryList(QDir::Files | QDir::NoDotAndDotDot);
            for (const QString &entry : entries) {
                QString childPath = dir.absoluteFilePath(entry);
                QString ext = QFileInfo(childPath).suffix().toLower();
                if (supportedExts.contains(ext)) {
                    expandedPaths.append(childPath);
                }
            }
        } else if (fi.isFile()) {
            QString ext = fi.suffix().toLower();
            if (supportedExts.contains(ext)) {
                expandedPaths.append(fp);
            }
        }
    }

    for (const QString &fp : expandedPaths) {
        bool exists = false;
        for (const MediaItem &it : m_items) {
            if (it.filePath() == fp) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            MediaItem item(fp);
            m_items.append(item);
        }
    }
    refreshList();
}

void MediaLibraryWidget::clearItems()
{
    m_items.clear();
    refreshList();
}

void MediaLibraryWidget::onSearchTextChanged(const QString &text)
{
    m_searchQuery = text.trimmed();
    refreshList();
}

void MediaLibraryWidget::setCategoryFilter(int categoryIndex)
{
    m_activeCategory = categoryIndex;
    m_btnAll->setChecked(categoryIndex == 0);
    m_btnVideos->setChecked(categoryIndex == 1);
    m_btnAudio->setChecked(categoryIndex == 2);
    m_btnImages->setChecked(categoryIndex == 3);
    refreshList();
}

void MediaLibraryWidget::refreshList()
{
    m_listWidget->clear();

    for (const MediaItem &item : m_items) {
        // Filter by category
        if (m_activeCategory == 1 && item.type() != ClipType::Video) continue;
        if (m_activeCategory == 2 && item.type() != ClipType::Audio) continue;
        if (m_activeCategory == 3 && item.type() != ClipType::Image) continue;

        // Filter by search query
        if (!m_searchQuery.isEmpty() && !item.fileName().contains(m_searchQuery, Qt::CaseInsensitive)) {
            continue;
        }

        QListWidgetItem *listItem = new QListWidgetItem(m_listWidget);
        listItem->setData(Qt::UserRole, item.id());
        listItem->setData(Qt::UserRole + 1, item.filePath());
        updateItemDisplay(listItem, item);
        m_listWidget->addItem(listItem);
    }
}

void MediaLibraryWidget::updateItemDisplay(QListWidgetItem *listItem, const MediaItem &mediaItem)
{
    // Create compound icon with thumbnail and duration/type badge
    QPixmap iconPixmap = mediaItem.thumbnail();
    if (iconPixmap.isNull()) {
        iconPixmap = QPixmap(70, 42);
        iconPixmap.fill(QColor("#161b22"));
    } else {
        iconPixmap = iconPixmap.scaled(70, 42, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    }

    // Overlay type badge color indicator
    QPainter p(&iconPixmap);
    QColor badgeColor;
    QString badgeText;
    switch (mediaItem.type()) {
    case ClipType::Video:
        badgeColor = QColor("#2f81f7");
        badgeText = "VID";
        break;
    case ClipType::Audio:
        badgeColor = QColor("#d29922");
        badgeText = "AUD";
        break;
    case ClipType::Image:
        badgeColor = QColor("#2ea043");
        badgeText = "IMG";
        break;
    case ClipType::Text:
        badgeColor = QColor("#d29922");
        badgeText = "TXT";
        break;
    }

    p.fillRect(QRect(0, 0, 24, 12), badgeColor);
    p.setPen(Qt::white);
    QFont f = p.font();
    f.setPixelSize(8);
    f.setBold(true);
    p.setFont(f);
    p.drawText(QRect(0, 0, 24, 12), Qt::AlignCenter, badgeText);

    // Duration overlay on bottom right
    if (mediaItem.type() != ClipType::Image) {
        QString dur = mediaItem.formattedDuration();
        p.fillRect(QRect(iconPixmap.width() - 36, iconPixmap.height() - 12, 36, 12), QColor(0, 0, 0, 180));
        p.drawText(QRect(iconPixmap.width() - 36, iconPixmap.height() - 12, 36, 12), Qt::AlignCenter, dur);
    }
    p.end();

    listItem->setIcon(QIcon(iconPixmap));
    listItem->setText(QString("%1\n%2").arg(mediaItem.fileName()).arg(mediaItem.detailsString()));
    listItem->setToolTip(mediaItem.technicalSummary() + "\n\nRuta: " + mediaItem.filePath());
}

const MediaItem* MediaLibraryWidget::itemById(const QString &id) const
{
    for (const MediaItem &it : m_items) {
        if (it.id() == id) return &it;
    }
    return nullptr;
}

void MediaLibraryWidget::onItemDoubleClicked(QListWidgetItem *item)
{
    if (!item) return;
    QString id = item->data(Qt::UserRole).toString();
    if (const MediaItem *mi = itemById(id)) {
        emit mediaItemDoubleClicked(*mi);
    }
}

void MediaLibraryWidget::showContextMenu(const QPoint &pos)
{
    QListWidgetItem *item = m_listWidget->itemAt(pos);
    if (!item) return;

    QString id = item->data(Qt::UserRole).toString();
    const MediaItem *mediaItem = itemById(id);
    if (!mediaItem) return;

    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu { background-color: #161b22; color: #c9d1d9; border: 1px solid #30363d; }"
        "QMenu::item:selected { background-color: #1f293d; color: #58a6ff; }"
    );

    QAction *actAdd = menu.addAction("Añadir a la línea de tiempo");
    QAction *actAddSeparate = nullptr;
    if (mediaItem->type() == ClipType::Video) {
        actAddSeparate = menu.addAction("Separar audio y añadir como pistas independientes");
    }
    QAction *actPreview = menu.addAction("Previsualizar recurso");
    menu.addSeparator();
#if defined(Q_OS_MACOS)
    QAction *actReveal = menu.addAction("Mostrar en Finder");
#elif defined(Q_OS_WIN)
    QAction *actReveal = menu.addAction("Mostrar en el Explorador de archivos");
#else
    QAction *actReveal = menu.addAction("Mostrar en el gestor de archivos");
#endif
    QAction *actDelete = menu.addAction("Eliminar de recursos");

    QAction *selected = menu.exec(m_listWidget->viewport()->mapToGlobal(pos));
    if (!selected) return;

    if (selected == actAdd) {
        emit addToTimelineRequested(*mediaItem, false);
    } else if (selected == actAddSeparate) {
        emit addToTimelineRequested(*mediaItem, true);
    } else if (selected == actPreview) {
        emit previewRequested(*mediaItem);
    } else if (selected == actReveal) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(mediaItem->filePath()).absolutePath()));
    } else if (selected == actDelete) {
        for (int i = 0; i < m_items.size(); ++i) {
            if (m_items[i].id() == id) {
                m_items.removeAt(i);
                break;
            }
        }
        refreshList();
    }
}

void MediaLibraryWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MediaLibraryWidget::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MediaLibraryWidget::dropEvent(QDropEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        QStringList paths;
        for (const QUrl &url : event->mimeData()->urls()) {
            if (url.isLocalFile()) {
                paths.append(url.toLocalFile());
            }
        }
        if (!paths.isEmpty()) {
            addMediaFiles(paths);
            event->acceptProposedAction();
        }
    }
}
