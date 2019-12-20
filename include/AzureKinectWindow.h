#pragma once
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

#include "AzureKinect.h"
#include "KinectRecord.h"
#include "ui_AzureKinect.h"

#include <QIntValidator>
#include <QtWidgets/QMainWindow>
#include <array>
#include <vector>

namespace Ak {
class AzureKinectWindow final : public QMainWindow
{
    Q_OBJECT

    friend class KinectWidget;

public:
    explicit AzureKinectWindow(QWidget* parent = Q_NULLPTR) noexcept;

public slots:

    /** Slot used to receive request to start recording camera data */
    void startSlot() noexcept;

    /** Slot used to receive request to exit the program */
    void exitSlot() noexcept;

    /** Slot used to pass error messages between threads/objects (calls exitSlot when done) */
    void errorSlot(const QString& message) noexcept;

    /** Slot used to notify program that camera is ready to start capture */
    void readySlot() noexcept;

    /** Slot used to select display of depth image */
    void viewDepthImageSlot() noexcept;

    /** Slot used to select display of colour image */
    void viewColourImageSlot() noexcept;

    /** Slot used to select display of IR image */
    void viewIRImageSlot() noexcept;

    /** Slot used to select display of body shadow */
    void viewBodyShadowSlot() noexcept;

    /** Slot used to select display of body skeleton */
    void viewBodySkeletonSlot() noexcept;

    /** Slot used to update the record options for the record object */
    void updateRecordOptionsSlot() noexcept;

private:
    Ui::AzureKinectClass m_ui;
    QValidator* m_validatorPID = nullptr;

    struct DataBuffers
    {
        std::vector<uint8_t> m_image;
        std::vector<uint8_t> m_shadow;
        std::vector<Joint> m_joints;
    };

    std::array<DataBuffers, 15> m_dataBuffer;
    uint32_t m_bufferIndex = 0;
    bool m_viewDepthImage = true;
    bool m_viewColourImage = false;
    bool m_viewIRImage = false;
    bool m_viewBodyShadow = true;
    bool m_viewBodySkeleton = true;
    bool m_started = false;
    bool m_ready = false;
    AzureKinect m_kinect;
    KinectRecord m_recorder;

    /**
     * Override used to capture and handle close events.
     * @param [in,out] event If non-null, the event.
     */
    void closeEvent(QCloseEvent* event) noexcept override;

    /**
     * Callback used by the camera thread to notify of errors.
     * @note This provide thread safe, asynchronous error handling.
     * @param message The error message.
     */
    void errorCallback(const std::string& message) const noexcept;

    /**
     * Callback used by the camera thread to notify when its ready to begin recording
     * @note This provide thread safe, asynchronous handling.
     * @param calibration The calibration information for the camera.
     */
    void readyCallback(const KinectCalibration& calibration) noexcept;

    /**
     * Callback used by the camera thread when new image/position information is available.
     * @param time        The timestamp of the capture.
     * @param depthImage  The depth image data.
     * @param colourImage The colour image data.
     * @param irImage     The IR image data.
     * @param shadowImage The body shadow image data.
     * @param joints      The joint data.
     */
    void dataCallback(uint64_t time, const KinectImage& depthImage, const KinectImage& colourImage,
        const KinectImage& irImage, const KinectImage& shadowImage, const KinectJoints& joints) noexcept;

    /** Updates the render options for the render widget */
    void updateRenderOptions() const noexcept;

signals:
    /**
     * Signal used to pass asynchronous thread safe error messages.
     * @note This is required by @errorCallback.
     * @param message The message.
     */
    void errorSignal(const QString& message) const;

    /**
     * Signal used to pass asynchronous thread safe ready state.
     * @note This is required by @readyCallback.
     */
    void readySignal() const;

    /**
     * Signal used to pass asynchronous thread safe image/position data.
     * @note This is required by @dataCallback.
     * @param depthImage  The depth image data.
     * @param colourImage The colour image data.
     * @param irImage     The IR image data.
     * @param shadowImage The body shadow image data.
     * @param joints      The joint data.
     */
    void dataSignal(KinectImage depthImage, KinectImage colourImage, KinectImage irImage, KinectImage shadowImage,
        KinectJoints joints) const;
};
} // namespace Ak
