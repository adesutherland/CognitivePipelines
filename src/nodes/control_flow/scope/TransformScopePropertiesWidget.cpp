#include "TransformScopePropertiesWidget.h"

#include "TransformScopeNode.h"

#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

TransformScopePropertiesWidget::TransformScopePropertiesWidget(TransformScopeNode* node, QWidget* parent)
    : QWidget(parent)
    , m_node(node)
{
    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout();

    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem(tr("Run once"), QStringLiteral("run_once"));
    m_modeCombo->addItem(tr("Retry until accepted"), QStringLiteral("retry_until_accepted"));
    form->addRow(tr("Mode"), m_modeCombo);

    m_maxAttemptsSpin = new QSpinBox(this);
    m_maxAttemptsSpin->setRange(1, 1000);
    form->addRow(tr("Max attempts"), m_maxAttemptsSpin);

    layout->addLayout(form);

    m_openButton = new QPushButton(tr("Open Body"), this);
    layout->addWidget(m_openButton);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);
    layout->addStretch(1);

    if (m_node) {
        setMode(m_node->mode());
        setMaxAttempts(m_node->maxAttempts());
        setStatus(m_node->lastStatus());

        connect(m_modeCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
            if (!m_node) {
                return;
            }
            m_node->setMode(m_modeCombo->itemData(index).toString());
        });
        connect(m_maxAttemptsSpin, &QSpinBox::valueChanged,
                m_node, &TransformScopeNode::setMaxAttempts);
        connect(m_openButton, &QPushButton::clicked,
                m_node, &TransformScopeNode::requestOpenBody);

        connect(m_node, &TransformScopeNode::modeChanged,
                this, &TransformScopePropertiesWidget::setMode);
        connect(m_node, &TransformScopeNode::maxAttemptsChanged,
                this, &TransformScopePropertiesWidget::setMaxAttempts);
        connect(m_node, &TransformScopeNode::statusChanged,
                this, &TransformScopePropertiesWidget::setStatus,
                Qt::QueuedConnection);
    }
}

void TransformScopePropertiesWidget::setMode(const QString& mode)
{
    if (!m_modeCombo) {
        return;
    }
    const int index = m_modeCombo->findData(mode);
    if (index >= 0 && index != m_modeCombo->currentIndex()) {
        m_modeCombo->setCurrentIndex(index);
    }
}

void TransformScopePropertiesWidget::setMaxAttempts(int value)
{
    if (m_maxAttemptsSpin && m_maxAttemptsSpin->value() != value) {
        m_maxAttemptsSpin->setValue(value);
    }
}

void TransformScopePropertiesWidget::setStatus(const QString& status)
{
    if (m_statusLabel) {
        m_statusLabel->setText(status.trimmed().isEmpty()
            ? tr("Status: not run")
            : tr("Status: %1").arg(status));
    }
}
