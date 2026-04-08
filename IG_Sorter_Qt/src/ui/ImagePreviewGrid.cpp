#include "ui/ImagePreviewGrid.h"
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
        auto* thumb = new ImageThumbnail(filePath, this);
        m_thumbnails.append(thumb);

        connect(thumb, &ImageThumbnail::clicked, this, [this]() {
            int selectedCount = 0;
            for (auto* t : m_thumbnails) {
                if (t->isSelected()) selectedCount++;
            }
            emit selectionChanged(selectedCount);
        });
    }

    rebuildGrid();
}

void ImagePreviewGrid::rebuildGrid() {
    if (m_thumbnails.isEmpty()) return;

    int availableWidth = width() - m_gridLayout->contentsMargins().left()
                       - m_gridLayout->contentsMargins().right();
    int availableHeight = height() - m_gridLayout->contentsMargins().top()
                        - m_gridLayout->contentsMargins().bottom();

    // Compute the average aspect ratio (width/height) across all images
    double avgAspect = 0.75;  // default portrait-ish
    double sumAspect = 0;
    for (const auto* thumb : m_thumbnails) {
        QSize imgSize = thumb->imageDimensions();
        if (!imgSize.isNull() && imgSize.height() > 0) {
            sumAspect += (double)imgSize.width() / imgSize.height();
        }
    }
    avgAspect = sumAspect / m_thumbnails.size();

    // Pick a column count so thumbnails are reasonably large
    int desiredThumbWidth = 300;
    int cols = qMax(1, availableWidth / desiredThumbWidth);
    cols = qMin(cols, m_thumbnails.size());

    int cellWidth = (availableWidth - (cols - 1) * m_gridLayout->horizontalSpacing()) / cols;
    // Height is derived from cell width and average aspect ratio
    int cellHeight = qMax(80, (int)(cellWidth / avgAspect));

    // If the total height needed exceeds available, scale down
    int rows = (m_thumbnails.size() + cols - 1) / cols;
    int totalNeededHeight = rows * cellHeight + (rows - 1) * m_gridLayout->verticalSpacing();
    if (totalNeededHeight > availableHeight && availableHeight > 0) {
        double scale = (double)availableHeight / totalNeededHeight;
        cellHeight = qMax(80, (int)(cellHeight * scale));
    }

    QSize cellSize(cellWidth, cellHeight);
    m_cols = cols;

    // Clear existing grid items
    for (int i = m_thumbnails.size() - 1; i >= 0; --i) {
        m_gridLayout->removeWidget(m_thumbnails[i]);
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
    rows = (m_thumbnails.size() + cols - 1) / cols;
    for (int r = 0; r < rows; ++r) {
        m_gridLayout->setRowMinimumHeight(r, cellHeight);
    }
    for (int c = 0; c < cols; ++c) {
        m_gridLayout->setColumnMinimumWidth(c, cellWidth);
    }

    // Add thumbnails back
    for (int i = 0; i < m_thumbnails.size(); ++i) {
        int row = i / m_cols;
        int col = i % m_cols;
        m_gridLayout->addWidget(m_thumbnails[i], row, col);
        m_thumbnails[i]->setCellSize(cellSize);
    }
}

void ImagePreviewGrid::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    rebuildGrid();
}

QStringList ImagePreviewGrid::selectedFilePaths() const {
    QStringList selected;
    for (const auto* thumb : m_thumbnails) {
        if (thumb->isSelected()) {
            selected.append(thumb->filePath());
        }
    }
    return selected;
}

void ImagePreviewGrid::removeSelected() {
    QList<ImageThumbnail*> toRemove;
    for (auto* thumb : m_thumbnails) {
        if (thumb->isSelected()) {
            toRemove.append(thumb);
        }
    }

    for (auto* thumb : toRemove) {
        m_gridLayout->removeWidget(thumb);
        thumb->deleteLater();
        m_thumbnails.removeOne(thumb);
    }

    emit selectionChanged(0);
    rebuildGrid();
}

void ImagePreviewGrid::clear() {
    for (auto* thumb : m_thumbnails) {
        m_gridLayout->removeWidget(thumb);
        thumb->deleteLater();
    }
    m_thumbnails.clear();
    m_cols = 0;
}

bool ImagePreviewGrid::hasImages() const {
    return !m_thumbnails.isEmpty();
}

void ImagePreviewGrid::setBatchInfo(const QString& current, const QString& total) {
    Q_UNUSED(current);
    Q_UNUSED(total);
}
