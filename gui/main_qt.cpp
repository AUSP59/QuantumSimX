// SPDX-License-Identifier: MIT

#ifdef QSX_GUI_QT
#include <QApplication>
#include "main_window.hpp"
int main(int argc, char** argv){
  QApplication app(argc, argv);
  MainWindow w; w.show();
  return app.exec();
}
#else
int main(){return 0;}
#endif
