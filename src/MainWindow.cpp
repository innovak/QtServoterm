/*
* This file is part of the stmbl project.
*
* Copyright (C) 2020 Forest Darling <fdarling@gmail.com>
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <QtWidgets>
#include <QToolBar>
#include <QComboBox>
#include <QSettings>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QChartView>
#include <QValueAxis>
#include <QLineSeries>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QDebug>

#include "MainWindow.h"
#include "ScopeDataDemux.h"

QT_CHARTS_USE_NAMESPACE

namespace STMBL_Servoterm {

static const int SAMPLE_WINDOW_LENGTH = 200;
static const quint16 STMBL_USB_VENDOR_ID  = 0x0483; //  1155
static const quint16 STMBL_USB_PRODUCT_ID = 0x5740; // 22336

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    _portList(new QComboBox),
    _connectButton(new QPushButton("Connect")),
    _disconnectButton(new QPushButton("Disconnect")),
    _clearButton(new QPushButton("Clear")),
    _resetButton(new QPushButton("Reset")),
    _chartView(new QChartView),
    _chart(new QChart),
    // _chartData(),
    _chartRollingLine(new QLineSeries),
    _textLog(new QTextEdit),
    _lineEdit(new QLineEdit),
    _sendButton(new QPushButton("Send")),
    _serialPort(new QSerialPort(this)),
    _settings(nullptr),
    _demux(new ScopeDataDemux(this)),
    _scopeX(0)
{
    _settings = new QSettings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    _textLog->setReadOnly(true);
    setAcceptDrops(true);

    // set up chart
    _chart->setMinimumSize(600, 256);
    {
        QValueAxis * const axisX = new QValueAxis;
        axisX->setRange(0, SAMPLE_WINDOW_LENGTH);
        axisX->setLabelFormat("%g");
        // axisX->setTitleText("Sample");
        axisX->setVisible(false);

        QValueAxis * const axisY = new QValueAxis;
        axisY->setRange(-1, 1);
        // axisY->setTitleText("Value");
        axisY->setVisible(false);

        _chart->addAxis(axisX, Qt::AlignBottom);
        _chart->addAxis(axisY, Qt::AlignLeft);
        for (int i = 0; i < SCOPE_CHANNEL_COUNT; i++)
        {
            QLineSeries * const series = new QLineSeries;
            _chartData[i] = series;
            _chart->addSeries(series);
            series->attachAxis(axisX);
            series->attachAxis(axisY);
        }

        // set up the visual indicator of where the new data is
        // overwriting the old in the rolling oscilloscope view
        _chart->addSeries(_chartRollingLine);
        _chartRollingLine->attachAxis(axisX);
        _chartRollingLine->attachAxis(axisY);
    }
    _chart->legend()->hide();
    _chart->setTitle("Oscilloscope");
    _chartView->setChart(_chart);

    setWindowTitle(QCoreApplication::applicationName());
    {
        QToolBar * const toolbar = new QToolBar;
        toolbar->setObjectName("ConnectionToolBar");
        // toolbar->addWidget(new QPushButton("Refresh"));
        toolbar->addWidget(_portList);
        toolbar->addWidget(_connectButton);
        toolbar->addWidget(_disconnectButton);
        toolbar->addSeparator();
        toolbar->addWidget(_clearButton);
        toolbar->addWidget(_resetButton);
        addToolBar(toolbar);
    }
    {
        QWidget * const dummy = new QWidget;
        QVBoxLayout * const vbox = new QVBoxLayout(dummy);
        vbox->addWidget(_chartView);
        vbox->addWidget(_textLog);
        {
            QHBoxLayout * const hbox = new QHBoxLayout;
            hbox->addWidget(_lineEdit);
            hbox->addWidget(_sendButton);
            vbox->addLayout(hbox);
        }
        setCentralWidget(dummy);
    }

    connect(_portList, SIGNAL(currentIndexChanged(const QString &)), this, SLOT(slot_UpdateButtons()));
    connect(_connectButton, SIGNAL(clicked()), this, SLOT(slot_ConnectClicked()));
    connect(_disconnectButton, SIGNAL(clicked()), this, SLOT(slot_DisconnectClicked()));
    connect(_clearButton, SIGNAL(clicked()), _textLog, SLOT(clear()));
    connect(_resetButton, SIGNAL(clicked()), this, SLOT(slot_ResetClicked()));
    connect(_lineEdit, SIGNAL(textChanged(const QString &)), this, SLOT(slot_UpdateButtons()));
    connect(_lineEdit, SIGNAL(returnPressed()), _sendButton, SLOT(click()));
    connect(_sendButton, SIGNAL(clicked()), this, SLOT(slot_SendClicked()));
    connect(_serialPort, SIGNAL(readyRead()), this, SLOT(slot_SerialDataReceived()));
    // connect(_serialPort, SIGNAL(readChannelFinished()), this, SLOT(slot_SerialPortClosed())); // NOTE: doesn't seem to actually work
    connect(_demux, SIGNAL(scopePacketReceived(const QVector<float> &)), this, SLOT(slot_ScopePacketReceived(const QVector<float> &)));
    connect(_demux, SIGNAL(scopeResetReceived()), this, SLOT(slot_ScopeResetReceived()));
    connect(_textLog, SIGNAL(textChanged()), this, SLOT(slot_UpdateButtons()));
    slot_UpdateButtons();

    _RepopulateDeviceList();
    _loadSettings();
}

MainWindow::~MainWindow()
{
}

void MainWindow::slot_ConnectClicked()
{
    if (_serialPort->isOpen())
    {
        QMessageBox::critical(this, "Error opening serial port", "Already connected! Currently open port is: \"" + _serialPort->portName() + "\"");
        return;
    }
    const QString portName = _portList->currentText();
    if (portName.isEmpty())
    {
        QMessageBox::critical(this, "Error opening serial port", "No port selected!");
        return;
    }
    _serialPort->setPortName(portName);
    if (!_serialPort->open(QIODevice::ReadWrite))
    {
        QMessageBox::critical(this, "Error opening serial port", "Unable to open port \"" + portName + "\"");
        return;
    }
    _textLog->append("<font color=\"FireBrick\">connected</font><br/>");
    slot_UpdateButtons();
}

void MainWindow::slot_DisconnectClicked()
{
    if (!_serialPort->isOpen())
    {
        QMessageBox::critical(this, "Error closing serial port", "Already disconnected!");
        return;
    }
    _serialPort->close();
    if (_serialPort->isOpen())
    {
        QMessageBox::critical(this, "Error closing serial port", "Unknown reason -- it is open, but cannot be closed?");
        return;
    }
    _textLog->append("<font color=\"FireBrick\">disconnected</font><br/>");
    slot_UpdateButtons();
}

void MainWindow::slot_ResetClicked()
{
    if (!_serialPort->isOpen())
    {
        QMessageBox::warning(this, "Error sending reset commands", "Serial port not open!");
        return;
    }
    _serialPort->write(QString("fault0.en = 0\n").toLatin1());
    _serialPort->write(QString("fault0.en = 1\n").toLatin1());
}

void MainWindow::slot_SendClicked()
{
    if (!_serialPort->isOpen())
    {
        QMessageBox::warning(this, "Error sending command", "Serial port not open!");
        return;
    }
    const QString line = _lineEdit->text();
    _lineEdit->clear();
    _serialPort->write((line + "\n").toLatin1()); // TODO perhaps have more intelligent Unicode conversion?
}

void MainWindow::slot_SerialDataReceived()
{
    const QByteArray buf = _serialPort->readAll();
    QString txt = _demux->addData(buf);
    if (!txt.isEmpty())
    {
        // some serious ugliness to work around a bug
        // where QTextEdit (or QTextDocument?) effectively
        // deletes trailing HTML <br/>'s, so we must use
        // plain text newlines to trick it
        const QStringList lines = txt.split("\n", QString::KeepEmptyParts);
        for (QStringList::const_iterator it = lines.begin(); it != lines.end(); ++it)
        {
            _textLog->moveCursor(QTextCursor::End);
            if (it != lines.begin())
            {
                _textLog->insertPlainText("\n");
                _textLog->moveCursor(QTextCursor::End);
            }
            if (!it->isEmpty())
            {
                _textLog->insertHtml(*it);
                _textLog->moveCursor(QTextCursor::End);
            }
        }
    }
}

/*void MainWindow::slot_SerialPortClosed()
{
}*/

void MainWindow::slot_ScopePacketReceived(const QVector<float> &packet)
{
    //Zerocross detection
    /*if(((trigger_last < 0.01 && values[trigger_wave] > 0) || (trigger_last < 0 && values[trigger_wave] > 0.01)) && !trigger_zerocross){
        trigger_zerocross = true;
    }
        trigger_last = values[trigger_wave];

        //Only plot if triggrd
    if((trigger_enabled && trigger_wait && (values[trigger_wave] >= trigger_lvl) && trigger_zerocross) || (trigger_enabled && !trigger_wait)){
        trigger_buttonstate = 2;
        trigger_wait = false;

        plot(values);
    }else if (!trigger_enabled) {  //rolling plot if trigger is disabled*/
        // plot(values);

    /*}*/

    ///////////

    // plot the samples
    for (int channel = 0; channel < packet.size() && channel < SCOPE_CHANNEL_COUNT; channel++)
    {
        QLineSeries * const series = _chartData[channel];
        const float yValue = packet[channel];
        if (series->count() < SAMPLE_WINDOW_LENGTH)
            series->append(_scopeX, yValue);
        else
            series->replace(_scopeX, _scopeX, yValue);
    }
    // advance the sample index and roll around if necessary
    _scopeX++;
    if (_scopeX >= SAMPLE_WINDOW_LENGTH)
        _scopeX = 0;
    // update the incoming data vertical line indicator
    {
        QVector<QPointF> verticalLine;
        verticalLine.append(QPointF(_scopeX, -1.0));
        verticalLine.append(QPointF(_scopeX,  1.0));
        _chartRollingLine->replace(verticalLine);
    }
}

void MainWindow::slot_ScopeResetReceived()
{
    _scopeX = 0; // TODO
}

void MainWindow::slot_UpdateButtons()
{
    const bool portSelected = !_portList->currentText().isEmpty();
    const bool portOpen = _serialPort->isOpen();
    const bool hasCommand = !_lineEdit->text().isEmpty();
    _connectButton->setEnabled(!portOpen && portSelected);
    _disconnectButton->setEnabled(portOpen);
    _clearButton->setEnabled(!_textLog->document()->isEmpty());
    _resetButton->setEnabled(portOpen);
    _sendButton->setEnabled(portOpen && hasCommand);
}

/*void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    event->acceptProposedAction();
}

void MainWindow::dragMoveEvent(QDragMoveEvent *event)
{
    event->acceptProposedAction();
}

void MainWindow::dragLeaveEvent(QDragLeaveEvent *event)
{
    event->accept();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    const QMimeData* mimeData = event->mimeData();

    // check for our needed mime type, here a file or a list of files
    if (mimeData->hasUrls())
    {
        QStringList pathList;
        QList<QUrl> urlList = mimeData->urls();

        // extract the local paths of the files
        for (int i = 0; i < urlList.size() && i < 32; ++i)
        {
            pathList.append(urlList.at(i).toLocalFile());
        }

        // call a function to open the files
        qDebug() << pathList;
        // openFiles(pathList);
    }
}*/

void MainWindow::closeEvent(QCloseEvent *event)
{
    _saveSettings();
    QMainWindow::closeEvent(event);
}

void MainWindow::_RepopulateDeviceList()
{
    _portList->clear();
    QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
    for (QList<QSerialPortInfo>::const_iterator it = ports.begin(); it != ports.end(); ++it)
    {
        if (it->manufacturer().contains("STMicroelectronics")
         || it->description().contains("STMBL")
         || (it->vendorIdentifier() == STMBL_USB_VENDOR_ID
          && it->productIdentifier()== STMBL_USB_PRODUCT_ID))
        {
            _portList->addItem(it->portName());
        }
    }
}

void MainWindow::_saveSettings()
{
    _settings->beginGroup("MainWindow");
    _settings->setValue("geometry", saveGeometry());
    _settings->setValue("windowState", saveState());
    _settings->endGroup();
}

void MainWindow::_loadSettings()
{
    _settings->beginGroup("MainWindow");
    restoreGeometry(_settings->value("geometry").toByteArray());
    restoreState(_settings->value("windowState").toByteArray());
    _settings->endGroup();
}

} // namespace STMBL_Servoterm
