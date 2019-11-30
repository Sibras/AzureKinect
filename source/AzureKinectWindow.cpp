/**
 * Copyright Matthew Oliver
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "AzureKinectWindow.h"

#include <QMessageBox>
#include <QThread>
#include <QTimer>
#include <QtEvents>

using namespace std;

namespace Ak {
AzureKinectWindow::AzureKinectWindow(QWidget* parent) noexcept
    : QMainWindow(parent)
{
    m_ui.setupUi(this);

    // Ensure only 3 digit integers can be entered for PID
    m_validatorPID = new QIntValidator(100L, 999L, m_ui.centralWidget);
    m_ui.lineEditPID->setValidator(m_validatorPID);

    // Connect required signals/slots
    connect(m_ui.buttonStart, &QPushButton::clicked, this, &AzureKinectWindow::startSlot);
    connect(m_ui.actionExit, &QAction::triggered, this, &AzureKinectWindow::exitSlot);
    connect(this, &AzureKinectWindow::errorSignal, this, &AzureKinectWindow::errorSlot);
    connect(this, &AzureKinectWindow::readySignal, this, &AzureKinectWindow::readySlot);
    connect(this, &AzureKinectWindow::imageSignal, m_ui.openGLWidget, &KinectWidget::imageSlot);
    connect(m_ui.openGLWidget, &KinectWidget::errorSignal, this, &AzureKinectWindow::errorSlot);

    // Start device (uses timer to start once UI is fully loaded so we can receive messages)
    QTimer::singleShot(0, this, [=]() {
        m_kinect.init(bind(&AzureKinectWindow::errorCallback, this, placeholders::_1),
            bind(&AzureKinectWindow::readyCallback, this),
            bind(&AzureKinectWindow::imageCallback, this, placeholders::_1, placeholders::_2, placeholders::_3,
                placeholders::_4));
    });
}

void AzureKinectWindow::startSlot() noexcept
{
    if (!m_started) {
        // Validate PID has been entered
        if (m_ui.lineEditPID->text().length() == 0) {
            QMessageBox::warning(this, tr("AzureKinect"), tr("Please enter a valid PID"));
            return;
        }
        const auto pid = m_ui.lineEditPID->text().toInt();

        // Start capture thread
        m_kinect.start(pid);

        // Change button to stop
        m_started = true;
        m_ui.buttonStart->setText(tr("Stop"));
    } else {
        m_kinect.stop();
        m_started = false;
        m_ui.buttonStart->setText(tr("Start"));
    }
}

void AzureKinectWindow::exitSlot() noexcept
{
    // Shutdown kinect thread
    m_kinect.shutdown();

    QCoreApplication::quit();
}

void AzureKinectWindow::errorSlot(const QString& message) noexcept
{
    QMessageBox::critical(this, tr("AzureKinect"), message);
    exitSlot();
}

void AzureKinectWindow::readySlot() const noexcept
{
    // Enable start button
    m_ui.buttonStart->setEnabled(true);
}

void AzureKinectWindow::closeEvent(QCloseEvent* event) noexcept
{
    exitSlot();
    event->accept();
}

void AzureKinectWindow::errorCallback(const std::string& message) const noexcept
{
    // Must ensure that message box is displayed in primary gui thread
    emit errorSignal(QString::fromStdString(message));
}

void AzureKinectWindow::readyCallback() const noexcept
{
    emit readySignal();
}

void AzureKinectWindow::imageCallback(
    uint8_t* imageData, const uint32_t width, const uint32_t height, const uint32_t stride) noexcept
{
    // Need to copy data into local storage
    m_depthBuffer[1].setRawData(reinterpret_cast<char*>(imageData), height * stride);
    {
        m_depthBuffer[0].swap(m_depthBuffer[1]);
    }
    emit imageSignal(m_depthBuffer[0].data(), width, height, stride);
}
} // namespace Ak
