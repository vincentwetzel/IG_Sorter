#pragma once

#include <QWidget>
#include <QList>

class QGridLayout;

class ImageThumbnail;

class ImagePreviewGrid : public QWidget {
    Q_OBJECT
public:
    explicit ImagePreviewGrid(QWidget* parent = nullptr);

    void setImages(const QStringList& filePaths);
    QStringList selectedFilePaths() const;
    void        removeSelected();
    void        clear();
    bool        hasImages() const;

    void setBatchInfo(const QString& current, const QString& total);

signals:
    void selectionChanged(int selectedCount);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void rebuildGrid();

    QGridLayout*         m_gridLayout;
    QList<ImageThumbnail*> m_thumbnails;
    int               m_cols = 0;
};
