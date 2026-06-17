#include "ThemeRegistry.h"

namespace Theme {

const std::vector<ThemeDef>& availableThemes() {
    // 主题注册表：要加新主题，在这里追加一条即可（详见 ThemeRegistry.h 注释）。
    // 顺序即偏好设置里的展示顺序；索引 0 默认为程序首次启动时的回退主题。
    static const std::vector<ThemeDef> kThemes = {
        { QStringLiteral("classic"),
          QStringLiteral("经典"),
          QStringLiteral(":/styles/theme_classic.qss"),
          /*frostedSurface=*/false },

        { QStringLiteral("aurora"),
          QStringLiteral("暗色"),
          QStringLiteral(":/styles/theme_aurora.qss"),
          /*frostedSurface=*/true },
    };
    return kThemes;
}

int themeCount() {
    return static_cast<int>(availableThemes().size());
}

int indexForKey(const QString& key) {
    const auto& themes = availableThemes();
    for (int i = 0; i < static_cast<int>(themes.size()); ++i) {
        if (themes[i].key == key) {
            return i;
        }
    }
    return 0; // 未知 key 回退到第一个主题
}

const ThemeDef& themeAt(int index) {
    const auto& themes = availableThemes();
    if (index < 0 || index >= static_cast<int>(themes.size())) {
        index = 0;
    }
    return themes[index];
}

} // namespace Theme
