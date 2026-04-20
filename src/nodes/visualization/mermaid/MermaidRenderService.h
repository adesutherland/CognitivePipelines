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

#include <QObject>
#include <QString>

class QWebEngineView;

class MermaidRenderService : public QObject {
    Q_OBJECT
public:
    struct RenderSizing {
        int viewWidth {0};
        int viewHeight {0};
        double effectiveScale {1.0};
        bool clamped {false};
        QString detail;
        QString error;
    };

    struct RenderResult {
        bool ok {false};
        QString error;
        QString detail;
        bool clamped {false};
        double requestedScale {1.0};
        double effectiveScale {1.0};
        double devicePixelRatio {1.0};
    };

    static MermaidRenderService& instance();

    // Compute the effective scale and viewport size for a render request without
    // invoking WebEngine, to allow preflight checks and testing.
    static RenderSizing planRenderSizing(double svgWidth, double svgHeight, double scaleFactor, double devicePixelRatio = 1.0);

    static QString formatClampDetail(double requestedScale,
                                     double effectiveScale,
                                     const QString& reason,
                                     int viewWidth,
                                     int viewHeight,
                                     double devicePixelRatio);

    RenderResult renderMermaid(const QString& mermaidCode, const QString& outputPath, double scaleFactor = 1.0);

private:
    explicit MermaidRenderService(QObject* parent = nullptr);
    ~MermaidRenderService() override = default;

private slots:
    void renderOnMainThread(const QString& mermaidCode, const QString& outputPath, double scaleFactor, RenderResult* result);

private:
    Q_DISABLE_COPY(MermaidRenderService);

    void ensureProfileInitialized();

    bool m_profileInitialized{false};
};
