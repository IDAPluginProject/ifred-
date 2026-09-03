#include "dialogs.h"

#include <palette/utils.h>

#include <QtGui>
#include <QtWidgets>

#include <cmath>

// Set from CMake as YYYY.MM.DD at configure time.
#ifndef IFRED_VERSION
#define IFRED_VERSION "0.0.0"
#endif

// Greppable build stamp, embedded as plain ASCII in the binary on every OS -
// on Linux this is the only version marker an ELF .so can carry:
//   strings ida_palette64.* | grep "ifred version"
// (extern "C" gives it external linkage so it cannot be optimized away.)
extern "C" const char ifred_version_tag[] = "ifred version " IFRED_VERSION;

namespace {

// Both dialogs are small and fixed-size, but must not be narrow: a single
// column of short lines is hard to read. This is the content width.
constexpr int kDialogMinWidth = 480;

// Same idiom as api.cpp: IDA's main window is the only QMainWindow around.
// Parenting to it keeps the dialog centered on IDA, correctly modal, and -
// important here - inside the scope of IDA's theme stylesheet.
QWidget* mainWindow() {
  for (auto& widget : qApp->topLevelWidgets()) {
    if (qobject_cast<QMainWindow*>(widget)) {
      return widget;
    }
  }
  return nullptr;
}

// WCAG relative luminance / contrast ratio, used to judge a candidate link
// colour against the dialog background.
double luminance(const QColor& c) {
  auto lin = [](double v) {
    return v <= 0.03928 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
  };
  return 0.2126 * lin(c.redF()) + 0.7152 * lin(c.greenF()) +
         0.0722 * lin(c.blueF());
}

double contrast(const QColor& a, const QColor& b) {
  double hi = luminance(a), lo = luminance(b);
  if (hi < lo) std::swap(hi, lo);
  return (hi + 0.05) / (lo + 0.05);
}

// Pick a link colour that is readable under whatever theme IDA has applied.
//
// IDA themes are Qt stylesheets. They restyle text and backgrounds, but
// hyperlinks inside rich-text labels are drawn with QPalette::Link, which no
// IDA theme sets - so links come out in Qt's default blue, invisible on
// solarized-dark and friends. The stylesheet's colours do reach the widget's
// palette once it has been polished, so read them from there:
//   1. If the palette's window/text pair is self-consistent (the stylesheet
//      updated both), accept Link or Highlight when it contrasts well enough
//      against that background.
//   2. Otherwise use the text colour itself: it is, by definition, what the
//      theme made readable here. Underlining still marks it as a link.
QColor linkColor(QWidget* w) {
  w->ensurePolished();
  const QPalette pal = w->palette();
  const QColor bg = pal.color(QPalette::Window);
  const QColor fg = pal.color(QPalette::WindowText);

  if (contrast(fg, bg) >= 3.0) {
    for (auto role : {QPalette::Link, QPalette::Highlight}) {
      QColor c = pal.color(role);
      if (contrast(c, bg) >= 4.5) return c;
    }
  }
  return fg;
}

QString link(const QString& url, const QString& text, const QColor& color) {
  return QStringLiteral("<a href=\"%1\" style=\"color:%2\">%3</a>")
      .arg(url, color.name(), text);
}

// A rich-text label whose links open in the browser (QDesktopServices).
// TextBrowserInteraction is what makes them clickable and gives the hover
// cursor; the colour is baked into the HTML, see linkColor().
QLabel* richLabel(const QString& html, QWidget* parent) {
  auto* label = new QLabel(html, parent);
  label->setTextFormat(Qt::RichText);
  label->setWordWrap(true);
  label->setOpenExternalLinks(true);
  label->setTextInteractionFlags(Qt::TextBrowserInteraction);
  return label;
}

// A centered OK button, wired to close the dialog.
QDialogButtonBox* okButton(QDialog* dialog) {
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok, dialog);
  buttons->setCenterButtons(true);
  QObject::connect(buttons, &QDialogButtonBox::accepted, dialog,
                   &QDialog::accept);
  return buttons;
}

}  // namespace

void show_settings_dialog() {
  QDialog dialog(mainWindow());
  dialog.setWindowTitle(QStringLiteral("IDA Palette Settings"));
  dialog.resize(680, 540);

  const QString configPath = pluginPath("config.json");
  const QString themesDir = pluginPath("theme/");

  auto* layout = new QVBoxLayout(&dialog);
  layout->setContentsMargins(16, 16, 16, 16);

  auto* tabs = new QTabWidget(&dialog);

  // --- Tab 1: Theme Selector ------------------------------------------------
  auto* themeTab = new QWidget(tabs);
  auto* themeLayout = new QVBoxLayout(themeTab);

  auto* combo = new QComboBox(themeTab);
  auto* themeStatus = new QLabel(themeTab);
  themeStatus->setWordWrap(true);

  auto* themeHint = new QLabel(
      QStringLiteral(
          "A theme is a directory under<br><tt>%1</tt><br>"
          "holding a <tt>window.css</tt> (plus an optional <tt>styles.json</tt> "
          "and images). Each directory is one entry above; "
          "<tt>" PALETTE_DEFAULT_THEME "</tt> is the default.<br>"
          "The choice is stored as <tt>\"theme\"</tt> in <tt>config.json</tt> "
          "and takes effect the next time a palette opens.")
          .arg(themesDir.toHtmlEscaped()),
      themeTab);
  themeHint->setTextFormat(Qt::RichText);
  themeHint->setWordWrap(true);

  themeLayout->addWidget(new QLabel(QStringLiteral("Theme:"), themeTab));
  themeLayout->addWidget(combo);
  themeLayout->addWidget(themeStatus);
  themeLayout->addSpacing(12);
  themeLayout->addWidget(themeHint);
  themeLayout->addStretch(1);

  // --- Tab 2: Edit config.json ---------------------------------------------
  auto* configTab = new QWidget(tabs);
  auto* configLayout = new QVBoxLayout(configTab);

  auto* pathLabel = new QLabel(configPath, configTab);
  pathLabel->setWordWrap(true);

  auto* editor = new QPlainTextEdit(configTab);
  QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  editor->setFont(mono);
  editor->setLineWrapMode(QPlainTextEdit::NoWrap);
  editor->setTabStopDistance(
      4 * QFontMetricsF(mono).horizontalAdvance(QLatin1Char(' ')));

  auto* configStatus = new QLabel(configTab);
  configStatus->setWordWrap(true);
  auto* save = new QPushButton(QStringLiteral("Save"), configTab);

  auto* saveRow = new QHBoxLayout();
  saveRow->addWidget(configStatus, 1);
  saveRow->addWidget(save);

  configLayout->addWidget(pathLabel);
  configLayout->addWidget(editor, 1);
  configLayout->addLayout(saveRow);

  tabs->addTab(themeTab, QStringLiteral("Theme Selector"));
  tabs->addTab(configTab, QStringLiteral("Edit config.json"));

  // config.json is the single source of truth: the selector writes it, the
  // editor shows it, and after either changes it the other one re-reads it.
  auto reloadEditor = [&]() {
    editor->setPlainText(loadFile("config.json"));
    editor->document()->setModified(false);
  };
  auto reloadThemes = [&]() {
    QSignalBlocker blocker(combo);
    combo->clear();
    combo->addItems(availableThemes());
    int index = combo->findText(currentTheme());
    combo->setCurrentIndex(index >= 0 ? index : 0);
  };

  QObject::connect(
      combo, &QComboBox::currentTextChanged, &dialog,
      [&](const QString& name) {
        if (name.isEmpty()) return;  // clear() during reload
        if (!setCurrentTheme(name)) {
          themeStatus->setText(
              QStringLiteral("Could not write %1").arg(configPath));
          return;
        }
        themeStatus->setText(
            QStringLiteral("Theme \"%1\" selected.").arg(name));
        // Don't clobber edits in progress; Save will re-sync the selector.
        if (!editor->document()->isModified()) reloadEditor();
      });

  QObject::connect(save, &QPushButton::clicked, &dialog, [&]() {
    const QByteArray text = editor->toPlainText().toUtf8();

    QJsonParseError error{};
    auto doc = QJsonDocument::fromJson(text, &error);
    if (error.error != QJsonParseError::NoError) {
      configStatus->setText(QStringLiteral("Not saved - invalid JSON: %1 (at %2)")
                                .arg(error.errorString())
                                .arg(error.offset));
      return;
    }
    if (!doc.isObject()) {
      configStatus->setText(
          QStringLiteral("Not saved - the top level must be a JSON object"));
      return;
    }

    QFile file(configPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
      configStatus->setText(
          QStringLiteral("Not saved - could not write %1").arg(configPath));
      return;
    }
    file.write(text);
    file.close();

    editor->document()->setModified(false);
    configStatus->setText(QStringLiteral("Saved."));
    reloadThemes();  // the file may have changed "theme"
  });

  reloadThemes();
  reloadEditor();

  layout->addWidget(tabs, 1);
  layout->addSpacing(8);
  layout->addWidget(okButton(&dialog));

  dialog.exec();
}

void show_about_dialog() {
  QDialog dialog(mainWindow());
  dialog.setWindowTitle(QStringLiteral("IDA Palette About"));
  dialog.setMinimumWidth(kDialogMinWidth);

  const QColor lc = linkColor(&dialog);

  auto* layout = new QVBoxLayout(&dialog);
  layout->setContentsMargins(16, 16, 16, 16);
  layout->setSizeConstraint(QLayout::SetFixedSize);

  // Banner across the top. The 896x280 asset is tagged as 2x, so it renders at
  // 448x140 logical points - sharp on both normal and HiDPI screens.
  Q_INIT_RESOURCE(theme_bundle);  // resource lives in a static lib; ensure init
  QPixmap banner(QStringLiteral(":/bundle/ifred-banner.png"));
  if (!banner.isNull()) {
    banner.setDevicePixelRatio(2.0);
    auto* bannerLabel = new QLabel(&dialog);
    bannerLabel->setPixmap(banner);
    bannerLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(bannerLabel);
    layout->addSpacing(12);
  }

  auto* title = new QLabel(
      QStringLiteral("<b>ifred - IDA Palette</b><br>Version: %1")
          .arg(QStringLiteral(IFRED_VERSION)),
      &dialog);
  title->setTextFormat(Qt::RichText);
  title->setMinimumWidth(kDialogMinWidth - 32);
  layout->addWidget(title);
  layout->addSpacing(12);

  // Two columns: a right-aligned key, then the value(s). Reads far better
  // than one narrow column of alternating headings and links.
  auto* grid = new QGridLayout();
  grid->setHorizontalSpacing(16);
  grid->setVerticalSpacing(8);
  grid->setColumnStretch(1, 1);

  const struct {
    const char* key;
    QString value;
  } rows[] = {
      {"Author:", link("https://github.com/Jinmo", "jinmo", lc)},
      {"Contributors:",
       link("https://github.com/blue-devil", "BlueDeviL", lc) + "<br>" +
           link("https://github.com/nyx0", "nyx0", lc) + "<br>" +
           link("https://github.com/yrp604", "_yrp", lc)},
      {"Homepage:",
       link("https://github.com/Jinmo/ifred", "https://github.com/Jinmo/ifred",
            lc)},
      {"Themes:",
       link("https://github.com/Jinmo/ifred/tree/master/palette/res/theme",
       "IDA Palette Themes",
            lc)},
      {"License:",
       link("https://github.com/Jinmo/ifred/blob/master/LICENSE", "MIT", lc)},
  };

  int row = 0;
  for (auto& r : rows) {
    auto* key = new QLabel(QString::fromUtf8(r.key), &dialog);
    key->setAlignment(Qt::AlignRight | Qt::AlignTop);
    grid->addWidget(key, row, 0);
    grid->addWidget(richLabel(r.value, &dialog), row, 1);
    row++;
  }
  layout->addLayout(grid);

  layout->addSpacing(16);
  layout->addWidget(okButton(&dialog));

  dialog.exec();
}
