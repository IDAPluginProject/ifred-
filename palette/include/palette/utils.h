#pragma once

#include <QFileSystemWatcher>
#include <QtGui>
#include <QtWidgets>
#include <functional>

static bool static_updated;

QJsonObject json(const char* filename, bool force_update = false);

QString loadFile(const char* filename, bool force_update = false,
                 bool& updated = static_updated);

// File handler
typedef QString (*pathhandler_t)(char const* path);
extern pathhandler_t pluginPath;

// Themes.
//
// A theme is a directory <plugin>/theme/<name>/ holding window.css (required)
// plus optional styles.json and images. Which one is active is the "theme" key
// of <plugin>/config.json; with no key at all, PALETTE_DEFAULT_THEME is used.
// A theme file that does not exist on disk falls back to the bundled copy, so a
// partial theme still works. The whole theme/ tree is folders only - there are
// no top-level theme files.
#define PALETTE_DEFAULT_THEME "dark"

// Every theme directory on disk (one with a window.css), sorted.
QStringList availableThemes();
QString currentTheme();
// Persists the choice to config.json and makes it effective for the next
// palette shown. Returns false if config.json could not be written.
bool setCurrentTheme(const QString& name);
