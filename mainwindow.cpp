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
    initializeDb();
    initializeTcpServer();

    connect(ui->getGnssDataBtn, &QPushButton::clicked, this, &MainWindow::getGnssData);
    connect(ui->clearGnssDataBtn, &QPushButton::clicked, this, &MainWindow::clearGnssTable);
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

void MainWindow::initializeDb()
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("gnss.db");

    if (!db.open()) {
        qDebug() << "Error al abrir la base de datos:" << db.lastError().text();
        return;
    } else {
        qDebug() << "Base de datos abierta correctamente.";

        // Crear tabla si no existe
        QSqlQuery query;
        if (!query.exec("CREATE TABLE IF NOT EXISTS gnss_data ("
                        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                        "date TEXT, "
                        "time TEXT, "
                        "raw_line TEXT)")) {
            qDebug() << "Error creando tabla:" << query.lastError().text();
        }

        modelGnssData = new QSqlTableModel(this);
        modelGnssData->setTable("gnss_data");
        modelGnssData->select();

        modelGnssData->setHeaderData(modelGnssData->record().indexOf("date"), Qt::Horizontal, tr("Fecha"));
        modelGnssData->setHeaderData(modelGnssData->record().indexOf("time"), Qt::Horizontal, tr("Hora"));
        modelGnssData->setHeaderData(modelGnssData->record().indexOf("raw_line"), Qt::Horizontal, tr("Línea NMEA"));

        ui->gnssDataTable->setModel(modelGnssData);
        ui->gnssDataTable->resizeColumnsToContents();
    }
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

        if (command == "CLEAR") {
            clearGnssTable();
            clientConnection->write("Tabla limpiada\n");
        } else if (command == "READ") {
            getGnssData();
            clientConnection->write("Lectura iniciada\n");
        }

        clientConnection->flush();
    });

    connect(clientConnection, &QTcpSocket::disconnected, clientConnection, &QObject::deleteLater);
}

void MainWindow::getGnssData()
{
    if (!serialPort->isOpen()) {
        qDebug() << "Puerto serial no está abierto.";
        return;
    }

    linesRead = 0;

    while (serialPort->canReadLine() && linesRead < linesToRead) {
        QByteArray rawData = serialPort->readLine();
        QString line = QString::fromUtf8(rawData).trimmed();

        qDebug() << "Línea recibida:" << line;
        saveGnssLine(line);
        sendLineToServer(line);
        linesRead++;
    }

    loadGnssDataToTable();
}

void MainWindow::loadGnssDataToTable()
{
    if (modelGnssData) {
        modelGnssData->select();
    }
}

void MainWindow::saveGnssLine(const QString& line)
{
    QDateTime now = QDateTime::currentDateTime();

    QSqlQuery query;
    query.prepare("INSERT INTO gnss_data (date, time, raw_line) VALUES (?, ?, ?)");
    query.addBindValue(now.date().toString("yyyy-MM-dd"));
    query.addBindValue(now.time().toString("HH:mm:ss"));
    query.addBindValue(line);

    if (!query.exec())
        qDebug() << "Error guardando línea NMEA:" << query.lastError().text();
    else
        qDebug() << "Línea GNSS guardada:" << line;
}

void MainWindow::clearGnssTable()
{
    QSqlQuery query;
    if (!query.exec("DELETE FROM gnss_data")) {
        qDebug() << "Error al limpiar la tabla:" << query.lastError().text();
    } else {
        qDebug() << "Tabla gnss_data limpiada correctamente.";
    }

    loadGnssDataToTable();
}

void MainWindow::sendLineToServer(const QString& line)
{
    QUrl url("http://192.168.0.177:1234/api/gnss");
    QNetworkRequest request(url);

    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QByteArray payload = QString(R"({"line": "%1"})").arg(line).toUtf8();

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
