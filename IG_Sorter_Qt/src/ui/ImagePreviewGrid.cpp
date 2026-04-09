#include "ui/ImagePreviewGrid.h"
#include "ui/ThumbnailWithLabel.h"
#include "ui/ImageThumbnail.h"
#include <QGridLayout>
#include <QResizeEvent>

ImagePreviewGrid::ImagePreviewGrid(QWidget* parent)
    : QWidget(parent), m_gridLayout(new QGridLayout(this))
{
    m_gridLayout->setSpacing(4);
    m_gridLayout->setContentsMargins(4, 4, 4, 4);
}

void ImagePreviewGrid::setImages(const QStringList& filePaths) {
    clear();

    for (const auto& filePath : filePaths) {
        auto* item = new ThumbnailWithLabel(filePath, this);
        m_items.append(item);

        connect(item, &ThumbnailWithLabel::clicked, this, [this]() {
            int selectedCount = 0;
            for (auto* it : m_items) {
                if (it->thumbnail()->isSelected()) selectedCount++;
            }
            emit selectionChanged(selectedCount);
        });
    }

    rebuildGrid();
}

void ImagePreviewGrid::rebuildGrid() {
    if (m_items.isEmpty()) return;

    int availableWidth = width() - m_gridLayout->contentsMargins().left()
                       - m_gridLayout->contentsMargins().right();
    int availableHeight = height() - m_gridLayout->contentsMargins().top()
                        - m_gridLayout->contentsMargins().bottom();

    // Compute the average aspect ratio (width/height) across all images
    double avgAspect = 0.75;  // default portrait-ish
    double sumAspect = 0;
    for (const auto* item : m_items) {
        QSize imgSize = item->thumbnail()->imageDimensions();
        if (!imgSize.isNull() && imgSize.height() > 0) {
            sumAspect += (double)imgSize.width() / imgSize.height();
        }
    }
    avgAspect = sumAspect / m_items.size();

    // Pick a column count so thumbnails are reasonably large
    int desiredThumbWidth = 300;
    int cols = qMax(1, availableWidth / desiredThumbWidth);
    cols = qMin(cols, m_items.size());

    int cellWidth = (availableWidth - (cols - 1) * m_gridLayout->horizontalSpacing()) / cols;
    // Height is derived from cell width and average aspect ratio
    int cellHeight = qMax(80, (int)(cellWidth / avgAspect));

    // If the total height needed exceeds available, scale down
    int rows = (m_items.size() + cols - 1) / cols;
    int totalNeededHeight = rows * cellHeight + (rows - 1) * m_gridLayout->verticalSpacing();
    if (totalNeededHeight > availableHeight && availableHeight > 0) {
        double scale = (double)availableHeight / totalNeededHeight;
        cellHeight = qMax(80, (int)(cellHeight * scale));
    }

    QSize cellSize(cellWidth, cellHeight);
    m_cols = cols;

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

    // Apply minimum sizes
    rows = (m_items.size() + cols - 1) / cols;
    for (int r = 0; r < rows; ++r) {
        m_gridLayout->setRowMinimumHeight(r, cellHeight);
    }
    for (int c = 0; c < cols; ++c) {
        m_gridLayout->setColumnMinimumWidth(c, cellWidth);
    }

    // Add items back and notify of cell size
    for (int i = 0; i < m_items.size(); ++i) {
        int row = i / m_cols;
        int col = i % m_cols;
        m_gridLayout->addWidget(m_items[i], row, col);
        m_items[i]->thumbnail()->setCellSize(cellSize);
    }
}

void ImagePreviewGrid::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    rebuildGrid();
}

QStringList ImagePreviewGrid::selectedFilePaths() const {
    QStringList selected;
    for (const auto* item : m_items) {
        if (item->thumbnail()->isSelected()) {
            selected.append(item->filePath());
        }
    }
    return selected;
}

void ImagePreviewGrid::removeSelected() {
    QList<ThumbnailWithLabel*> toRemove;
    for (auto* item : m_items) {
        if (item->thumbnail()->isSelected()) {
            toRemove.append(item);
        }
    }

    for (auto* item : toRemove) {
        m_gridLayout->removeWidget(item);
        item->deleteLater();
        m_items.removeOne(item);
    }

    emit selectionChanged(0);
    rebuildGrid();
}

void ImagePreviewGrid::removePath(const QString& filePath) {
    for (auto* item : m_items) {
        if (item->filePath() == filePath) {
            m_gridLayout->removeWidget(item);
            item->deleteLater();
            m_items.removeOne(item);
            emit selectionChanged(0);
            rebuildGrid();
            return;
        }
    }
}

void ImagePreviewGrid::releaseImages(const QStringList& filePaths) {
    for (auto* item : m_items) {
        if (filePaths.contains(item->filePath())) {
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
}

bool ImagePreviewGrid::hasImages() const {
    return !m_items.isEmpty();
}

void ImagePreviewGrid::setBatchInfo(const QString& current, const QString& total) {
    Q_UNUSED(current);
    Q_UNUSED(total);
}
