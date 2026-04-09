#include "dialogs/mainwindow.h"

#include <QApplication>
#include <QLocale>
#include <QTranslator>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;

    QStringList files;
    for (int i = 1; i < argc; ++i) {
        QString filePath = QString::fromLocal8Bit(argv[i]);
        if (QFile::exists(filePath)) {
            files << filePath;
        }
    }

    if (!files.isEmpty()) {
        w.openFiles(files);
    }

    w.show();
    return a.exec();
}
