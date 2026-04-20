//
// Cognitive Pipeline Application
//
// Copyright (c) 2025 Adrian Sutherland
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
#pragma once

#include <QDialog>
#include <QPixmap>

class QLabel;
class QScrollArea;
class QDialogButtonBox;

// Popup dialog for viewing full-resolution images
class ImagePopupDialog : public QDialog {
    Q_OBJECT
public:
    explicit ImagePopupDialog(const QPixmap& pixmap, QWidget* parent = nullptr);
    explicit ImagePopupDialog(const QString& imagePath, QWidget* parent = nullptr);
    ~ImagePopupDialog() override = default;

protected:
    void wheelEvent(QWheelEvent* event) override;
    void showEvent(QShowEvent* event) override;

private slots:
    void zoomIn();
    void zoomOut();
    void zoomFit();
    void zoomFitToWidth();
    void normalSize();

private:
    void setupUI(const QPixmap& pixmap);
    void updateImageDisplay();

    QLabel* m_imageLabel {nullptr};
    QScrollArea* m_scrollArea {nullptr};
    QDialogButtonBox* m_buttonBox {nullptr};
    
    QPixmap m_originalPixmap;
    double m_scaleFactor {1.0};
    bool m_initialZoomDone {false};
};
