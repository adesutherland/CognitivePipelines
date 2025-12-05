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

#include <QWidget>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QPushButton>

class QPlainTextEdit;

/**
 * @brief Properties widget for configuring RagQueryNode behavior.
 *
 * Exposes controls for:
 * - Database file path (with browse button)
 * - Default query text (multi-line)
 * - Max Results: integer in [1, 50], default 5
 * - Min Relevance: double in [0.0, 1.0], default 0.5
 */
class RagQueryPropertiesWidget : public QWidget {
    Q_OBJECT
public:
    explicit RagQueryPropertiesWidget(QWidget* parent = nullptr);
    ~RagQueryPropertiesWidget() override = default;
    int maxResults() const;
    double minRelevance() const;
    QString databasePath() const;
    QString queryText() const;

public slots:
    void setMaxResults(int value);
    void setMinRelevance(double value);
    void setDatabasePath(const QString& path);
    void setQueryText(const QString& text);

signals:
    void maxResultsChanged(int value);
    void minRelevanceChanged(double value);
    void databasePathChanged(const QString& path);
    void queryTextChanged(const QString& text);

private:
    QSpinBox* m_maxResultsSpinBox {nullptr};
    QDoubleSpinBox* m_minRelevanceSpinBox {nullptr};
    QLineEdit* m_databaseEdit {nullptr};
    QPushButton* m_browseDatabaseBtn {nullptr};
    QPlainTextEdit* m_queryEdit {nullptr};

private slots:
    void onBrowseDatabase();
};
