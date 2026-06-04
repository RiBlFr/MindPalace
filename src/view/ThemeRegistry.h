#ifndef THEMEREGISTRY_H
#define THEMEREGISTRY_H

#include <QString>
#include <vector>

namespace Theme {

/**
 * @brief 单个主题的定义。
 *
 * 新增主题只需三步：
 *   1. 在 res/styles/ 下新增一份 theme_xxx.qss；
 *   2. 在 res/resources.qrc 里注册该 qss；
 *   3. 在 ThemeRegistry.cpp 的 availableThemes() 列表里追加一条 ThemeDef。
 */
struct ThemeDef {
    QString key;            // 持久化用的稳定标识
    QString displayName;    // 偏好设置弹窗中显示的名字。
    QString qssPath;        // QSS 资源路径，如 ":/styles/theme_aurora.qss"。
    bool    frostedSurface; // 该主题的卡片表面是否使用磨砂玻璃效果（供 MainWindow 逻辑使用）。
};


const std::vector<ThemeDef>& availableThemes();

int themeCount();

int indexForKey(const QString& key);

const ThemeDef& themeAt(int index);

} // namespace Theme

#endif // THEMEREGISTRY_H
