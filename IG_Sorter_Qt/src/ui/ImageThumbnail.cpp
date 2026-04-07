#include "ui/ImageThumbnail.h"
#include <QMouseEvent>
#include <QPainter>
#include <QFileInfo>
#include <QStyleOption>

ImageThumbnail::ImageThumbnail(const QString& filePath, QWidget* parent)
    : QLabel(parent), m_filePath(filePath), m_selected(false)
{
    setMinimumSize(150, 150);
    setMaximumSize(200, 200);
    setAlignment(Qt::AlignCenter);
    setScaledContents(false);

    // Load image
    QPixmap img(filePath);
    if (!img.isNull()) {
        m_pixmap = img.scaled(180, 180, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        setPixmap(m_pixmap);
    } else {
        // Non-image file — show a placeholder with filename
        m_pixmap = QPixmap(180, 180);
        m_pixmap.fill(QColor("#404040"));
        QPainter painter(&m_pixmap);
        painter.setPen(QColor("#E0E0E0"));
        QFont font = painter.font();
        font.setPointSize(10);
        painter.setFont(font);
        painter.drawText(m_pixmap.rect(), Qt::AlignCenter | Qt::TextWordWrap,
                         QFileInfo(filePath).fileName());
        setPixmap(m_pixmap);
    }

    setToolTip(filePath);
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
    QLabel::paintEvent(event);

    if (m_selected) {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        // Draw selection border
        QPen pen(QColor("#0078D4"), 3);
        painter.setPen(pen);
        painter.drawRect(rect().adjusted(1, 1, -1, -1));

        // Draw checkmark indicator
        painter.setBrush(QColor("#0078D4"));
        painter.drawRoundedRect(width() - 24, 4, 20, 20, 4, 4);
        painter.setPen(QPen(Qt::white, 2));
        painter.drawLine(width() - 20, 14, width() - 16, 18);
        painter.drawLine(width() - 16, 18, width() - 8, 10);
    }
}
