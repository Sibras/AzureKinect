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
Q_DECLARE_METATYPE(AzureKinect::KinectImage);

AzureKinectWindow::AzureKinectWindow(QWidget* parent) noexcept
    : QMainWindow(parent)
{
    m_ui.setupUi(this);

    // Ensure only 3 digit integers can be entered for PID
    m_validatorPID = new QIntValidator(100L, 999L, m_ui.centralWidget);
    m_ui.lineEditPID->setValidator(m_validatorPID);

    // Reserve initial memory
    m_imageBuffer[0].reserve(1280 * 720 * 40);
    m_imageBuffer[1].reserve(1280 * 720 * 40);

    // Connect required signals/slots
    connect(m_ui.buttonStart, &QPushButton::clicked, this, &AzureKinectWindow::startSlot);
    connect(m_ui.actionExit, &QAction::triggered, this, &AzureKinectWindow::exitSlot);
    connect(this, &AzureKinectWindow::errorSignal, this, &AzureKinectWindow::errorSlot);
    connect(this, &AzureKinectWindow::readySignal, this, &AzureKinectWindow::readySlot);
    qRegisterMetaType<AzureKinect::KinectImage>();
    connect(this, &AzureKinectWindow::imageSignal, m_ui.openGLWidget, &KinectWidget::imageSlot);
    connect(m_ui.openGLWidget, &KinectWidget::errorSignal, this, &AzureKinectWindow::errorSlot);
    connect(m_ui.actionDepth_Image, &QAction::triggered, this, &AzureKinectWindow::depthImageSlot);
    connect(m_ui.actionColour_Image, &QAction::triggered, this, &AzureKinectWindow::colourImageSlot);
    connect(m_ui.actionBody_Shadow, &QAction::triggered, this, &AzureKinectWindow::bodyShadowSlot);
    connect(m_ui.actionBody_Skeleton, &QAction::triggered, this, &AzureKinectWindow::bodySkeletonSlot);

    // Start device (uses timer to start once UI is fully loaded so we can receive messages)
    QTimer::singleShot(0, this, [=]() {
        m_kinect.init(bind(&AzureKinectWindow::errorCallback, this, placeholders::_1),
            bind(&AzureKinectWindow::readyCallback, this),
            bind(&AzureKinectWindow::imageCallback, this, placeholders::_1, placeholders::_2));
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

void AzureKinectWindow::depthImageSlot() noexcept
{
    m_depthImage = true;
    // Only 1 of depth/colour image can be selected at a time so disable the current one and enable the other
    m_ui.actionDepth_Image->setChecked(true);
    m_ui.actionDepth_Image->setDisabled(true);
    m_ui.actionColour_Image->setChecked(false);
    m_ui.actionColour_Image->setEnabled(true);
    m_ui.openGLWidget->setRenderOptions(m_depthImage, !m_depthImage, m_bodyShadowImage, m_bodySkeletonImage);
}

void AzureKinectWindow::colourImageSlot() noexcept
{
    m_depthImage = false;
    m_ui.actionColour_Image->setChecked(true);
    m_ui.actionColour_Image->setDisabled(true);
    m_ui.actionDepth_Image->setChecked(false);
    m_ui.actionDepth_Image->setEnabled(true);
    m_ui.openGLWidget->setRenderOptions(m_depthImage, !m_depthImage, m_bodyShadowImage, m_bodySkeletonImage);
}

void AzureKinectWindow::bodyShadowSlot() noexcept
{
    m_bodyShadowImage = m_ui.actionBody_Shadow->isChecked();
    m_ui.openGLWidget->setRenderOptions(m_depthImage, !m_depthImage, m_bodyShadowImage, m_bodySkeletonImage);
}

void AzureKinectWindow::bodySkeletonSlot() noexcept
{
    m_bodySkeletonImage = m_ui.actionBody_Skeleton->isChecked();
    m_ui.openGLWidget->setRenderOptions(m_depthImage, !m_depthImage, m_bodyShadowImage, m_bodySkeletonImage);
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
    const AzureKinect::KinectImage& depthImage, const AzureKinect::KinectImage& colourImage) noexcept
{
    // Need to copy data into local storage
    if (m_depthImage) {
        if (depthImage.m_image == nullptr) {
            return;
        }
        m_imageBuffer[1].insert(m_imageBuffer[1].begin(), depthImage.m_image,
            depthImage.m_image + (static_cast<size_t>(depthImage.m_height) * depthImage.m_stride));
    } else {
        if (colourImage.m_image == nullptr) {
            return;
        }
        m_imageBuffer[1].insert(m_imageBuffer[1].begin(), colourImage.m_image,
            colourImage.m_image + (static_cast<size_t>(colourImage.m_height) * colourImage.m_stride));
    }
    m_imageBuffer[0].swap(m_imageBuffer[1]);
    if (m_bodyShadowImage) {
        // TODO:*********
    }
    if (m_bodySkeletonImage) {
        // TODO:*********
    }
    auto depthCopy = depthImage;
    depthCopy.m_image = m_imageBuffer[0].data();
    auto colourCopy = colourImage;
    colourCopy.m_image = m_imageBuffer[0].data();
    emit imageSignal(depthCopy, colourCopy);
}
} // namespace Ak
