#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlTableModel>
#include <QSqlRecord>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QDateTime>
#include <QCoreApplication>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    networkManager = new QNetworkAccessManager(this);

    initializeUartb();
    initializeTcpServer();

    connect(ui->refreshDataBtn, &QPushButton::clicked, this, &MainWindow::getGNGGAData);
}

MainWindow::~MainWindow()
{
    if (serialPort) {
        serialPort->flush();
        serialPort->clear();
        serialPort->close();
    }

    delete ui;
}

void MainWindow::initializeUartb()
{
    serialPort = new QSerialPort(this);

    serialPort->setPortName("/dev/colibri-uartb");

    serialPort->setBaudRate(QSerialPort::Baud9600);
    serialPort->setDataBits(QSerialPort::Data8);
    serialPort->setParity(QSerialPort::NoParity);
    serialPort->setStopBits(QSerialPort::OneStop);
    serialPort->setFlowControl(QSerialPort::NoFlowControl);

    if (!serialPort->open(QIODevice::ReadOnly)) {
        qDebug() << "No se pudo abrir el puerto serial:" << serialPort->errorString();
        return;
    } else {
        qDebug() << "Puerto Uart B abierto correctamente";
    }
}

void MainWindow::initializeTcpServer()
{
    tcpServer = new QTcpServer(this);

    connect(tcpServer, &QTcpServer::newConnection, this, &MainWindow::handleNewConnection);

    if (!tcpServer->listen(QHostAddress::Any, 12345)) {
        qDebug() << "No se pudo iniciar el servidor TCP:" << tcpServer->errorString();
    } else {
        qDebug() << "Servidor TCP escuchando en el puerto 12345";
    }
}

void MainWindow::handleNewConnection()
{
    clientConnection = tcpServer->nextPendingConnection();

    connect(clientConnection, &QTcpSocket::readyRead, this, [this]() {
        QByteArray data = clientConnection->readAll();
        QString command = QString::fromUtf8(data).trimmed();

        qDebug() << "Comando recibido por TCP:" << command;

        if (command == "REFRESH") {
            getGNGGAData();
            clientConnection->write("Refrescando...\n");
        }

        clientConnection->flush();
    });

    connect(clientConnection, &QTcpSocket::disconnected, clientConnection, &QObject::deleteLater);
}

void MainWindow::getGNGGAData()
{
    if (!serialPort->isOpen()) {
        qDebug() << "Puerto serial no está abierto.";
        return;
    }

    while (serialPort->canReadLine()) {
        QByteArray rawData = serialPort->readLine();
        QString line = QString::fromUtf8(rawData).trimmed();

        if (line.startsWith("$GNGGA")) {
            MainWindow::GNGGAData data = parseGNGGALine(line);

            updateUi(data);
            sendDataToServer(data);

            return;
        }
    }
}

void MainWindow::updateUi(const GNGGAData& data)
{
    ui->latitudeInput->setText(QString::number(data.latitude, 'f', 6));
    ui->longitudeInput->setText(QString::number(data.longitude, 'f', 6));
    ui->satelitesInput->setText(QString::number(data.satellites, 'f', 6));
    ui->altitudeInput->setText(QString::number(data.altitude, 'f', 6));
}

double convertNmeaToDecimal(const QString& nmeaCoord, const QString& direction) {
    if (nmeaCoord.isEmpty() || direction.isEmpty()) return 0.0;

    bool ok = false;
    double raw = nmeaCoord.toDouble(&ok);
    if (!ok) return 0.0;

    int degrees = static_cast<int>(raw / 100);
    double minutes = raw - (degrees * 100);

    double decimal = degrees + minutes / 60.0;

    if (direction == "S" || direction == "W") decimal = -decimal;

    return decimal;
}

MainWindow::GNGGAData MainWindow::parseGNGGALine(const QString& line) {
    MainWindow::GNGGAData data;

    QStringList parts = line.split(',');

    if (parts.size() < 15) return data;

    int fixQuality = parts[6].toInt();
    if (fixQuality == 0) {
        data.validFix = false;
        return data;
    }

    data.validFix = true;

    data.latitude = convertNmeaToDecimal(parts[2], parts[3]);
    data.longitude = convertNmeaToDecimal(parts[4], parts[5]);

    data.satellites = parts[7].toInt();

    data.altitude = parts[9].toDouble();

    return data;
}

void MainWindow::sendDataToServer(const GNGGAData& data)
{
    QUrl url("http://192.168.0.177:1234/api/gnss");
    QNetworkRequest request(url);

    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QString jsonString = QString(
                             R"({
            "latitude": "%1",
            "longitude": "%2",
            "satelites": "%3",
            "altitude": "%4"
        })")
        .arg(data.latitude)
        .arg(data.longitude)
        .arg(data.satellites)
        .arg(data.altitude);

    QByteArray payload = jsonString.toUtf8();

    QNetworkReply* reply = networkManager->post(request, payload);

    connect(reply, &QNetworkReply::finished, this, [reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            qDebug() << "Respuesta del servidor:" << reply->readAll();
        } else {
            qDebug() << "Error en la petición HTTP:" << reply->errorString();
        }
        reply->deleteLater();
    });
}
