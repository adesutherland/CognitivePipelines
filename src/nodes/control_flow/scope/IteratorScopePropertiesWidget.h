#pragma once

#include <QWidget>

class QLabel;
class QComboBox;
class QPushButton;
class IteratorScopeNode;

class IteratorScopePropertiesWidget : public QWidget {
    Q_OBJECT

public:
    explicit IteratorScopePropertiesWidget(IteratorScopeNode* node, QWidget* parent = nullptr);
    ~IteratorScopePropertiesWidget() override = default;

public slots:
    void setFailurePolicy(const QString& policy);
    void setStatus(const QString& status);

private:
    IteratorScopeNode* m_node {nullptr};
    QComboBox* m_failurePolicyCombo {nullptr};
    QLabel* m_statusLabel {nullptr};
    QPushButton* m_openButton {nullptr};
};
