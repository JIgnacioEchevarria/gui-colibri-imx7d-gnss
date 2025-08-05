#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSerialPort>
#include <QTcpServer>
#include <QTcpSocket>
#include <QNetworkAccessManager>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    struct GNGGAData {
        double latitude = 0.0;
        double longitude = 0.0;
        double altitude = 0.0;
        int satellites = 0;
        bool validFix = false;
    };

private slots:
    void getGNGGAData();

private:
    Ui::MainWindow *ui;
    QSerialPort *serialPort;
    QTcpServer *tcpServer = nullptr;
    QTcpSocket *clientConnection = nullptr;
    QNetworkAccessManager* networkManager;
    void initializeUartb();
    void initializeTcpServer();
    void handleNewConnection();
    double convertNmeaToDecimal(const QString& nmeaCoord, const QString& direction);
    GNGGAData parseGNGGALine(const QString& line);
    void updateUi(const GNGGAData& data);
    void sendDataToServer(const GNGGAData& data);
};

#endif // MAINWINDOW_H
