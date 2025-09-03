// SPDX-License-Identifier: MIT

#include "main_window.hpp"
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget* parent): QMainWindow(parent){
  auto* central = new QWidget(this);
  auto* layout = new QVBoxLayout(central);
  auto* btn = new QPushButton("Hello QUANTUM-SIMX", central);
  layout->addWidget(btn);
  setCentralWidget(central);
}
