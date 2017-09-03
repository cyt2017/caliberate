#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <opencv2/opencv.hpp>
#include <iostream>
#include <fstream>
#include <QFile>
#include <QDebug>
#include <QDir>

using namespace std;
using namespace cv;

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    void qualifiedImage();
    void calibrate();

protected slots:
    void onStartBtn();

private:
    Ui::MainWindow *ui;
    int boardX;
    int boardY;
    float boardSize;
    Size board_size;
    Size image_size;

    vector<Mat> vecMat;
    vector<vector<Point2f> > image_points_seq;
};

#endif // MAINWINDOW_H
