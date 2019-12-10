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
// Allow data types to be passed by qt connect function
Q_DECLARE_METATYPE(KinectImage);
Q_DECLARE_METATYPE(KinectJoints);

AzureKinectWindow::AzureKinectWindow(QWidget* parent) noexcept
    : QMainWindow(parent)
{
    m_ui.setupUi(this);

    // Ensure only 3 digit integers can be entered for PID
    m_validatorPID = new QIntValidator(1L, 999L, m_ui.centralWidget);
    m_ui.lineEditPID->setValidator(m_validatorPID);

    // Reserve initial memory
    for (auto& i : m_dataBuffer) {
        i.m_image.reserve(1280 * 720 * 4);
        i.m_joints.reserve(40);
    }

    // Connect required signals/slots
    connect(m_ui.buttonStart, &QPushButton::clicked, this, &AzureKinectWindow::startSlot);
    connect(m_ui.actionExit, &QAction::triggered, this, &AzureKinectWindow::exitSlot);
    connect(this, &AzureKinectWindow::errorSignal, this, &AzureKinectWindow::errorSlot);
    connect(this, &AzureKinectWindow::readySignal, this, &AzureKinectWindow::readySlot);
    qRegisterMetaType<KinectImage>();
    qRegisterMetaType<KinectJoints>();
    connect(this, &AzureKinectWindow::dataSignal, m_ui.openGLWidget, &KinectWidget::dataSlot);
    connect(m_ui.openGLWidget, &KinectWidget::errorSignal, this, &AzureKinectWindow::errorSlot);
    connect(m_ui.openGLWidget, &KinectWidget::refreshRenderSignal, m_ui.openGLWidget, &KinectWidget::refreshRenderSlot);
    connect(m_ui.openGLWidget, &KinectWidget::refreshCalibrationSignal, m_ui.openGLWidget,
        &KinectWidget::refreshCalibrationSlot);
    connect(m_ui.actionDepth_Image, &QAction::triggered, this, &AzureKinectWindow::viewDepthImageSlot);
    connect(m_ui.actionColour_Image, &QAction::triggered, this, &AzureKinectWindow::viewColourImageSlot);
    connect(m_ui.actionIR_Image, &QAction::triggered, this, &AzureKinectWindow::viewIRImageSlot);
    connect(m_ui.actionBody_Shadow, &QAction::triggered, this, &AzureKinectWindow::viewBodyShadowSlot);
    connect(m_ui.actionBody_Skeleton, &QAction::triggered, this, &AzureKinectWindow::viewBodySkeletonSlot);
    connect(m_ui.actionDepth_Image_2, &QAction::triggered, this, &AzureKinectWindow::recordDepthImageSlot);
    connect(m_ui.actionColour_Image_2, &QAction::triggered, this, &AzureKinectWindow::recordColourImageSlot);
    connect(m_ui.actionIR_Image_2, &QAction::triggered, this, &AzureKinectWindow::recordIRImageSlot);
    connect(m_ui.actionBody_Skeleton_2, &QAction::triggered, this, &AzureKinectWindow::recordBodySkeletonSlot);

    // Start device (uses timer to start once UI is fully loaded so we can receive messages)
    QTimer::singleShot(0, this, [=]() {
        m_recorder.init(bind(&AzureKinectWindow::errorCallback, this, placeholders::_1));
        m_kinect.init(bind(&AzureKinectWindow::errorCallback, this, placeholders::_1),
            bind(&AzureKinectWindow::readyCallback, this, placeholders::_1),
            bind(&AzureKinectWindow::dataCallback, this, placeholders::_1, placeholders::_2, placeholders::_3,
                placeholders::_4, placeholders::_5, placeholders::_6));
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

        // Start recorder thread
        m_recorder.start(pid);

        // Change button to stop
        m_started = true;
        m_ui.buttonStart->setText(tr("Stop"));
        m_ui.actionDepth_Image_2->setEnabled(false);
        // Disable changing recording settings while running
        m_ui.actionColour_Image_2->setEnabled(false);
        m_ui.actionIR_Image_2->setEnabled(false);
        m_ui.actionBody_Skeleton_2->setEnabled(false);
    } else {
        m_recorder.stop();
        m_started = false;
        m_ui.buttonStart->setText(tr("Start"));
        m_ui.actionDepth_Image_2->setEnabled(true);
        m_ui.actionColour_Image_2->setEnabled(true);
        m_ui.actionIR_Image_2->setEnabled(true);
        m_ui.actionBody_Skeleton_2->setEnabled(true);
    }
}

void AzureKinectWindow::exitSlot() noexcept
{
    // Shutdown kinect threads
    m_kinect.shutdown();
    m_recorder.shutdown();

    QCoreApplication::quit();
}

void AzureKinectWindow::errorSlot(const QString& message) noexcept
{
    QMessageBox::critical(this, tr("AzureKinect"), message);
    exitSlot();
}

void AzureKinectWindow::readySlot() noexcept
{
    m_ready = true;
    updateRecordOptions(); // This will enable the start button if possible
}

void AzureKinectWindow::viewDepthImageSlot() noexcept
{
    m_viewDepthImage = true;
    // Only 1 of depth/colour image can be selected at a time so disable the current one and enable the others
    m_viewColourImage = false;
    m_viewIRImage = false;
    updateRenderOptions();
}

void AzureKinectWindow::viewColourImageSlot() noexcept
{
    m_viewColourImage = true;
    m_viewDepthImage = false;
    m_viewIRImage = false;
    updateRenderOptions();
}

void AzureKinectWindow::viewIRImageSlot() noexcept
{
    m_viewIRImage = true;
    m_viewDepthImage = false;
    m_viewColourImage = false;
    updateRenderOptions();
}

void AzureKinectWindow::viewBodyShadowSlot() noexcept
{
    m_viewBodyShadow = m_ui.actionBody_Shadow->isChecked();
    updateRenderOptions();
}

void AzureKinectWindow::viewBodySkeletonSlot() noexcept
{
    m_viewBodySkeleton = m_ui.actionBody_Skeleton->isChecked();
}

void AzureKinectWindow::recordDepthImageSlot() noexcept
{
    m_recordDepthImage = m_ui.actionDepth_Image_2->isChecked();
    updateRecordOptions();
}

void AzureKinectWindow::recordColourImageSlot() noexcept
{
    m_recordColourImage = m_ui.actionColour_Image_2->isChecked();
    updateRecordOptions();
}

void AzureKinectWindow::recordIRImageSlot() noexcept
{
    m_recordIRImage = m_ui.actionIR_Image_2->isChecked();
    updateRecordOptions();
}

void AzureKinectWindow::recordBodySkeletonSlot() noexcept
{
    m_recordBodySkeleton = m_ui.actionBody_Skeleton_2->isChecked();
    updateRecordOptions();
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

void AzureKinectWindow::readyCallback(const KinectCalibration& calibration) noexcept
{
    // Send information to the render widget and recorder
    m_ui.openGLWidget->updateCalibration(calibration);
    m_recorder.updateCalibration(calibration);

    // Note: Currently assumes that the recorder thread has already initialised at this point
    // TODO: Correctly wait for both threads to have started
    emit readySignal();
}

void AzureKinectWindow::dataCallback(const uint64_t time, const KinectImage& depthImage, const KinectImage& colourImage,
    const KinectImage& irImage, const KinectImage& shadowImage, const KinectJoints& joints) noexcept
{
    // Call recorder callback
    m_recorder.dataCallback(time, depthImage, colourImage, irImage, shadowImage, joints);

    // Need to copy data into local storage
    ++m_bufferIndex;
    m_bufferIndex = m_bufferIndex < m_dataBuffer.size() ? m_bufferIndex : 0;
    if (m_viewDepthImage) {
        if (depthImage.m_image == nullptr) {
            return;
        }
        m_dataBuffer[m_bufferIndex].m_image.resize(0);
        m_dataBuffer[m_bufferIndex].m_image.insert(m_dataBuffer[m_bufferIndex].m_image.begin(), depthImage.m_image,
            depthImage.m_image + (static_cast<size_t>(depthImage.m_height) * depthImage.m_stride));
    } else if (m_viewColourImage) {
        if (colourImage.m_image == nullptr) {
            return;
        }
        m_dataBuffer[m_bufferIndex].m_image.resize(0);
        m_dataBuffer[m_bufferIndex].m_image.insert(m_dataBuffer[m_bufferIndex].m_image.begin(), colourImage.m_image,
            colourImage.m_image + (static_cast<size_t>(colourImage.m_height) * colourImage.m_stride));
    } else if (m_viewIRImage) {
        if (irImage.m_image == nullptr) {
            return;
        }
        m_dataBuffer[m_bufferIndex].m_image.resize(0);
        m_dataBuffer[m_bufferIndex].m_image.insert(m_dataBuffer[m_bufferIndex].m_image.begin(), irImage.m_image,
            irImage.m_image + (static_cast<size_t>(irImage.m_height) * irImage.m_stride));
    }

    if (m_viewBodyShadow) {
        m_dataBuffer[m_bufferIndex].m_shadow.resize(0);
        m_dataBuffer[m_bufferIndex].m_shadow.insert(m_dataBuffer[m_bufferIndex].m_shadow.begin(), shadowImage.m_image,
            shadowImage.m_image + (static_cast<size_t>(shadowImage.m_height) * shadowImage.m_stride));
    }
    if (m_viewBodySkeleton) {
        m_dataBuffer[m_bufferIndex].m_joints.resize(0);
        m_dataBuffer[m_bufferIndex].m_joints.insert(m_dataBuffer[m_bufferIndex].m_joints.begin(), joints.m_joints,
            joints.m_joints + static_cast<size_t>(joints.m_length));
    }
    auto depthCopy = depthImage;
    depthCopy.m_image = m_dataBuffer[m_bufferIndex].m_image.data();
    auto colourCopy = colourImage;
    colourCopy.m_image = m_dataBuffer[m_bufferIndex].m_image.data();
    auto irCopy = irImage;
    irCopy.m_image = m_dataBuffer[m_bufferIndex].m_image.data();
    auto shadowCopy = shadowImage;
    shadowCopy.m_image = m_dataBuffer[m_bufferIndex].m_shadow.data();
    auto jointCopy = joints;
    jointCopy.m_joints = m_dataBuffer[m_bufferIndex].m_joints.data();
    emit dataSignal(depthCopy, colourCopy, irCopy, shadowCopy, jointCopy);
}

void AzureKinectWindow::updateRenderOptions() const noexcept
{
    m_ui.actionDepth_Image->setChecked(m_viewDepthImage);
    m_ui.actionDepth_Image->setEnabled(!m_viewDepthImage);
    m_ui.actionColour_Image->setChecked(m_viewColourImage);
    m_ui.actionColour_Image->setEnabled(!m_viewColourImage);
    m_ui.actionIR_Image->setChecked(m_viewIRImage);
    m_ui.actionIR_Image->setEnabled(!m_viewIRImage);
    m_ui.openGLWidget->setRenderOptions(
        m_viewDepthImage, m_viewColourImage, m_viewIRImage, m_viewBodyShadow, m_viewBodySkeleton);
}

void AzureKinectWindow::updateRecordOptions() noexcept
{
    if (!m_recordDepthImage && !m_recordColourImage && !m_recordIRImage && !m_recordBodySkeleton) {
        // If no recording options have been specified then disable the start button
        m_ui.buttonStart->setEnabled(false);
    } else if (m_ready && !m_ui.buttonStart->isEnabled()) {
        m_ui.buttonStart->setEnabled(true);
    }
    m_recorder.setRecordOptions(m_recordDepthImage, m_recordColourImage, m_recordIRImage, m_recordBodySkeleton);
}
} // namespace Ak
