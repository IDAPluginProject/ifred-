#include <api.h>
#include <time.h>
#include <utils.h>

static QString themesRoot() { return pluginPath("theme/"); }

// Copy every theme directory bundled in the resources to disk, unless a
// directory of that name already exists - user edits are never overwritten.
// Files copied out of Qt resources come out read-only; fix that up so the
// user can edit them.
static void seedBundledThemes() {
  QDir bundle(":/bundle/theme");
  for (auto& name : bundle.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
    QDir dst(themesRoot() + name);
    if (dst.exists()) continue;
    if (!dst.mkpath(".")) continue;

    QDir src(bundle.filePath(name));
    for (auto& file : src.entryList(QDir::Files)) {
      QString target = dst.filePath(file);
      if (QFile::copy(src.filePath(file), target))
        QFile::setPermissions(target, QFileDevice::ReadOwner |
                                          QFileDevice::WriteOwner |
                                          QFileDevice::ReadUser |
                                          QFileDevice::WriteUser |
                                          QFileDevice::ReadGroup |
                                          QFileDevice::ReadOther);
    }
  }
}

static void ensureThemeInit() {
  static bool done;
  if (done) return;
  done = true;  // before anything below reads config.json through loadFile()

  Q_INIT_RESOURCE(theme_bundle);
  seedBundledThemes();
}

// Rewrite a "theme/<file>" request into the active theme's folder, e.g.
// "theme/window.css" -> "theme/dark/window.css". Every theme - including the
// default - is a real folder now; there are no top-level theme files. Non-theme
// paths (config.json) pass through unchanged.
//
// This is also where the "theme:" search path used by url(theme:...) in the css
// files is refreshed, so it always follows whatever config.json says by the time
// a theme file is loaded, with the themes root as a fallback.
static QString themeRelative(const QString& filename) {
  const QString prefix("theme/");
  if (!filename.startsWith(prefix)) return filename;

  const QString theme = currentTheme();
  QDir::setSearchPaths("theme",
                       QStringList() << (themesRoot() + theme) << themesRoot());
  return prefix + theme + "/" + filename.mid(prefix.size());
}

// Reads the bundled copy of <rel> (":/bundle/<rel>") and, as a side effect,
// caches it to <file> on disk so later loads read from disk.
QString loadFileFromBundle(const QString& rel, QFile& file, bool& updated) {
  QFile resFile(":/bundle/" + rel);

  updated = false;

  if (resFile.exists()) {
    if (!resFile.open(QIODevice::ReadOnly)) return QString();
    auto bytes = resFile.readAll();
    auto content = QString::fromUtf8(bytes);

    QFileInfo fileInfo(file.fileName());
    QDir().mkpath(fileInfo.absolutePath());

    if (file.open(QIODevice::WriteOnly)) {
      file.write(bytes);
      file.close();
    }

    updated = true;

    return content;
  } else
    return QString();
}

QString loadFile(const char* filename, bool force_update, bool& updated) {
  ensureThemeInit();

  const QString rel = themeRelative(QString::fromUtf8(filename));
  QFile file(pluginPath(rel.toUtf8().constData()));

  updated = false;

  if (!file.exists()) {
    // Not on disk yet - read (and cache) the bundled copy.
    return loadFileFromBundle(rel, file, updated);
  }

  if (!file.open(QIODevice::ReadOnly)) return QString();

  auto content = QString::fromUtf8(file.readAll());
  updated = true;

  return content;
}

QHash<QString, QJsonDocument> cached_json;

QJsonObject json(const char* filename, bool force_update) {
  bool updated;
  const QString& content_str = loadFile(filename, force_update, updated);

  if (!updated) return cached_json[filename].object();

  const QByteArray& content = content_str.toUtf8();
  QJsonDocument json(QJsonDocument::fromJson(content));
  cached_json[filename] = json;

  return json.object();
}

QStringList availableThemes() {
  ensureThemeInit();

  QStringList themes;
  QDir root(themesRoot());
  for (auto& name : root.entryList(QDir::Dirs | QDir::NoDotAndDotDot,
                                   QDir::Name | QDir::IgnoreCase)) {
    if (QFile::exists(root.filePath(name + "/window.css"))) themes << name;
  }
  return themes;
}

QString currentTheme() {
  auto theme = json("config.json")["theme"].toString();
  return theme.isEmpty() ? QString(PALETTE_DEFAULT_THEME) : theme;
}

bool setCurrentTheme(const QString& name) {
  // Rewrite config.json with the new "theme" key and everything else intact.
  bool updated;
  auto doc = QJsonDocument::fromJson(
      loadFile("config.json", false, updated).toUtf8());
  auto config = doc.object();
  config["theme"] = name.isEmpty() ? QString(PALETTE_DEFAULT_THEME) : name;

  QFile file(pluginPath("config.json"));
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
  file.write(QJsonDocument(config).toJson(QJsonDocument::Indented));
  file.close();
  return true;
}
