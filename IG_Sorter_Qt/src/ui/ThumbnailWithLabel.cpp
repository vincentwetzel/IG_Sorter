#include "ui/ThumbnailWithLabel.h"
#include "ui/ImageThumbnail.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QFileInfo>

ThumbnailWithLabel::ThumbnailWithLabel(const QString& filePath, QWidget* parent)
    : QWidget(parent)
{
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(2);

    m_thumbnail = new ImageThumbnail(filePath, this);
    m_layout->addWidget(m_thumbnail, 1);

    m_nameLabel = new QLabel(QFileInfo(filePath).fileName(), this);
    m_nameLabel->setAlignment(Qt::AlignCenter);
    m_nameLabel->setWordWrap(true);
    QFont font = m_nameLabel->font();
    font.setPointSize(8);
    m_nameLabel->setFont(font);
    m_layout->addWidget(m_nameLabel);

    // Forward thumbnail clicks
    connect(m_thumbnail, &ImageThumbnail::clicked, this, [this]() {
        emit clicked();
    });
}

QString ThumbnailWithLabel::filePath() const {
    return m_thumbnail->filePath();
}
