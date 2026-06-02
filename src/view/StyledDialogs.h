#ifndef MINDPALACE_STYLEDDIALOGS_H
#define MINDPALACE_STYLEDDIALOGS_H

#include <QString>
#include <optional>
#include <utility>

class QWidget;

/**
 * @brief 项目内统一风格的小型对话框 helper。
 *
 * 所有窗口共用 res/styles/SimpleDialog.qss 风格，避免散落的 QInputDialog/QMessageBox
 * 各自呈现系统原生外观。
 */
namespace StyledDialogs {

/**
 * @brief 弹出一个文本输入对话框。返回 std::nullopt 表示用户取消，
 *        返回 QString 时已对结果做过 trimmed() 且非空。
 */
std::optional<QString> getText(QWidget* parent,
                               const QString& title,
                               const QString& prompt,
                               const QString& placeholder = {});

/**
 * @brief 弹出一个
 * 确认对话框。dangerAction=true 时确认按钮显示为红色，
 *        且默认焦点落在"取消"以减少误操作。
 */
bool confirm(QWidget* parent,
             const QString& title,
             const QString& message,
             bool dangerAction = false);

/**
 * @brief 弹出一个同时编辑正面/背面的对话框。
 *        std::nullopt 表示用户取消或任一字段在 trim 后为空。
 *        返回的 pair 中 first=正面、second=背面，均已 trimmed。
 */
std::optional<std::pair<QString, QString>> getCardPair(
    QWidget* parent,
    const QString& title,
    const QString& initFront = {},
    const QString& initBack  = {});

} // namespace StyledDialogs

#endif // MINDPALACE_STYLEDDIALOGS_H
