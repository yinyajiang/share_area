#include <QApplication>
#include <QLocale>
#include <QTranslator>

#include "constants.h"
#include "core/app_settings.h"
#include "ui/app_icon.h"
#include "ui/main_window.h"
#include "ui/setup_dialog.h"

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);
  app.setApplicationName(Constants::APP_NAME);
  app.setApplicationVersion(Constants::APP_VERSION);
  app.setOrganizationName(QStringLiteral("ShareArea"));

  // macOS: 不在 Dock 栏显示图标
  app.setQuitOnLastWindowClosed(false);

  // 设置应用图标（Dock/任务栏）
  app.setWindowIcon(createAppIcon());

  // Load settings
  AppSettings &settings = AppSettings::instance();
  settings.load();

  // Load translation (heap allocated, MainWindow takes ownership)
  auto *translator = new QTranslator(&app);
  QString lang = settings.language();
  if (lang.isEmpty()) {
    lang = QLocale::system().name();
  }
  QString translationsDir =
      QApplication::applicationDirPath() + QStringLiteral("/translations");
  if (!translator->load(QStringLiteral("share_area_") + lang,
                        translationsDir)) {
    lang = "en";
    (void)(translator->load(QStringLiteral("share_area_") + lang,
                            translationsDir));
  }
  app.installTranslator(translator);

  // Check if setup is needed
  if (!settings.hasGroupCode()) {
    SetupDialog dialog;
    if (dialog.exec() == QDialog::Accepted) {
      settings.setGroupCode(dialog.groupCode());
      settings.save();
    } else {
      return 0;
    }
  }

  MainWindow window;
  window.setTranslator(translator);
  window.show();
  window.initialize();

  return app.exec();
}
