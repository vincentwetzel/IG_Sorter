#pragma once

#include <QLabel>

class QFrame;

class ImageThumbnail : public QLabel {
    Q_OBJECT
public:
    explicit ImageThumbnail(const QString& filePath, QWidget* parent = nullptr);

    QString filePath() const { return m_filePath; }
    bool    isSelected() const { return m_selected; }
    void    setSelected(bool selected);
    void    toggleSelected();

signals:
    void clicked();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    QString m_filePath;
    bool    m_selected;
    QPixmap m_pixmap;
};
