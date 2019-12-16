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
extern void logHandler(const std::string& message);

static void azureCallback(void*, k4a_log_level_t, const char*, const int, const char* message)
{
    logHandler(message);
}

AzureKinect::~AzureKinect()
{
    shutdown();
    cleanup();
}

bool AzureKinect::init(errorCallback error, readyCallback ready, dataCallback data) noexcept
{
    // Store callbacks
    m_errorCallback = move(error);
    m_dataCallback = move(data);

    // Set k4a debug callback
    k4a_set_debug_message_handler(nullptr, nullptr, K4A_LOG_LEVEL_INFO);
#ifdef _DEBUG
    k4a_set_debug_message_handler(azureCallback, nullptr, K4A_LOG_LEVEL_INFO);
#else
    k4a_set_debug_message_handler(azureCallback, nullptr, K4A_LOG_LEVEL_ERROR);
#endif

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
    deviceConfig.color_resolution = K4A_COLOR_RESOLUTION_2160P;
    deviceConfig.camera_fps = K4A_FRAMES_PER_SECOND_30;
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
    trackerConfig.processing_mode = K4ABT_TRACKER_PROCESSING_MODE_GPU;
    if (k4abt_tracker_create(&sensorCalibration, trackerConfig, &m_tracker) != K4A_RESULT_SUCCEEDED) {
        if (m_errorCallback != nullptr) {
            m_errorCallback("Failed to create K4A body tracker");
        }
        return false;
    }

    // Setup internal conversion data
    m_calibration.m_depthBC = {{sensorCalibration.depth_camera_calibration.intrinsics.parameters.param.cx,
                                   sensorCalibration.depth_camera_calibration.intrinsics.parameters.param.cy},
        {sensorCalibration.depth_camera_calibration.intrinsics.parameters.param.fx,
            sensorCalibration.depth_camera_calibration.intrinsics.parameters.param.fy},
        {sensorCalibration.depth_camera_calibration.intrinsics.parameters.param.k1,
            sensorCalibration.depth_camera_calibration.intrinsics.parameters.param.k4},
        {sensorCalibration.depth_camera_calibration.intrinsics.parameters.param.k2,
            sensorCalibration.depth_camera_calibration.intrinsics.parameters.param.k5},
        {sensorCalibration.depth_camera_calibration.intrinsics.parameters.param.k3,
            sensorCalibration.depth_camera_calibration.intrinsics.parameters.param.k6},
        {sensorCalibration.depth_camera_calibration.intrinsics.parameters.param.p1,
            sensorCalibration.depth_camera_calibration.intrinsics.parameters.param.p2}};
    m_calibration.m_colourBC = {{sensorCalibration.color_camera_calibration.intrinsics.parameters.param.cx,
                                    sensorCalibration.color_camera_calibration.intrinsics.parameters.param.cy},
        {sensorCalibration.color_camera_calibration.intrinsics.parameters.param.fx,
            sensorCalibration.color_camera_calibration.intrinsics.parameters.param.fy},
        {sensorCalibration.color_camera_calibration.intrinsics.parameters.param.k1,
            sensorCalibration.color_camera_calibration.intrinsics.parameters.param.k4},
        {sensorCalibration.color_camera_calibration.intrinsics.parameters.param.k2,
            sensorCalibration.color_camera_calibration.intrinsics.parameters.param.k5},
        {sensorCalibration.color_camera_calibration.intrinsics.parameters.param.k3,
            sensorCalibration.color_camera_calibration.intrinsics.parameters.param.k6},
        {sensorCalibration.color_camera_calibration.intrinsics.parameters.param.p1,
            sensorCalibration.color_camera_calibration.intrinsics.parameters.param.p2}};
    m_calibration.m_irBC = m_calibration.m_depthBC;

    m_calibration.m_jointToDepth = glm::mat4(1.0f);
    m_calibration.m_jointToColour = glm::mat4(
        sensorCalibration.extrinsics[K4A_CALIBRATION_TYPE_DEPTH][K4A_CALIBRATION_TYPE_COLOR].rotation[0],
        sensorCalibration.extrinsics[K4A_CALIBRATION_TYPE_DEPTH][K4A_CALIBRATION_TYPE_COLOR].rotation[3],
        sensorCalibration.extrinsics[K4A_CALIBRATION_TYPE_DEPTH][K4A_CALIBRATION_TYPE_COLOR].rotation[6], 0.0f,
        sensorCalibration.extrinsics[K4A_CALIBRATION_TYPE_DEPTH][K4A_CALIBRATION_TYPE_COLOR].rotation[1],
        sensorCalibration.extrinsics[K4A_CALIBRATION_TYPE_DEPTH][K4A_CALIBRATION_TYPE_COLOR].rotation[4],
        sensorCalibration.extrinsics[K4A_CALIBRATION_TYPE_DEPTH][K4A_CALIBRATION_TYPE_COLOR].rotation[7], 0.0f,
        sensorCalibration.extrinsics[K4A_CALIBRATION_TYPE_DEPTH][K4A_CALIBRATION_TYPE_COLOR].rotation[2],
        sensorCalibration.extrinsics[K4A_CALIBRATION_TYPE_DEPTH][K4A_CALIBRATION_TYPE_COLOR].rotation[5],
        sensorCalibration.extrinsics[K4A_CALIBRATION_TYPE_DEPTH][K4A_CALIBRATION_TYPE_COLOR].rotation[8], 0.0f,
        sensorCalibration.extrinsics[K4A_CALIBRATION_TYPE_DEPTH][K4A_CALIBRATION_TYPE_COLOR].translation[0] * 0.001f,
        sensorCalibration.extrinsics[K4A_CALIBRATION_TYPE_DEPTH][K4A_CALIBRATION_TYPE_COLOR].translation[1] * 0.001f,
        sensorCalibration.extrinsics[K4A_CALIBRATION_TYPE_DEPTH][K4A_CALIBRATION_TYPE_COLOR].translation[2] * 0.001f,
        1.0f);
    m_calibration.m_jointToIR = glm::mat4(1.0f);

    // K4A depth camera FOV is as follows:
    //  K4A_DEPTH_MODE_NFOV_2X2BINNED= 75x65;
    //  K4A_DEPTH_MODE_NFOV_UNBINNED= 75x65;
    //  K4A_DEPTH_MODE_WFOV_2X2BINNED= 120x120;
    //  K4A_DEPTH_MODE_WFOV_UNBINNED= 120x120;
    //  K4A_DEPTH_MODE_PASSIVE_IR= NA
    if (sensorCalibration.depth_mode == K4A_DEPTH_MODE_NFOV_2X2BINNED ||
        sensorCalibration.depth_mode == K4A_DEPTH_MODE_NFOV_UNBINNED) {
        m_calibration.m_depthFOV = {75.0f, 65.0f};
    } else {
        m_calibration.m_depthFOV = {120.0f, 120.0f};
    }

    // K4A colour camera FOV is as follows:
    //  K4A_COLOR_RESOLUTION_720P= 90x59
    //  K4A_COLOR_RESOLUTION_1080P= 90x59
    //  K4A_COLOR_RESOLUTION_1440P= 90x59
    //  K4A_COLOR_RESOLUTION_1536P= 90x74.3
    //  K4A_COLOR_RESOLUTION_2160P= 90x59
    //  K4A_COLOR_RESOLUTION_3072P= 90x74.3
    if (sensorCalibration.color_resolution == K4A_COLOR_RESOLUTION_1536P ||
        sensorCalibration.color_resolution == K4A_COLOR_RESOLUTION_3072P) {
        m_calibration.m_colourFOV = {90.0f, 74.3f};
    } else {
        m_calibration.m_colourFOV = {90.0f, 59.0f};
    }

    m_calibration.m_irFOV = m_calibration.m_depthFOV;

    // K4A image dimensions are as follows:
    //  K4A_DEPTH_MODE_NFOV_2X2BINNED= {320, 288};
    //  K4A_DEPTH_MODE_NFOV_UNBINNED= {640, 576};
    //  K4A_DEPTH_MODE_WFOV_2X2BINNED= {512, 512};
    //  K4A_DEPTH_MODE_WFOV_UNBINNED= {1024, 1024};
    //  K4A_DEPTH_MODE_PASSIVE_IR= {1024, 1024}; otherwise IR equals same as depth
    if (sensorCalibration.depth_mode == K4A_DEPTH_MODE_NFOV_2X2BINNED) {
        m_calibration.m_depthDimensions = {320.0f, 288.0f};
    } else if (sensorCalibration.depth_mode == K4A_DEPTH_MODE_NFOV_UNBINNED) {
        m_calibration.m_depthDimensions = {640.0f, 576.0f};
    } else if (sensorCalibration.depth_mode == K4A_DEPTH_MODE_WFOV_2X2BINNED) {
        m_calibration.m_depthDimensions = {512.0f, 512.0f};
    } else {
        m_calibration.m_depthDimensions = {1024.0f, 1024.0f};
    }

    if (sensorCalibration.color_resolution == K4A_COLOR_RESOLUTION_720P) {
        m_calibration.m_colourDimensions = {1280, 720};
    } else if (sensorCalibration.color_resolution == K4A_COLOR_RESOLUTION_1080P) {
        m_calibration.m_colourDimensions = {1920, 1080};
    } else if (sensorCalibration.color_resolution == K4A_COLOR_RESOLUTION_1440P) {
        m_calibration.m_colourDimensions = {2560, 1440};
    } else if (sensorCalibration.color_resolution == K4A_COLOR_RESOLUTION_1536P) {
        m_calibration.m_colourDimensions = {2048, 1536};
    } else if (sensorCalibration.color_resolution == K4A_COLOR_RESOLUTION_2160P) {
        m_calibration.m_colourDimensions = {3840, 2160};
    } else if (sensorCalibration.color_resolution == K4A_COLOR_RESOLUTION_3072P) {
        m_calibration.m_colourDimensions = {4096, 3072};
    }

    if (sensorCalibration.depth_mode == K4A_DEPTH_MODE_PASSIVE_IR) {
        m_calibration.m_irDimensions = {1024, 1024};
    } else {
        m_calibration.m_irDimensions = m_calibration.m_depthDimensions;
    }

    if (deviceConfig.camera_fps == K4A_FRAMES_PER_SECOND_5) {
        m_calibration.m_fps = 5;
    } else if (deviceConfig.camera_fps == K4A_FRAMES_PER_SECOND_15) {
        m_calibration.m_fps = 15;
    } else if (deviceConfig.camera_fps == K4A_FRAMES_PER_SECOND_30) {
        m_calibration.m_fps = 30;
    }

    //  K4A_DEPTH_MODE_NFOV_2X2BINNED = 500 -> 5800
    //  K4A_DEPTH_MODE_NFOV_UNBINNED = 500 -> 4000
    //  K4A_DEPTH_MODE_WFOV_2X2BINNED = 250 -> 3000
    //  K4A_DEPTH_MODE_WFOV_UNBINNED = 250 -> 2500
    //  K4A_DEPTH_MODE_PASSIVE_IR = 0 -> 1000
    if (sensorCalibration.depth_mode == K4A_DEPTH_MODE_NFOV_2X2BINNED) {
        m_calibration.m_depthRange = {500, 5800};
    } else if (sensorCalibration.depth_mode == K4A_DEPTH_MODE_NFOV_UNBINNED) {
        m_calibration.m_depthRange = {500, 4000};
    } else if (sensorCalibration.depth_mode == K4A_DEPTH_MODE_WFOV_2X2BINNED) {
        m_calibration.m_depthRange = {250, 3000};
    } else if (sensorCalibration.depth_mode == K4A_DEPTH_MODE_WFOV_UNBINNED) {
        m_calibration.m_depthRange = {250, 2500};
    } else {
        m_calibration.m_depthRange = {0, 1000};
    }

    m_calibration.m_irRange = {0, 1000};

    return true;
}

bool AzureKinect::run(const readyCallback& ready) noexcept
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
        ready(m_calibration);
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
                            static_cast<float>(joint.confidence_level) /
                                static_cast<float>(
                                    K4ABT_JOINT_CONFIDENCE_MEDIUM)); // Medium is currently the highest supported by SDK
                    } else {
                        bodyJoint.emplace_back(unknown, unknown2, false);
                    }
                }
            } else {
                // Reset the buffers to contain invalid data
                const auto depthWidth = k4a_image_get_width_pixels(depthImage);
                const auto depthHeight = k4a_image_get_height_pixels(depthImage);
                bodyPixel.assign(static_cast<size_t>(depthWidth) * depthHeight, 0);
                bodyJoint.resize(0);
            }

            if (m_dataCallback) {
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
                if (m_dataCallback) {
                    m_dataCallback(time, depthPass, colourPass, irPass, shadow, joints);
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
