#include "StyledDialogs.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDialog>
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

QString trText(const char* sourceText) {
    return QCoreApplication::translate("StyledDialogs", sourceText);
}

} // namespace

void StyledDialogs::applyStyle(QDialog* dialog) {
    if (!dialog) return;
    dialog->setObjectName("simpleDialog");

    QFile qssFile(":/styles/SimpleDialog.qss");
    if (!qssFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "StyledDialogs: failed to load QSS:" << qssFile.errorString();
        return;
    }
    dialog->setStyleSheet(QString::fromUtf8(qssFile.readAll()));
}

std::optional<QString> StyledDialogs::getText(QWidget* parent,
                                              const QString& title,
                                              const QString& prompt,
                                              const QString& placeholder) {
    QDialog dialog(parent);
    dialog.setWindowTitle(title);
    dialog.setMinimumWidth(380);
    applyStyle(&dialog);

    auto *root = new QVBoxLayout(&dialog);
    root->setContentsMargins(24, 20, 24, 18);
    root->setSpacing(14);

    auto *promptLabel = new QLabel(prompt, &dialog);
    promptLabel->setObjectName("dialogPrompt");
    promptLabel->setWordWrap(true);
    root->addWidget(promptLabel);

    auto *edit = new QLineEdit(&dialog);
    edit->setObjectName("dialogInput");
    if (!placeholder.isEmpty()) edit->setPlaceholderText(placeholder);
    root->addWidget(edit);

    auto *footer = new QHBoxLayout;
    footer->setSpacing(10);
    footer->addStretch();
    auto *cancelBtn = new QPushButton(trText("取消"), &dialog);
    cancelBtn->setObjectName("dialogSecondary");
    cancelBtn->setCursor(Qt::PointingHandCursor);
    auto *okBtn = new QPushButton(trText("确定"), &dialog);
    okBtn->setObjectName("dialogPrimary");
    okBtn->setCursor(Qt::PointingHandCursor);
    okBtn->setDefault(true);
    footer->addWidget(cancelBtn);
    footer->addWidget(okBtn);
    root->addLayout(footer);

    QObject::connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);
    QObject::connect(okBtn,     &QPushButton::clicked, &dialog, &QDialog::accept);
    QObject::connect(edit,      &QLineEdit::returnPressed, &dialog, &QDialog::accept);

    edit->setFocus();
    if (dialog.exec() != QDialog::Accepted) return std::nullopt;

    const QString result = edit->text().trimmed();
    if (result.isEmpty()) return std::nullopt;
    return result;
}

std::optional<std::pair<QString, QString>> StyledDialogs::getCardPair(
    QWidget* parent,
    const QString& title,
    const QString& initFront,
    const QString& initBack) {
    QDialog dialog(parent);
    dialog.setWindowTitle(title);
    dialog.setMinimumWidth(420);
    applyStyle(&dialog);

    auto *root = new QVBoxLayout(&dialog);
    root->setContentsMargins(24, 20, 24, 18);
    root->setSpacing(12);

    auto *frontLabel = new QLabel(trText("正面 (问题)"), &dialog);
    frontLabel->setObjectName("dialogPrompt");
    root->addWidget(frontLabel);
    auto *frontEdit = new QLineEdit(&dialog);
    frontEdit->setObjectName("dialogInput");
    frontEdit->setText(initFront);
    root->addWidget(frontEdit);

    auto *backLabel = new QLabel(trText("背面 (答案)"), &dialog);
    backLabel->setObjectName("dialogPrompt");
    root->addWidget(backLabel);
    auto *backEdit = new QLineEdit(&dialog);
    backEdit->setObjectName("dialogInput");
    backEdit->setText(initBack);
    root->addWidget(backEdit);

    auto *footer = new QHBoxLayout;
    footer->setSpacing(10);
    footer->addStretch();
    auto *cancelBtn = new QPushButton(trText("取消"), &dialog);
    cancelBtn->setObjectName("dialogSecondary");
    cancelBtn->setCursor(Qt::PointingHandCursor);
    auto *okBtn = new QPushButton(trText("保存"), &dialog);
    okBtn->setObjectName("dialogPrimary");
    okBtn->setCursor(Qt::PointingHandCursor);
    okBtn->setDefault(true);
    footer->addWidget(cancelBtn);
    footer->addWidget(okBtn);
    root->addLayout(footer);

    QObject::connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);
    QObject::connect(okBtn,     &QPushButton::clicked, &dialog, &QDialog::accept);

    frontEdit->setFocus();
    frontEdit->selectAll();

    if (dialog.exec() != QDialog::Accepted) return std::nullopt;

    const QString f = frontEdit->text().trimmed();
    const QString b = backEdit->text().trimmed();
    if (f.isEmpty() || b.isEmpty()) return std::nullopt;
    return std::make_pair(f, b);
}

void StyledDialogs::info(QWidget* parent,
                         const QString& title,
                         const QString& message) {
    QDialog dialog(parent);
    dialog.setWindowTitle(title);
    dialog.setMinimumWidth(360);
    applyStyle(&dialog);

    auto *root = new QVBoxLayout(&dialog);
    root->setContentsMargins(24, 20, 24, 18);
    root->setSpacing(14);

    auto *msgLabel = new QLabel(message, &dialog);
    msgLabel->setObjectName("dialogMessage");
    msgLabel->setWordWrap(true);
    root->addWidget(msgLabel);

    auto *footer = new QHBoxLayout;
    footer->addStretch();
    auto *okBtn = new QPushButton(trText("确定"), &dialog);
    okBtn->setObjectName("dialogPrimary");
    okBtn->setCursor(Qt::PointingHandCursor);
    okBtn->setDefault(true);
    footer->addWidget(okBtn);
    root->addLayout(footer);

    QObject::connect(okBtn, &QPushButton::clicked, &dialog, &QDialog::accept);

    okBtn->setFocus();
    dialog.exec();
}

bool StyledDialogs::confirm(QWidget* parent,
                            const QString& title,
                            const QString& message,
                            bool dangerAction) {
    QDialog dialog(parent);
    dialog.setWindowTitle(title);
    dialog.setMinimumWidth(380);
    applyStyle(&dialog);

    auto *root = new QVBoxLayout(&dialog);
    root->setContentsMargins(24, 20, 24, 18);
    root->setSpacing(14);

    auto *msgLabel = new QLabel(message, &dialog);
    msgLabel->setObjectName("dialogMessage");
    msgLabel->setWordWrap(true);
    root->addWidget(msgLabel);

    auto *footer = new QHBoxLayout;
    footer->setSpacing(10);
    footer->addStretch();
    auto *cancelBtn = new QPushButton(trText("取消"), &dialog);
    cancelBtn->setObjectName("dialogSecondary");
    cancelBtn->setCursor(Qt::PointingHandCursor);
    auto *okBtn = new QPushButton(trText("确定"), &dialog);
    okBtn->setObjectName(dangerAction ? "dialogDanger" : "dialogPrimary");
    okBtn->setCursor(Qt::PointingHandCursor);
    // 危险操作默认焦点放在"取消"，避免误按 Enter 执行
    if (dangerAction) {
        cancelBtn->setDefault(true);
        cancelBtn->setFocus();
    } else {
        okBtn->setDefault(true);
    }
    footer->addWidget(cancelBtn);
    footer->addWidget(okBtn);
    root->addLayout(footer);

    QObject::connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);
    QObject::connect(okBtn,     &QPushButton::clicked, &dialog, &QDialog::accept);

    return dialog.exec() == QDialog::Accepted;
}
