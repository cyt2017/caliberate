#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);


    ui->lineEdit_boardX->setText(QString::number(8));
    ui->lineEdit_boardY->setText(QString::number(6));
    ui->lineEdit_boardSize->setText("108.0");

    connect(ui->startBtn,SIGNAL(clicked(bool)),this,SLOT(onStartBtn()));


}




MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::qualifiedImage()
{

    QString path = "./img/";
    QDir dir(path);//!根据放图片的文件夹遍历图片
    ui->textEdit->append(QString::fromLocal8Bit("正在进入目录：./img/\n"));
    if(!dir.exists())
    {
        printf("dir empty...\n");
        return ;
    }

    vecMat.clear();//!清空缓存图像
    image_points_seq.clear();//!清空缓存 亚像素焦点检测到的关键点
    image_size = Size(0,0);//!初始化图像大小
    boardX = ui->lineEdit_boardX->text().toInt();//!从界面获取棋盘格的横向黑格数和纵向黑格数
    boardY = ui->lineEdit_boardY->text().toInt();
    board_size = Size(boardX,boardY);//!设置棋盘格的格子数目
    boardSize = ui->lineEdit_boardSize->text().toFloat();

    dir.setFilter(QDir::Files | QDir::NoSymLinks);
    QFileInfoList imgList = dir.entryInfoList();

    vector<Point2f> image_points_buf;

    //!从文件夹遍历图片
    for(int i=0;i<imgList.count();i++)
    {
        //!防止卡顿 设置响应事件的时间
        QCoreApplication::processEvents(QEventLoop::AllEvents,10);
        fflush(NULL);
        QString imgName = imgList[i].absoluteFilePath();
        ui->textEdit->append(QString::fromLocal8Bit("正在处理"));
        ui->textEdit->append(imgName);
        Mat src = imread(imgName.toStdString());
        if(src.empty())
        {
            printf("src.empty...\n");
            continue;
        }

        //!把从文件读取的图像进行opencv中的棋盘格角点检测函数（findChessboardCorners），
        //!如果检测不到角点返回NULL
        if(0 == findChessboardCorners(src,board_size,image_points_buf))
        {
            printf("no corners\n");
            continue;
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents,10);
        fflush(NULL);

        //!如果是输入的第一张图像，将图像的大小设置成第一张图像的大小，
        //!使之后的图像的大小和第一张图像保持一致
        if (image_size.width == 0)
        {
            image_size.width = src.cols;
            image_size.height =src.rows;
        }
        else
        {
            if(image_size.width != src.cols || image_size.height !=src.rows)
            {
                printf("image_size error...\n");
                exit(1);
            }
        }
        //!把能通过棋盘格角点检测的图像保存到缓存中
        vecMat.push_back(src);
        Mat view_gray;
        cvtColor(src,view_gray,CV_RGB2GRAY);//!图像灰度处理
        //!为了找到精确度更高的角点位置，使用opencv的亚像素级焦点检测，
        //!针对灰度图像把获取的角点保存起来
        find4QuadCornerSubpix(view_gray,image_points_buf,Size(5,5));
        image_points_seq.push_back(image_points_buf);
        QCoreApplication::processEvents(QEventLoop::AllEvents,10);
        fflush(NULL);
    }
}

void MainWindow::calibrate()
{//!进行标定
    //!设置方格大小
    Size square_size = Size(boardSize,boardSize);
    vector<vector<Point3f> > object_points;
    //!单目摄像头的内参数矩阵
    Mat cameraMatrix=Mat(3,3,CV_32FC1,Scalar::all(0));
    vector<int> point_counts;
    //!单目摄像头的畸变矩阵
    Mat distCoeffs=Mat(1,5,CV_32FC1,Scalar::all(0));
    vector<Mat> tvecsMat;//!位移向量
    vector<Mat> rvecsMat;//!旋转向量

    ui->textEdit->append(QString::fromLocal8Bit("正在开始标定"));
    QCoreApplication::processEvents(QEventLoop::AllEvents,10);
    fflush(NULL);

    int image_count = vecMat.size();

    int i,j,t;
    for (t=0;t<image_count;t++)
    {
        vector<Point3f> tempPointSet;
        for (i=0;i<board_size.height;i++)
        {
            for (j=0;j<board_size.width;j++)
            {
                Point3f realPoint;
                realPoint.x = i*square_size.width;
                realPoint.y = j*square_size.height;
                realPoint.z = 0;
                tempPointSet.push_back(realPoint);
            }
        }
        //!世界坐标系中的点
        object_points.push_back(tempPointSet);
    }

    for (i=0;i<image_count;i++)
    {
        point_counts.push_back(board_size.width*board_size.height);
    }
    //!相机标定
    //!根据世界坐标系中的点和通过opencv亚像素查找到的点，
    //!图像的大小，以及相机的内参和畸变参数获得平移和旋转变量
    calibrateCamera(object_points,image_points_seq,image_size,cameraMatrix,distCoeffs,rvecsMat,tvecsMat,0);
    cout<<"Calibrate………………done\n";
    ui->textEdit->append(QString::fromLocal8Bit("标定结束，正在计算误差..."));
    QCoreApplication::processEvents(QEventLoop::AllEvents,10);
    fflush(NULL);
    cout<<"score precision………………\n";
    double total_err = 0.0;
    double err = 0.0;
    vector<Point2f> image_points2;
//    cout<<"\t每幅图像的标定误差：\n";
////    fout<<"每幅图像的标定误差：\n";
    for (i=0;i<image_count;i++)
    {
        vector<Point3f> tempPointSet=object_points[i];

        //!将世界坐标系中的点经过之前得到的旋转向量、平移向量、
        //!相机内参和畸变参数，投影形成一张新图像(作用：可以使用该函数得到新的角点图像，
        //!然后拿新图像与亚像素角点检测到的点进行误差运算)
        projectPoints(tempPointSet,rvecsMat[i],tvecsMat[i],cameraMatrix,distCoeffs,image_points2);

        //!得到亚像素图像中的某张图像中的角点
        vector<Point2f> tempImagePoint = image_points_seq[i];

        //!创建亚像素角点图像和世界坐标系中转换来的角点图像
        Mat tempImagePointMat = Mat(1,tempImagePoint.size(),CV_32FC2);
        Mat image_points2Mat = Mat(1,image_points2.size(), CV_32FC2);

        //!给以上创建的图像进行赋值操作
        for (int j = 0 ; j < tempImagePoint.size(); j++)
        {
            image_points2Mat.at<Vec2f>(0,j) = Vec2f(image_points2[j].x, image_points2[j].y);
            tempImagePointMat.at<Vec2f>(0,j) = Vec2f(tempImagePoint[j].x, tempImagePoint[j].y);
        }

        //!通过亚像素图像和新角点图像计算误差
        err = norm(image_points2Mat, tempImagePointMat, NORM_L2);
        total_err += err/=  point_counts[i];
//        std::cout<<"第"<<i+1<<"幅图像的平均误差："<<err<<"像素"<<endl;
//        fout<<"第"<<i+1<<"幅图像的平均误差："<<err<<"像素"<<endl;
    }
    std::cout<<"total_err:"<<total_err/image_count<<"pix"<<endl;
//    fout<<"total_err:"<<total_err/image_count<<"pix"<<endl;
    ui->textEdit->append(QString::fromLocal8Bit("误差:")+QString::number(total_err/image_count)+"pix");
    std::cout<<"finish!"<<endl;
    ui->textEdit->append(QString::fromLocal8Bit("标定完成:"));
    QString str;
    int ii = CV_ELEM_SIZE1(DataType<float>::depth);
    int jj = cameraMatrix.elemSize1();
    printf("ii:%d,jj:%d\n",ii,jj);
    std::cout<<cameraMatrix;
    fflush(NULL);


    //!将相机内参和畸变参数打印出来
    str.sprintf("cameraMatrix:\n%f,%f,%f\n%f,%f,%f\n%f,%f,%f\n",\
                 cameraMatrix.at<double>(0,0),cameraMatrix.at<double>(0,1),cameraMatrix.at<double>(0,2),\
                  cameraMatrix.at<double>(1,0),cameraMatrix.at<double>(1,1),cameraMatrix.at<double>(1,2),\
                  cameraMatrix.at<double>(2,0),cameraMatrix.at<double>(2,1),cameraMatrix.at<double>(2,2));
    QCoreApplication::processEvents(QEventLoop::AllEvents,10);
    fflush(NULL);
    ui->textEdit->append(str);
    str.sprintf("distCoeffs:\n%f,%f,%f,%f,%f\n",\
                 distCoeffs.at<double>(0,0),distCoeffs.at<double>(0,1),distCoeffs.at<double>(0,2),\
                  distCoeffs.at<double>(0,3),distCoeffs.at<double>(0,4));

    ui->textEdit->append(str);
    QCoreApplication::processEvents(QEventLoop::AllEvents,10);
    fflush(NULL);
}

void MainWindow::onStartBtn()
{
    qualifiedImage();
    calibrate();
}
