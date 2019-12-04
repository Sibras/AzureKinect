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

#include <array>
#include <k4a/k4a.h>
#include <vector>
using namespace std;

namespace Ak {
AzureKinect::~AzureKinect()
{
    shutdown();
    cleanup();
}

bool AzureKinect::init(errorCallback error, readyCallback ready, dataCallback data1, dataCallback data2) noexcept
{
    // Store callbacks
    m_errorCallback = move(error);
    m_data1Callback = move(data1);
    m_data2Callback = move(data2);

    // Start capture thread running
    m_captureThread = thread(&AzureKinect::run, this, move(ready));

    return true;
}

void AzureKinect::shutdown() noexcept
{
    m_shutdown = true;
    // Wait for thread to complete
    if (m_captureThread.joinable()) {
        m_captureThread.join();
    }
}

bool AzureKinect::initCamera() noexcept
{
    if (k4a_device_open(0, &m_device) != K4A_RESULT_SUCCEEDED) {
        if (m_errorCallback != nullptr) {
            m_errorCallback("Failed to open K4A device");
        }
        return false;
    }

    // Start camera. Make sure depth camera is enabled.
    k4a_device_configuration_t deviceConfig = K4A_DEVICE_CONFIG_INIT_DISABLE_ALL;
    deviceConfig.depth_mode = K4A_DEPTH_MODE_NFOV_UNBINNED;
    deviceConfig.color_resolution = K4A_COLOR_RESOLUTION_720P;
    deviceConfig.camera_fps = K4A_FRAMES_PER_SECOND_15;
    deviceConfig.color_format = K4A_IMAGE_FORMAT_COLOR_BGRA32;
    deviceConfig.synchronized_images_only = true;
    if (k4a_device_start_cameras(m_device, &deviceConfig) != K4A_RESULT_SUCCEEDED) {
        k4a_device_stop_cameras(m_device);
        if (k4a_device_start_cameras(m_device, &deviceConfig) != K4A_RESULT_SUCCEEDED) {
            if (m_errorCallback != nullptr) {
                m_errorCallback("Failed to start K4A camera");
            }
            return false;
        }
    }

    // Get calibration information
    k4a_calibration_t sensorCalibration;
    if (k4a_device_get_calibration(m_device, deviceConfig.depth_mode, deviceConfig.color_resolution,
            &sensorCalibration) != K4A_RESULT_SUCCEEDED) {
        if (m_errorCallback != nullptr) {
            m_errorCallback("Failed to calibrate K4A camera");
        }
        return false;
    }

    // Create Body Tracker
    k4abt_tracker_configuration_t trackerConfig = K4ABT_TRACKER_CONFIG_DEFAULT;
    trackerConfig.cpu_only_mode = false;
    if (k4abt_tracker_create(&sensorCalibration, trackerConfig, &m_tracker) != K4A_RESULT_SUCCEEDED) {
        if (m_errorCallback != nullptr) {
            m_errorCallback("Failed to create K4A body tracker");
        }
        return false;
    }

    return true;
}

bool AzureKinect::run(const std::function<void()>& ready) noexcept
{
    if (!initCamera()) {
        cleanup();
        return false;
    }

    // Pre-allocate storage
    vector<uint8_t> bodyPixel(1024 * 1024);
    vector<Joint> bodyJoint(K4ABT_JOINT_COUNT);
    const Position unknown(-10000.0f, -10000.0f, -10000.0f);
    const Quaternion unknown2(0.0f, 0.0f, 0.0f, 0.0f);

    // Trigger callback
    if (ready) {
        ready();
    }

    while (!m_shutdown) {
        // Get the capture from the camera
        k4a_capture_t captureHandle = nullptr;
        const k4a_wait_result_t captureResult = k4a_device_get_capture(m_device, &captureHandle, 10);

        if (captureResult == K4A_WAIT_RESULT_SUCCEEDED) {
            // Check if we have valid depth data
            const k4a_image_t depthImage = k4a_capture_get_depth_image(captureHandle);
            if (depthImage == nullptr) {
                k4a_image_release(depthImage);
                continue;
            }
            k4a_image_release(depthImage);

            // Send the new capture to the tracker
            const k4a_wait_result_t trackerResult = k4abt_tracker_enqueue_capture(m_tracker, captureHandle, 0);

            k4a_capture_release(captureHandle);

            if (trackerResult == K4A_WAIT_RESULT_FAILED) {
                if (m_errorCallback != nullptr) {
                    m_errorCallback("Failed to add K4A capture to tracker process queue");
                }
                break;
            }
        } else if (captureResult != K4A_WAIT_RESULT_TIMEOUT) {
            if (m_errorCallback != nullptr) {
                m_errorCallback("Failed to get capture from K4A camera");
            }
            break;
        }

        // Get result from tracker
        k4abt_frame_t bodyFrame = nullptr;
        const k4a_wait_result_t frameResult = k4abt_tracker_pop_result(m_tracker, &bodyFrame, 0);
        if (frameResult == K4A_WAIT_RESULT_SUCCEEDED) {
            // Get the original capture
            const k4a_capture_t originalCapture = k4abt_frame_get_capture(bodyFrame);
            const k4a_image_t depthImage = k4a_capture_get_depth_image(originalCapture);

            // Get the tracker data
            if (k4abt_frame_get_num_bodies(bodyFrame) > 0) {
                k4abt_body_t body;
                if (k4abt_frame_get_body_skeleton(bodyFrame, 0, &body.skeleton) != K4A_RESULT_SUCCEEDED) {
                    if (m_errorCallback != nullptr) {
                        m_errorCallback("Failed to get skeleton from K4A capture");
                    }
                }

                // Get the body pixel positions for the first detected body
                const auto depthWidth = k4a_image_get_width_pixels(depthImage);
                const auto depthHeight = k4a_image_get_height_pixels(depthImage);
                const k4a_image_t indexMap = k4abt_frame_get_body_index_map(bodyFrame);
                const uint8_t* indexMapBuffer = k4a_image_get_buffer(indexMap);
                bodyPixel.resize(static_cast<size_t>(depthWidth) * depthHeight);
                for (int i = 0; i < depthWidth * depthHeight; i++) {
                    const uint8_t bodyIndex = indexMapBuffer[i];
                    if (bodyIndex == 0) {
                        bodyPixel[i] = std::numeric_limits<uint8_t>::max();
                    } else {
                        // K4ABT_BODY_INDEX_MAP_BACKGROUND if not a body
                        bodyPixel[i] = 0;
                    }
                }
                k4a_image_release(indexMap);

                // Get the joint information
                bodyJoint.resize(0);
                for (auto& joint : body.skeleton.joints) {
                    if (joint.confidence_level >= K4ABT_JOINT_CONFIDENCE_LOW) {
                        const k4a_float3_t& jointPosition = joint.position;
                        const k4a_quaternion_t& jointOrientation = joint.orientation;

                        bodyJoint.emplace_back(Position{jointPosition.xyz.x, jointPosition.xyz.y, jointPosition.xyz.z},
                            Quaternion{jointOrientation.wxyz.x, jointOrientation.wxyz.y, jointOrientation.wxyz.z,
                                jointOrientation.wxyz.w},
                            joint.confidence_level >= K4ABT_JOINT_CONFIDENCE_MEDIUM);
                    } else {
                        bodyJoint.emplace_back(unknown, unknown2, false);
                    }
                }
            } else {
                // Reset the buffers to contain invalid data
                bodyPixel.assign(static_cast<size_t>(k4a_image_get_width_pixels(depthImage)) *
                        k4a_image_get_height_pixels(depthImage),
                    0);
                bodyJoint.resize(0);
            }

            // TODO: Send data to renderer
            if (m_data1Callback || m_data2Callback) {
                const auto time = k4abt_frame_get_device_timestamp_usec(bodyFrame);
                const auto colourImage = k4a_capture_get_color_image(originalCapture);
                const auto irImage = k4a_capture_get_ir_image(originalCapture);
                KinectImage depthPass = {k4a_image_get_buffer(depthImage), k4a_image_get_width_pixels(depthImage),
                    k4a_image_get_height_pixels(depthImage), k4a_image_get_stride_bytes(depthImage)};
                KinectImage colourPass = {k4a_image_get_buffer(colourImage), k4a_image_get_width_pixels(colourImage),
                    k4a_image_get_height_pixels(colourImage), k4a_image_get_stride_bytes(colourImage)};
                KinectImage irPass = {k4a_image_get_buffer(irImage), k4a_image_get_width_pixels(irImage),
                    k4a_image_get_height_pixels(irImage), k4a_image_get_stride_bytes(irImage)};

                KinectImage shadow = {bodyPixel.data(), depthPass.m_width, depthPass.m_height, depthPass.m_width};
                KinectJoints joints(bodyJoint.data(), static_cast<uint32_t>(bodyJoint.size()));
                if (m_data1Callback) {
                    m_data1Callback(time, depthPass, colourPass, irPass, shadow, joints);
                }

                if (m_data2Callback) {
                    m_data2Callback(time, depthPass, colourPass, irPass, shadow, joints);
                }

                k4a_image_release(colourImage);
                k4a_image_release(irImage);
            }

            k4a_image_release(depthImage);
            k4a_capture_release(originalCapture);
        }
        k4abt_frame_release(bodyFrame);
    }

    // Cleanup all data
    cleanup();

    return true;
}

void AzureKinect::cleanup() noexcept
{
    if (m_tracker) {
        k4abt_tracker_shutdown(m_tracker);
        k4abt_tracker_destroy(m_tracker);
        m_tracker = nullptr;
    }

    if (m_device) {
        k4a_device_stop_cameras(m_device);
        k4a_device_close(m_device);
        m_device = nullptr;
    }
}
} // namespace Ak
