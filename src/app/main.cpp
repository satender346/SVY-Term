#include "app/Application.h"

#include <QCoreApplication>

int main(int argc, char** argv) {
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    svy::app::Application app(argc, argv);
    return app.run();
}
