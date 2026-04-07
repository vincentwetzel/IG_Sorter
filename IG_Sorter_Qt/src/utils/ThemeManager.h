#pragma once

#include <QObject>
#include <QString>

enum class Theme { Light, Dark, System };

class ThemeManager : public QObject {
    Q_OBJECT
public:
    static ThemeManager* instance();

    Theme  currentTheme() const;
    void   setTheme(Theme t);
    void   applyTheme();
    static Theme fromString(const QString& str);
    static QString toString(Theme t);

signals:
    void themeChanged(Theme t);

private:
    explicit ThemeManager(QObject* parent = nullptr);
    Theme  detectSystemTheme() const;

    Theme m_theme = Theme::System;
    static ThemeManager* s_instance;
};
