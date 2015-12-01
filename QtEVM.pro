#-------------------------------------------------
#
# Project created by QtCreator 2014-01-07T13:31:23
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = excutable
TEMPLATE = app

SOURCES += main.cpp\
        mainwindow.cpp \
    WindowHelper.cpp \
    VideoProcessor.cpp \
    SpatialFilter.cpp \
    MagnifyDialog.cpp

HEADERS  += mainwindow.h \
    WindowHelper.h \
    VideoProcessor.h \
    SpatialFilter.h \
    MagnifyDialog.h

FORMS    += mainwindow.ui \
    MagnifyDialog.ui

RESOURCES += \
    myResources.qrc

CONFIG += c++11
INCLUDEPATH += "C:\Users\queens1\Desktop\builds\opencv\install\include"
DEPENDPATH += "C:\Users\queens1\Desktop\builds\opencv\install\include"

debug{
LIBS += "C:\Users\queens1\Desktop\builds\opencv\lib\Debug\opencv_core2412d.lib"
LIBS += "C:\Users\queens1\Desktop\builds\opencv\lib\Debug\opencv_core2412d.lib"
LIBS += "C:\Users\queens1\Desktop\builds\opencv\lib\Debug\opencv_highgui2412d.lib"
LIBS += "C:\Users\queens1\Desktop\builds\opencv\lib\Debug\opencv_imgproc2412d.lib"
LIBS += "C:\Users\queens1\Desktop\builds\opencv\lib\Debug\opencv_features2d2412d.lib"
LIBS += "C:\Users\queens1\Desktop\builds\opencv\lib\Debug\opencv_calib3d2412d.lib"
LIBS += "C:\Users\queens1\Desktop\builds\opencv\lib\Debug\opencv_imgproc2412d.lib"
LIBS += "C:\Users\queens1\Desktop\builds\opencv\lib\Debug\opencv_gpu2412d.lib"
LIBS += "C:\Users\queens1\Desktop\builds\opencv\lib\Debug\opencv_legacy2412d.lib"
LIBS += "C:\Users\queens1\Desktop\builds\opencv\lib\Debug\opencv_ml2412d.lib"
LIBS += "C:\Users\queens1\Desktop\builds\opencv\lib\Debug\opencv_objdetect2412d.lib"
LIBS += "C:\Users\queens1\Desktop\builds\opencv\lib\Debug\opencv_ocl2412d.lib"
LIBS += "C:\Users\queens1\Desktop\builds\opencv\lib\Debug\opencv_photo2412d.lib"
LIBS += "C:\Users\queens1\Desktop\builds\opencv\lib\Debug\opencv_stitching2412d.lib"
LIBS += "C:\Users\queens1\Desktop\builds\opencv\lib\Debug\opencv_video2412d.lib"
LIBS += "C:\Users\queens1\Desktop\builds\opencv\lib\Debug\opencv_videostab2412d.lib"
LIBS += "C:\Users\queens1\Desktop\builds\opencv\lib\Debug\opencv_nonfree2412d.lib"
}
