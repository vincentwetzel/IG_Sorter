#pragma once

#include <QWidget>
#include <QList>

class QGridLayout;

class ThumbnailWithLabel;

class ImagePreviewGrid : public QWidget {
    Q_OBJECT
public:
    explicit ImagePreviewGrid(QWidget* parent = nullptr);

    void setImages(const QStringList& filePaths);
    QStringList selectedFilePaths() const;
    void        removeSelected();
    void        removePath(const QString& filePath);
    void        releaseImages(const QStringList& filePaths);
    void        clear();
    bool        hasImages() const;

    void setBatchInfo(const QString& current, const QString& total);

    // Select/deselect all thumbnails
    void selectAll();
    void deselectAll();
    bool allSelected() const;

signals:
    void selectionChanged(int selectedCount);
    void deleteKeyPressed();

protected:
    void resizeEvent(QResizeEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    void rebuildGrid();

    QGridLayout*         m_gridLayout;
    QList<ThumbnailWithLabel*> m_items;
    int               m_cols = 0;
};
