#include "renderer.h"
#include "ui_renderer.h"

#define _WORKING_WITH_FILES_

Renderer::Renderer(VideoCapture *capturer, int *_brightestPixls, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Renderer),
    capWebCam(capturer),
    serial(NULL),
    iterator(0),
    brightestPixls(_brightestPixls),
    timer(this)
{
    ui->setupUi(this);
    ui->openGLWidget->setPointCloud(&points);
    initSerial();               //Inicializar la comunicacion serial
    connect(&timer, SIGNAL(timeout()), this, SLOT(frameBrightestPixels()));
    connect(ui->horizontalSlider, SIGNAL(valueChanged(int)), ui->openGLWidget, SLOT(setRotX(int)));
#ifndef _WORKING_WITH_FILES_
    connect(this, SIGNAL(finishedPixelCalculation()), this, SLOT(processSlice()));
#endif
}

Renderer::~Renderer()
{
    //Cerrar el puerto serial
    if(serial->isOpen())
        serial->close();

    delete ui;
}

void Renderer::setFrameSize(int _w, int _h)
{
    width = _w;
    height = _h;
    middle_x = _w / 2;

    r_x0 = 0;
    r_y0 = 0;
    r_xf = width;
    r_yf = height;

    points.setHeight(height);
}

void Renderer::setAngles(float _laserAngle, float _stepAngle)
{
    laserAngle = _laserAngle * 3.1416 / 180;
    stepAngle = _stepAngle * 3.1416 / 180;
}

void Renderer::setRange(int x0, int y0, int xf, int yf)
{
    qDebug() << "Setting: " << x0 << y0 << xf << yf;
    r_x0 = x0;
    r_y0 = y0;
    r_xf = xf;
    r_yf = yf;
}

void Renderer::initSerial()
{
    serial = new QSerialPort;

    // Initialize Serial
    serial->setPortName("/dev/ttyUSB0");
    if(serial->open(QIODevice::WriteOnly)) {
        qDebug() << "SERIAL: port opened!";
        serial->setBaudRate(QSerialPort::Baud9600);
        serial->setDataBits(QSerialPort::Data8);
        serial->setParity(QSerialPort::NoParity);
        serial->setStopBits(QSerialPort::OneStop);
        serial->setFlowControl(QSerialPort::NoFlowControl);
    }
    else
        qDebug() << "SERIAL: Couldn't find available serial port!" << endl;
}

void Renderer::captureFrames(int n, int rate) {
    n_frames = n;
    points.setFrames(n_frames);
    iterator = 0;
    qDebug() << "Will capture" << n_frames << "frames at" << rate << "ms";
    qDebug() << "Range: (" << r_x0 << ", " << r_y0 << ") -> (" << r_xf << ", " << r_yf << ")";
#ifdef _WORKING_WITH_FILES_
    frameBrightestPixels_();
#endif
    timer.start(rate);

    //Send command to arduino to start rotating
    if (serial->isOpen() && serial->isWritable()) {
        serial->write(QByteArray("s"));
//        serial->flush();
        qDebug() << "ROTATION: Data sent!" << endl;
    }
    else
        qDebug() << "ROTATION: Couldn't send command!" << endl;
}

void Renderer::frameBrightestPixels()
{
    //Obtener imagen desde OpenCV
    (*capWebCam) >> transformationMat;

    threshold(transformationMat, transformationMat, 240, 255, 3);
    cvtColor(transformationMat, transformationMat, CV_BGR2HLS);    //Imagen en HLS

    //Procesar pixel mas brillante para cada Y
    for (int i=r_y0; i < r_yf; i++) {
        brightestPixls[i] = 0;
        for(int j=r_x0; j < r_xf; j++)
            if(transformationMat.at<cv::Vec3b>(i,j)[1] >= transformationMat.at<cv::Vec3b>(i,brightestPixls[i])[1])
                brightestPixls[i] = j;
    }

    emit finishedPixelCalculation();
}

void Renderer::frameBrightestPixels_()
{
    for(int it = 5; it < 72; ++it) {
    //Obtener imagen desde OpenCV
    Mat currentMat,
        transformationMat;
    QString path = "output/scan" + QString::number(it) + ".jpg"; /////////Debug
//    QString path = "output/scan40.jpg"; /////////Debug
    currentMat = imread(path.toStdString(), CV_LOAD_IMAGE_COLOR);

    transformationMat = imread(path.toStdString(), CV_LOAD_IMAGE_COLOR);

    threshold(transformationMat, transformationMat, 240, 255, 3);
    cvtColor(transformationMat, transformationMat, CV_BGR2HLS);    //Imagen en HLS

    //Procesar pixel mas brillante para cada Y
    for (int i=r_y0; i < r_yf; i++) {
        brightestPixls[i] = 0;
        for(int j=r_x0; j < r_xf; j++)
            if(transformationMat.at<cv::Vec3b>(i,j)[1] >= transformationMat.at<cv::Vec3b>(i,brightestPixls[i])[1])
                brightestPixls[i] = j;
    }

    //Imagen en RGB
//    cvtColor(transformationMat, currentMat, CV_HLS2RGB);
//    cvtColor(currentMat, currentMat, CV_BGR2RGB);

//    //Load image using QImage
//    QImage img = QImage((uchar*) currentMat.data,
//                        currentMat.cols,
//                        currentMat.rows,
//                        currentMat.step,
//                        QImage::Format_RGB888);

//    //Escribir sobre pixels mas brillantes
//    for (int i=0; i < height ; i++)
//        img.setPixel(brightestPixls[i], i, qRgb(0, 0, 255));

//    //Mandar imagen a label: lbl_Pic
//    ui->lbl_Pic->setPixmap(QPixmap::fromImage(img));

    processSlice();
    }
    points.meshify();
}

void Renderer::processSlice()
{
    //Stop timer
    if(iterator >= n_frames) {
        timer.stop();
        points.meshify();                   ///Esto es lo que añadimos
        qDebug() << "Timer was stopped";
    }

    //Calculate position in space
    float dAngle = stepAngle * iterator++,
          dx, dz, x, z;
    for (int i=0; i < height ; i++) {
        dx = brightestPixls[i] - middle_x;
        dz = dx / sin(laserAngle);

        x = dz * cos(dAngle);
        z = dz * sin(dAngle);

//        points.push_back(x, height - i * 1.0, -z);
        points.push_back(x, height - i * 1.0, z);      ////TEST
    }

#ifndef _WORKING_WITH_FILES_
    ui->openGLWidget->update();
#endif

    emit finishedSliceProcessing();
}

void Renderer::turnOnLaser()
{
    //Send command to arduino to turn on laser
    if (serial->isOpen() && serial->isWritable()) {
        serial->write(QByteArray("r%1"));
        serial->flush();
    }
    else
        qDebug() << "LASER: Couldn't turn on laser!" << endl;
}
