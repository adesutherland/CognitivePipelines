#pragma once

#include <QWidget>

class QLabel;
class QComboBox;
class QPushButton;
class QSpinBox;
class TransformScopeNode;

class TransformScopePropertiesWidget : public QWidget {
    Q_OBJECT

public:
    explicit TransformScopePropertiesWidget(TransformScopeNode* node, QWidget* parent = nullptr);
    ~TransformScopePropertiesWidget() override = default;

public slots:
    void setMode(const QString& mode);
    void setMaxAttempts(int value);
    void setStatus(const QString& status);

private:
    TransformScopeNode* m_node {nullptr};
    QComboBox* m_modeCombo {nullptr};
    QSpinBox* m_maxAttemptsSpin {nullptr};
    QLabel* m_statusLabel {nullptr};
    QPushButton* m_openButton {nullptr};
};
