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
#include <filesystem>
#include <iomanip>
#include <k4a/k4a.h>
#include <sstream>
#include <vector>
using namespace std;

namespace Ak {
static array<pair<k4abt_joint_id_t, k4abt_joint_id_t>, 31> s_boneList = {
    make_pair(K4ABT_JOINT_SPINE_CHEST, K4ABT_JOINT_SPINE_NAVAL), make_pair(K4ABT_JOINT_SPINE_NAVAL, K4ABT_JOINT_PELVIS),
    make_pair(K4ABT_JOINT_SPINE_CHEST, K4ABT_JOINT_NECK), make_pair(K4ABT_JOINT_NECK, K4ABT_JOINT_HEAD),
    make_pair(K4ABT_JOINT_HEAD, K4ABT_JOINT_NOSE), make_pair(K4ABT_JOINT_SPINE_CHEST, K4ABT_JOINT_CLAVICLE_LEFT),
    make_pair(K4ABT_JOINT_CLAVICLE_LEFT, K4ABT_JOINT_SHOULDER_LEFT),
    make_pair(K4ABT_JOINT_SHOULDER_LEFT, K4ABT_JOINT_ELBOW_LEFT),
    make_pair(K4ABT_JOINT_ELBOW_LEFT, K4ABT_JOINT_WRIST_LEFT), make_pair(K4ABT_JOINT_WRIST_LEFT, K4ABT_JOINT_HAND_LEFT),
    make_pair(K4ABT_JOINT_HAND_LEFT, K4ABT_JOINT_HANDTIP_LEFT),
    make_pair(K4ABT_JOINT_WRIST_LEFT, K4ABT_JOINT_THUMB_LEFT), make_pair(K4ABT_JOINT_PELVIS, K4ABT_JOINT_HIP_LEFT),
    make_pair(K4ABT_JOINT_HIP_LEFT, K4ABT_JOINT_KNEE_LEFT), make_pair(K4ABT_JOINT_KNEE_LEFT, K4ABT_JOINT_ANKLE_LEFT),
    make_pair(K4ABT_JOINT_ANKLE_LEFT, K4ABT_JOINT_FOOT_LEFT), make_pair(K4ABT_JOINT_NOSE, K4ABT_JOINT_EYE_LEFT),
    make_pair(K4ABT_JOINT_EYE_LEFT, K4ABT_JOINT_EAR_LEFT),
    make_pair(K4ABT_JOINT_SPINE_CHEST, K4ABT_JOINT_CLAVICLE_RIGHT),
    make_pair(K4ABT_JOINT_CLAVICLE_RIGHT, K4ABT_JOINT_SHOULDER_RIGHT),
    make_pair(K4ABT_JOINT_SHOULDER_RIGHT, K4ABT_JOINT_ELBOW_RIGHT),
    make_pair(K4ABT_JOINT_ELBOW_RIGHT, K4ABT_JOINT_WRIST_RIGHT),
    make_pair(K4ABT_JOINT_WRIST_RIGHT, K4ABT_JOINT_HAND_RIGHT),
    make_pair(K4ABT_JOINT_HAND_RIGHT, K4ABT_JOINT_HANDTIP_RIGHT),
    make_pair(K4ABT_JOINT_WRIST_RIGHT, K4ABT_JOINT_THUMB_RIGHT), make_pair(K4ABT_JOINT_PELVIS, K4ABT_JOINT_HIP_RIGHT),
    make_pair(K4ABT_JOINT_HIP_RIGHT, K4ABT_JOINT_KNEE_RIGHT),
    make_pair(K4ABT_JOINT_KNEE_RIGHT, K4ABT_JOINT_ANKLE_RIGHT),
    make_pair(K4ABT_JOINT_ANKLE_RIGHT, K4ABT_JOINT_FOOT_RIGHT), make_pair(K4ABT_JOINT_NOSE, K4ABT_JOINT_EYE_RIGHT),
    make_pair(K4ABT_JOINT_EYE_RIGHT, K4ABT_JOINT_EAR_RIGHT)};

string toString(const int number, const unsigned length) noexcept
{
    string num = to_string(number);
    while (num.length() < length) {
        num.insert(0, 1, '0');
    }
    return num;
}

AzureKinect::~AzureKinect()
{
    cleanup();
}

void AzureKinect::start(const uint32_t pid) noexcept
{
    m_pid = pid;
    m_run = true;
}

void AzureKinect::stop() noexcept
{
    m_run = false;
}

void AzureKinect::shutdown() noexcept
{
    stop();
    m_shutdown = true;
    // Wait for thread to complete
    if (m_captureThread.joinable()) {
        m_captureThread.join();
    }
}

AzureKinect::KinectImage::KinectImage(
    uint8_t* const image, const int32_t width, const int32_t height, const int32_t stride)
    : m_image(image)
    , m_width(width)
    , m_height(height)
    , m_stride(stride)
{}

bool AzureKinect::init(errorCallback error, readyCallback ready, imageCallback image) noexcept
{
    // Store callbacks
    m_errorCallback = move(error);
    m_imageCallback = move(image);

    // Start capture thread running
    m_captureThread = thread(&AzureKinect::run, this, move(ready));

    return true;
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
        if (m_errorCallback != nullptr) {
            m_errorCallback("Failed to start K4A camera");
        }
        return false;
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

bool AzureKinect::initOutput() noexcept
{
    // Create output directory
    const string pidString = "PID"s + toString(m_pid, 3);
    string baseDir = "./";
    baseDir += pidString;
    baseDir += '/';
    int scanID = 1;
    error_code ec;
    while (true) {
        string scanDir = baseDir + toString(scanID, 3);
        if (!filesystem::exists(scanDir, ec)) {
            baseDir = scanDir;
            break;
        }
        ++scanID;
    }

    if (!filesystem::create_directories(baseDir, ec)) {
        if (ec && m_errorCallback != nullptr) {
            m_errorCallback("Failed creating output directory ("s + baseDir + ") with " + ec.message());
        }
        return false;
    }

    // Determine output filename
    string timeString;
    stringstream ss;
    time_t inTimeT = chrono::system_clock::to_time_t(chrono::system_clock::now());
    ss << put_time(localtime(&inTimeT), "%Y-%m-%d");
    ss >> timeString;
    string poseFile = baseDir;
    ((poseFile += '/') += pidString) += timeString;

    string videoFile = poseFile;
    videoFile += ".mkv";
    poseFile += ".csv";

    // Create pose file

    // Start recording
    // TODO:

    return true;
}

void AzureKinect::cleanupOutput() noexcept
{
    // Finalise output files
    // TODO:
}

bool AzureKinect::run(const std::function<void()>& ready) noexcept
{
    if (!initCamera()) {
        cleanup();
        return false;
    }

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
                    m_run = false;
                }
                body.id = k4abt_frame_get_body_id(bodyFrame, 0);

                // Get the body pixel positions for the first detected body
                const auto depthWidth = k4a_image_get_width_pixels(depthImage);
                const auto depthHeight = k4a_image_get_height_pixels(depthImage);
                vector<bool> bodyPixel(static_cast<size_t>(depthWidth) * depthHeight);
                const k4a_image_t indexMap = k4abt_frame_get_body_index_map(bodyFrame);
                const uint8_t* indexMapBuffer = k4a_image_get_buffer(indexMap);
                for (int i = 0; i < depthWidth * depthHeight; i++) {
                    const uint8_t bodyIndex = indexMapBuffer[i];
                    if (bodyIndex == body.id) {
                        bodyPixel[i] = true;
                    } else {
                        // K4ABT_BODY_INDEX_MAP_BACKGROUND if not a body
                        bodyPixel[i] = false;
                    }
                }
                k4a_image_release(indexMap);

                // Get the joint information
                vector<Joint> bodyJoint(K4ABT_JOINT_COUNT);
                for (auto& joint : body.skeleton.joints) {
                    if (joint.confidence_level >= K4ABT_JOINT_CONFIDENCE_LOW) {
                        const k4a_float3_t& jointPosition = joint.position;
                        const k4a_quaternion_t& jointOrientation = joint.orientation;

                        bodyJoint.emplace_back(Position{jointPosition.xyz.x, jointPosition.xyz.y, jointPosition.xyz.z},
                            Quaternion{jointOrientation.wxyz.x, jointOrientation.wxyz.y, jointOrientation.wxyz.z,
                                jointOrientation.wxyz.w},
                            joint.confidence_level >= K4ABT_JOINT_CONFIDENCE_MEDIUM);
                    }
                }

                // Visualize bones
                vector<Bone> bodyBones(s_boneList.size());
                for (auto& bone : s_boneList) {
                    const k4abt_joint_id_t joint1 = bone.first;
                    const k4abt_joint_id_t joint2 = bone.second;

                    if (body.skeleton.joints[joint1].confidence_level >= K4ABT_JOINT_CONFIDENCE_LOW &&
                        body.skeleton.joints[joint2].confidence_level >= K4ABT_JOINT_CONFIDENCE_LOW) {
                        const k4a_float3_t& joint1Position = body.skeleton.joints[joint1].position;
                        const k4a_float3_t& joint2Position = body.skeleton.joints[joint2].position;

                        bodyBones.emplace_back(
                            Position{joint1Position.xyz.x, joint1Position.xyz.y, joint1Position.xyz.z},
                            Position{joint2Position.xyz.x, joint2Position.xyz.y, joint2Position.xyz.z},
                            body.skeleton.joints[joint1].confidence_level >= K4ABT_JOINT_CONFIDENCE_MEDIUM &&
                                body.skeleton.joints[joint2].confidence_level >= K4ABT_JOINT_CONFIDENCE_MEDIUM);
                    }
                }
            }

            if (m_run) {
                if (!m_run2) {
                    if (initOutput()) {
                        m_run2 = true;
                    } else {
                        m_run = false;
                    }
                }
                if (m_run) {
                    // TODO: Write out to file

                    // Encode images
                    // TODO:
                }
            } else if (m_run2) {
                cleanupOutput();
                m_run2 = false;
            }

            // TODO: Send data to renderer
            if (m_imageCallback) {
                const k4a_image_t colourImage = k4a_capture_get_color_image(originalCapture);
                KinectImage depthPass = {k4a_image_get_buffer(depthImage), k4a_image_get_width_pixels(depthImage),
                    k4a_image_get_height_pixels(depthImage), k4a_image_get_stride_bytes(depthImage)};
                KinectImage colourPass = {k4a_image_get_buffer(colourImage), k4a_image_get_width_pixels(colourImage),
                    k4a_image_get_height_pixels(colourImage), k4a_image_get_stride_bytes(colourImage)};
                m_imageCallback(depthPass, colourPass);
                k4a_image_release(colourImage);
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
