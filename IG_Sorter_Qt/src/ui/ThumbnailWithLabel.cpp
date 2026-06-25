#include "ui/ThumbnailWithLabel.h"
#include <QDir>
#include <QFileInfo>
#include <QLabel>
#include <QProcess>
#include <QVBoxLayout>
#include "ui/ImageThumbnail.h"

ThumbnailWithLabel::ThumbnailWithLabel(const QString& filePath, QWidget* parent)
    : QWidget(parent) {
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(2);

    m_thumbnail = new ImageThumbnail(filePath, this);
    m_layout->addWidget(m_thumbnail, 1);

    // Dimensions label (e.g. "720x720")
    m_dimensionsLabel = new QLabel(this);
    m_dimensionsLabel->setAlignment(Qt::AlignCenter);
    QFont dimFont = m_dimensionsLabel->font();
    dimFont.setPointSize(8);
    m_dimensionsLabel->setFont(dimFont);
    m_dimensionsLabel->setText("-");  // placeholder until image loads
    m_layout->addWidget(m_dimensionsLabel);

    const QFileInfo fi(filePath);
    const QString fileName = fi.fileName();

    // Make the filename a clickable hyperlink
    m_nameLabel = new QLabel(this);
    m_nameLabel->setText(QString("<a href=\"file://%1\">%2</a>")
                             .arg(filePath, fileName));
    m_nameLabel->setAlignment(Qt::AlignCenter);
    m_nameLabel->setWordWrap(true);
    m_nameLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    m_nameLabel->setOpenExternalLinks(false);
    QFont font = m_nameLabel->font();
    font.setPointSize(8);
    m_nameLabel->setFont(font);
    m_layout->addWidget(m_nameLabel);

    // Handle link clicks — open file explorer and highlight the file
#ifdef Q_OS_WIN
    const QString nativePath = QDir::toNativeSeparators(filePath);
    connect(m_nameLabel, &QLabel::linkActivated, this, [nativePath]() {
        QProcess::startDetached("explorer", {"/select,", nativePath});
    });
#elif defined(Q_OS_MACOS)
    connect(m_nameLabel, &QLabel::linkActivated, this, [filePath]() {
        QProcess::startDetached("open", {"-R", filePath});
    });
#else
    const QString absolutePath = fi.absolutePath();
    connect(m_nameLabel, &QLabel::linkActivated, this, [absolutePath]() {
        QProcess::startDetached("xdg-open", {absolutePath});
    });
#endif

    // Forward thumbnail clicks
    connect(m_thumbnail, &ImageThumbnail::clicked, this, &ThumbnailWithLabel::clicked);

    // Set dimensions from already-loaded image
    const QSize size = m_thumbnail->imageDimensions();
    if (!size.isNull() && size.width() > 0 && size.height() > 0) {
        m_dimensionsLabel->setText(QString("%1x%2").arg(size.width()).arg(size.height()));
    }
}

QString ThumbnailWithLabel::filePath() const {
    return m_thumbnail->filePath();
}
