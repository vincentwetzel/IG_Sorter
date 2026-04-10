#include "ui/ImageThumbnail.h"
#include "utils/LogManager.h"
#include "utils/WebpDecoder.h"
#include <QMouseEvent>
#include <QPainter>
#include <QFileInfo>
#include <QImageReader>
#include <QFile>

static bool isWebpHeader(const QByteArray& header) {
    // WebP: RIFF....WEBP
    return header.size() >= 12 &&
           header[0] == 'R' && header[1] == 'I' &&
           header[2] == 'F' && header[3] == 'F' &&
           header[8] == 'W' && header[9] == 'E' &&
           header[10] == 'B' && header[11] == 'P';
}

ImageThumbnail::ImageThumbnail(const QString& filePath, QWidget* parent)
    : QLabel(parent), m_filePath(filePath), m_selected(false)
{
    // Check magic bytes to detect actual format
    QFile file(filePath);
    bool isWebp = false;
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray header = file.read(16);
        file.close();
        isWebp = isWebpHeader(header);
    }

    if (isWebp) {
        // WebP files can't be previewed without the Qt WebP plugin
        // but the extension fixer already corrected the file extension
    }

    // Try loading with QImageReader first
    QImageReader reader(filePath);
    QImage image = reader.read();

    // If QImageReader failed and it's WebP, try Windows WIC decoder
    if (image.isNull() && isWebp) {
        image = decodeWebpViaWic(filePath);
        if (!image.isNull()) {
            LogManager::instance()->info(
                QString("Decoded WebP via WIC: %1").arg(QFileInfo(filePath).fileName()));
            m_pixmap = QPixmap::fromImage(image);
        }
    }

    if (!image.isNull() && m_pixmap.isNull()) {
        m_pixmap = QPixmap::fromImage(image);
    }

    if (m_pixmap.isNull()) {
        // Placeholder — show format info
        m_pixmap = QPixmap(200, 200);
        if (isWebp) {
            // Distinct color for WebP
            m_pixmap.fill(QColor("#2c3e50"));
        } else {
            m_pixmap.fill(QColor("#404040"));
        }
        QPainter painter(&m_pixmap);
        painter.setPen(QColor("#E0E0E0"));
        QFont font = painter.font();
        font.setPointSize(10);
        painter.setFont(font);

        QString label = QFileInfo(filePath).fileName();
        if (isWebp) {
            label = QString("[WebP — no preview]\n%1").arg(label);
        }

        painter.drawText(m_pixmap.rect(), Qt::AlignCenter | Qt::TextWordWrap, label);
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

void ImageThumbnail::releaseImage() {
    m_pixmap = QPixmap();
    update();
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
