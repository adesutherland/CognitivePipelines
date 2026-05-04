#include "IteratorScopePropertiesWidget.h"

#include "IteratorScopeNode.h"

#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

IteratorScopePropertiesWidget::IteratorScopePropertiesWidget(IteratorScopeNode* node, QWidget* parent)
    : QWidget(parent)
    , m_node(node)
{
    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout();

    m_failurePolicyCombo = new QComboBox(this);
    m_failurePolicyCombo->addItem(tr("Stop on error"), QStringLiteral("stop"));
    m_failurePolicyCombo->addItem(tr("Skip failed items"), QStringLiteral("skip"));
    m_failurePolicyCombo->addItem(tr("Include error rows"), QStringLiteral("include_error"));
    form->addRow(tr("Failure policy"), m_failurePolicyCombo);

    layout->addLayout(form);

    m_openButton = new QPushButton(tr("Open Body"), this);
    layout->addWidget(m_openButton);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);
    layout->addStretch(1);

    if (m_node) {
        setFailurePolicy(m_node->failurePolicy());
        setStatus(m_node->lastStatus());

        connect(m_failurePolicyCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
            if (!m_node) {
                return;
            }
            m_node->setFailurePolicy(m_failurePolicyCombo->itemData(index).toString());
        });
        connect(m_openButton, &QPushButton::clicked,
                m_node, &IteratorScopeNode::requestOpenBody);

        connect(m_node, &IteratorScopeNode::failurePolicyChanged,
                this, &IteratorScopePropertiesWidget::setFailurePolicy);
        connect(m_node, &IteratorScopeNode::statusChanged,
                this, &IteratorScopePropertiesWidget::setStatus,
                Qt::QueuedConnection);
    }
}

void IteratorScopePropertiesWidget::setFailurePolicy(const QString& policy)
{
    if (!m_failurePolicyCombo) {
        return;
    }
    const int index = m_failurePolicyCombo->findData(policy);
    if (index >= 0 && index != m_failurePolicyCombo->currentIndex()) {
        m_failurePolicyCombo->setCurrentIndex(index);
    }
}

void IteratorScopePropertiesWidget::setStatus(const QString& status)
{
    if (m_statusLabel) {
        m_statusLabel->setText(status.trimmed().isEmpty()
            ? tr("Status: not run")
            : tr("Status: %1").arg(status));
    }
}
