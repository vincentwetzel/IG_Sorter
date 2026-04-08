#include "ui/ImageThumbnail.h"
#include <QMouseEvent>
#include <QPainter>
#include <QFileInfo>

ImageThumbnail::ImageThumbnail(const QString& filePath, QWidget* parent)
    : QLabel(parent), m_filePath(filePath), m_selected(false)
{
    // Load full-resolution image once
    m_pixmap.load(filePath);
    if (m_pixmap.isNull()) {
        // Placeholder for non-image files
        m_pixmap = QPixmap(200, 200);
        m_pixmap.fill(QColor("#404040"));
        QPainter painter(&m_pixmap);
        painter.setPen(QColor("#E0E0E0"));
        QFont font = painter.font();
        font.setPointSize(10);
        painter.setFont(font);
        painter.drawText(m_pixmap.rect(), Qt::AlignCenter | Qt::TextWordWrap,
                         QFileInfo(filePath).fileName());
    }

    setToolTip(filePath);
}

QSize ImageThumbnail::sizeHint() const {
    return m_cellSize.isValid() ? m_cellSize : QSize(200, 200);
}

QSize ImageThumbnail::minimumSizeHint() const {
    return QSize(50, 50);
}

void ImageThumbnail::setCellSize(const QSize& size) {
    m_cellSize = size;
    update();
}

QSize ImageThumbnail::imageDimensions() const {
    return m_pixmap.size();
}

void ImageThumbnail::setSelected(bool selected) {
    m_selected = selected;
    update();
}

void ImageThumbnail::toggleSelected() {
    m_selected = !m_selected;
    update();
    emit clicked();
}

void ImageThumbnail::mousePressEvent(QMouseEvent* event) {
    Q_UNUSED(event);
    toggleSelected();
}

void ImageThumbnail::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    if (!m_pixmap.isNull() && m_cellSize.isValid()) {
        // Scale to fit entirely within the cell — no cropping
        QPixmap scaled = m_pixmap.scaled(m_cellSize, Qt::KeepAspectRatio,
                                         Qt::SmoothTransformation);
        // Center within the widget
        int x = (width() - scaled.width()) / 2;
        int y = (height() - scaled.height()) / 2;
        painter.drawPixmap(x, y, scaled);
    }

    if (m_selected) {
        // Draw selection border
        QPen pen(QColor("#0078D4"), 3);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(rect().adjusted(1, 1, -1, -1));

        // Draw checkmark indicator
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor("#0078D4"));
        painter.drawRoundedRect(width() - 24, 4, 20, 20, 4, 4);
        painter.setPen(QPen(Qt::white, 2));
        painter.drawLine(width() - 20, 14, width() - 16, 18);
        painter.drawLine(width() - 16, 18, width() - 8, 10);
    }
}
