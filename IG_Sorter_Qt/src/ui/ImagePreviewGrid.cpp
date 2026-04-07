#include "ui/ImagePreviewGrid.h"
#include "ui/ImageThumbnail.h"
#include <QGridLayout>

ImagePreviewGrid::ImagePreviewGrid(QWidget* parent)
    : QWidget(parent), m_gridLayout(new QGridLayout(this))
{
    m_gridLayout->setSpacing(8);
    m_gridLayout->setContentsMargins(8, 8, 8, 8);
}

void ImagePreviewGrid::setImages(const QStringList& filePaths) {
    clear();

    int cols = qMin(filePaths.size(), 5);
    // int rows = (filePaths.size() + cols - 1) / cols;  // calculated but not currently used

    for (int i = 0; i < filePaths.size(); ++i) {
        auto* thumb = new ImageThumbnail(filePaths[i], this);
        m_thumbnails.append(thumb);

        int row = i / cols;
        int col = i % cols;
        m_gridLayout->addWidget(thumb, row, col);

        connect(thumb, &ImageThumbnail::clicked, this, [this]() {
            int selectedCount = 0;
            for (auto* t : m_thumbnails) {
                if (t->isSelected()) selectedCount++;
            }
            emit selectionChanged(selectedCount);
        });
    }
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
}

void ImagePreviewGrid::clear() {
    for (auto* thumb : m_thumbnails) {
        m_gridLayout->removeWidget(thumb);
        thumb->deleteLater();
    }
    m_thumbnails.clear();
}

bool ImagePreviewGrid::hasImages() const {
    return !m_thumbnails.isEmpty();
}

void ImagePreviewGrid::setBatchInfo(const QString& current, const QString& total) {
    Q_UNUSED(current);
    Q_UNUSED(total);
    // Could be used to display batch info above the grid
}
