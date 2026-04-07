#include "utils/ThemeManager.h"
#include "utils/ConfigManager.h"
#include <QApplication>
#include <QFile>
#include <QStyleHints>

ThemeManager* ThemeManager::s_instance = nullptr;

ThemeManager::ThemeManager(QObject* parent)
    : QObject(parent)
{
    // Read saved theme preference on construction
    m_theme = fromString(ConfigManager::instance()->theme());
}

ThemeManager* ThemeManager::instance() {
    if (!s_instance) {
        s_instance = new ThemeManager();
    }
    return s_instance;
}

Theme ThemeManager::currentTheme() const {
    return m_theme;
}

void ThemeManager::setTheme(Theme t) {
    m_theme = t;
    ConfigManager::instance()->setTheme(toString(t));
    applyTheme();
}

void ThemeManager::applyTheme() {
    Theme effectiveTheme = m_theme;
    if (m_theme == Theme::System) {
        effectiveTheme = detectSystemTheme();
    }

    QString themeFile;
    switch (effectiveTheme) {
    case Theme::Light:
        themeFile = ":/styles/light.qss";
        break;
    case Theme::Dark:
        themeFile = ":/styles/dark.qss";
        break;
    case Theme::System:
        // Should not reach here since we resolve System above
        return;
    }

    QFile file(themeFile);
    if (!file.open(QFile::ReadOnly)) {
        qWarning("ThemeManager: Failed to open theme file: %s", qPrintable(themeFile));
        return;
    }

    QString styleSheet = QString::fromUtf8(file.readAll());
    file.close();

    if (styleSheet.isEmpty()) {
        qWarning("ThemeManager: Theme file is empty: %s", qPrintable(themeFile));
        return;
    }

    qApp->setStyleSheet(styleSheet);

    emit themeChanged(effectiveTheme);
}

Theme ThemeManager::detectSystemTheme() const {
    if (qApp) {
        auto colorScheme = qApp->styleHints()->colorScheme();
        if (colorScheme == Qt::ColorScheme::Dark) {
            return Theme::Dark;
        }
    }
    return Theme::Light;
}

Theme ThemeManager::fromString(const QString& str) {
    if (str == "light") return Theme::Light;
    if (str == "dark")  return Theme::Dark;
    return Theme::System;
}

QString ThemeManager::toString(Theme t) {
    switch (t) {
    case Theme::Light: return "light";
    case Theme::Dark:  return "dark";
    case Theme::System: return "system";
    default: return "system";
    }
}
