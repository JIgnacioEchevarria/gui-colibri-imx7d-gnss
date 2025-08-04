#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSerialPort>
#include <QSqlTableModel>
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

private slots:
    void getGnssData();
    void clearGnssTable();

private:
    Ui::MainWindow *ui;
    QSerialPort *serialPort;
    QSqlTableModel *modelGnssData;
    QTcpServer *tcpServer = nullptr;
    QTcpSocket *clientConnection = nullptr;
    QNetworkAccessManager* networkManager;
    void initializeDb();
    void initializeUartb();
    void initializeTcpServer();
    void saveGnssLine(const QString& line);
    void sendLineToServer(const QString& line);
    void loadGnssDataToTable();
    void handleNewConnection();
    int linesToRead = 20;
    int linesRead = 0;
};

#endif // MAINWINDOW_H
