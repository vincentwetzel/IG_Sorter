#include "ui/ImagePreviewGrid.h"

#include <QGridLayout>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QSet>

#include "ui/ImageThumbnail.h"
#include "ui/ThumbnailWithLabel.h"

ImagePreviewGrid::ImagePreviewGrid(QWidget* parent)
    : QWidget(parent), m_gridLayout(new QGridLayout(this)) {
    m_gridLayout->setSpacing(4);
    m_gridLayout->setContentsMargins(4, 4, 4, 4);
    setFocusPolicy(Qt::StrongFocus);  // Allow keyboard focus for DELETE key
}

void ImagePreviewGrid::setImages(const QStringList& filePaths) {
    clear();

    m_items.reserve(filePaths.size());
    for (const auto& filePath : filePaths) {
        auto* item = new ThumbnailWithLabel(filePath, this);
        m_items.append(item);

        connect(item, &ThumbnailWithLabel::clicked, this, [this]() {
            int selectedCount = 0;
            for (const auto* it : m_items) {
                if (it->thumbnail()->isSelected()) {
                    selectedCount++;
                }
            }
            emit selectionChanged(selectedCount);
        });
    }

    rebuildGrid();
    setFocus();  // Set focus to grid so DELETE key works immediately
}

void ImagePreviewGrid::rebuildGrid() {
    if (m_items.isEmpty()) {
        return;
    }

    const int availableWidth = width() - m_gridLayout->contentsMargins().left()
                             - m_gridLayout->contentsMargins().right();
    const int availableHeight = height() - m_gridLayout->contentsMargins().top()
                              - m_gridLayout->contentsMargins().bottom();

    // Compute the average aspect ratio (width/height) across all images
    double avgAspect = 0.75;  // default portrait-ish
    QVariant cached = property("cachedAvgAspect");
    if (cached.isValid()) {
        avgAspect = cached.toDouble();
    } else {
        double sumAspect = 0;
        int validCount = 0;
        for (const auto* item : m_items) {
            const QSize imgSize = item->thumbnail()->imageDimensions();
            if (!imgSize.isNull() && imgSize.width() > 0 && imgSize.height() > 0) {
                sumAspect += static_cast<double>(imgSize.width()) / imgSize.height();
                validCount++;
            }
        }
        if (validCount > 0) {
            avgAspect = sumAspect / validCount;
        }
        setProperty("cachedAvgAspect", avgAspect);
    }

    // Pick a column count so thumbnails are reasonably large
    constexpr int desiredThumbWidth = 300;
    int cols = qMax(1, availableWidth / desiredThumbWidth);
    cols = qMin(cols, m_items.size());

    const int cellWidth = (availableWidth - (cols - 1) * m_gridLayout->horizontalSpacing()) / cols;
    // Height is derived from cell width and average aspect ratio
    int cellHeight = qMax(80, static_cast<int>(cellWidth / avgAspect));

    // If the total height needed exceeds available, scale down
    const int rows = (m_items.size() + cols - 1) / cols;
    const int totalNeededHeight = rows * cellHeight + (rows - 1) * m_gridLayout->verticalSpacing();
    if (totalNeededHeight > availableHeight && availableHeight > 0) {
        const double scale = static_cast<double>(availableHeight) / totalNeededHeight;
        cellHeight = qMax(80, static_cast<int>(cellHeight * scale));
    }

    const QSize cellSize(cellWidth, cellHeight);
    const bool colsChanged = (cols != m_cols);
    m_cols = cols;

    if (colsChanged) {
        // Clear existing grid items
        for (int i = m_items.size() - 1; i >= 0; --i) {
            m_gridLayout->removeWidget(m_items[i]);
        }

        // Reset all row/column properties
        for (int c = 0; c < m_gridLayout->columnCount(); ++c) {
            m_gridLayout->setColumnStretch(c, 0);
            m_gridLayout->setColumnMinimumWidth(c, 0);
        }
        for (int r = 0; r < m_gridLayout->rowCount(); ++r) {
            m_gridLayout->setRowStretch(r, 0);
            m_gridLayout->setRowMinimumHeight(r, 0);
        }
    }

    // Apply minimum sizes
    for (int r = 0; r < rows; ++r) {
        if (m_gridLayout->rowMinimumHeight(r) != cellHeight) {
            m_gridLayout->setRowMinimumHeight(r, cellHeight);
        }
    }
    for (int c = 0; c < cols; ++c) {
        if (m_gridLayout->columnMinimumWidth(c) != cellWidth) {
            m_gridLayout->setColumnMinimumWidth(c, cellWidth);
        }
    }

    // Add items back and notify of cell size
    for (int i = 0; i < m_items.size(); ++i) {
        int row = i / m_cols;
        int col = i % m_cols;
        if (colsChanged) {
            m_gridLayout->addWidget(m_items[i], row, col);
        }
        m_items[i]->thumbnail()->setCellSize(cellSize);
    }
}

void ImagePreviewGrid::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    rebuildGrid();
}

void ImagePreviewGrid::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Delete) {
        // Optimize: avoid constructing a temporary string list and copying paths on keypress
        bool hasSelected = false;
        for (const auto* item : m_items) {
            if (item->thumbnail()->isSelected()) {
                hasSelected = true;
                break;
            }
        }
        if (hasSelected) {
            emit deleteKeyPressed();
        }
    } else {
        // Pass other keys to parent
        QWidget::keyPressEvent(event);
    }
}

QStringList ImagePreviewGrid::selectedFilePaths() const {
    QStringList selected;
    selected.reserve(m_items.size());
    for (const auto* item : m_items) {
        if (item->thumbnail()->isSelected()) {
            selected.append(item->filePath());
        }
    }
    return selected;
}

void ImagePreviewGrid::removeSelected() {
    QList<ThumbnailWithLabel*> remainingItems;
    remainingItems.reserve(m_items.size());

    for (auto* item : m_items) {
        if (item->thumbnail()->isSelected()) {
            m_gridLayout->removeWidget(item);
            item->deleteLater();
        } else {
            remainingItems.append(item);
        }
    }
    m_items = remainingItems;
    setProperty("cachedAvgAspect", QVariant());

    emit selectionChanged(0);
    rebuildGrid();
}

void ImagePreviewGrid::removePath(const QString& filePath) {
    for (auto it = m_items.begin(); it != m_items.end(); ++it) {
        if ((*it)->filePath() == filePath) {
            auto* item = *it;
            m_gridLayout->removeWidget(item);
            item->deleteLater();
            m_items.erase(it);
            setProperty("cachedAvgAspect", QVariant());
            emit selectionChanged(0);
            rebuildGrid();
            return;
        }
    }
}

void ImagePreviewGrid::releaseImages(const QStringList& filePaths) {
    if (filePaths.isEmpty()) {
        return;
    }
    if (filePaths.size() < 4) {
        for (auto* item : m_items) {
            if (filePaths.contains(item->filePath())) {
                item->thumbnail()->releaseImage();
            }
        }
        return;
    }
    const QSet<QString> pathSet(filePaths.begin(), filePaths.end());
    for (auto* item : m_items) {
        if (pathSet.contains(item->filePath())) {
            item->thumbnail()->releaseImage();
        }
    }
}

void ImagePreviewGrid::clear() {
    for (auto* item : m_items) {
        m_gridLayout->removeWidget(item);
        item->deleteLater();
    }
    m_items.clear();
    m_cols = 0;
    setProperty("cachedAvgAspect", QVariant());

    // Reset all row/column properties to prevent stale layout constraints
    for (int c = 0; c < m_gridLayout->columnCount(); ++c) {
        m_gridLayout->setColumnStretch(c, 0);
        m_gridLayout->setColumnMinimumWidth(c, 0);
    }
    for (int r = 0; r < m_gridLayout->rowCount(); ++r) {
        m_gridLayout->setRowStretch(r, 0);
        m_gridLayout->setRowMinimumHeight(r, 0);
    }
}

bool ImagePreviewGrid::hasImages() const {
    return !m_items.isEmpty();
}

void ImagePreviewGrid::setBatchInfo(const QString& current, const QString& total) {
    Q_UNUSED(current);
    Q_UNUSED(total);
}

void ImagePreviewGrid::selectAll() {
    for (auto* item : m_items) {
        item->thumbnail()->setSelected(true);
    }
    int selectedCount = m_items.size();
    emit selectionChanged(selectedCount);
}

void ImagePreviewGrid::deselectAll() {
    for (auto* item : m_items) {
        item->thumbnail()->setSelected(false);
    }
    emit selectionChanged(0);
}

bool ImagePreviewGrid::allSelected() const {
    if (m_items.isEmpty()) {
        return false;
    }
    for (const auto* item : m_items) {
        if (!item->thumbnail()->isSelected()) {
            return false;
        }
    }
    return true;
}

void ImagePreviewGrid::selectSingle(int index) {
    const bool wasBlocked = blockSignals(true);
    deselectAll();
    blockSignals(wasBlocked);

    if (index >= 0 && index < m_items.size()) {
        m_items[index]->thumbnail()->setSelected(true);
        emit selectionChanged(1);
    } else {
        emit selectionChanged(0);
    }
}
