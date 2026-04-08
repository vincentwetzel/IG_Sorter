#pragma once

#include <QWidget>
#include <QString>

class QVBoxLayout;
class QLabel;

class ImageThumbnail;

class ThumbnailWithLabel : public QWidget {
    Q_OBJECT
public:
    explicit ThumbnailWithLabel(const QString& filePath, QWidget* parent = nullptr);

    ImageThumbnail* thumbnail() const { return m_thumbnail; }
    QString filePath() const;

signals:
    void clicked();

private:
    QVBoxLayout*     m_layout;
    ImageThumbnail*  m_thumbnail;
    QLabel*          m_nameLabel;
};
