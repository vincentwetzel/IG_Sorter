#pragma once

#include <QLabel>

class ImageThumbnail : public QLabel {
    Q_OBJECT
public:
    explicit ImageThumbnail(const QString& filePath, QWidget* parent = nullptr);

    QString filePath() const { return m_filePath; }
    bool    isSelected() const { return m_selected; }
    void    setSelected(bool selected);
    void    toggleSelected();

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    // Set the computed cell size for layout — image is drawn with this size
    void setCellSize(const QSize& size);

    // Get the image dimensions for layout calculations
    QSize imageDimensions() const;

    // Release the pixmap to free memory and any associated file handles
    void releaseImage();

signals:
    void clicked();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    QString m_filePath;
    bool    m_selected;
    QPixmap m_pixmap;
    QSize   m_cellSize;
};
