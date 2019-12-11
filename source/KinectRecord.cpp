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

#include "KinectRecord.h"

#include <array>
#include <filesystem>
#include <k4abttypes.h>
#include <sstream>
#include <string>
#include <system_error>
using namespace std;

namespace Ak {
// Define the joint string names
static array<pair<k4abt_joint_id_t, std::string>, 32> s_jointNames = {std::make_pair(K4ABT_JOINT_PELVIS, "PELVIS"),
    std::make_pair(K4ABT_JOINT_SPINE_NAVAL, "SPINE_NAVAL"), std::make_pair(K4ABT_JOINT_SPINE_CHEST, "SPINE_CHEST"),
    std::make_pair(K4ABT_JOINT_NECK, "NECK"), std::make_pair(K4ABT_JOINT_CLAVICLE_LEFT, "CLAVICLE_LEFT"),
    std::make_pair(K4ABT_JOINT_SHOULDER_LEFT, "SHOULDER_LEFT"), std::make_pair(K4ABT_JOINT_ELBOW_LEFT, "ELBOW_LEFT"),
    std::make_pair(K4ABT_JOINT_WRIST_LEFT, "WRIST_LEFT"), std::make_pair(K4ABT_JOINT_HAND_LEFT, "HAND_LEFT"),
    std::make_pair(K4ABT_JOINT_HANDTIP_LEFT, "HANDTIP_LEFT"), std::make_pair(K4ABT_JOINT_THUMB_LEFT, "THUMB_LEFT"),
    std::make_pair(K4ABT_JOINT_CLAVICLE_RIGHT, "CLAVICLE_RIGHT"),
    std::make_pair(K4ABT_JOINT_SHOULDER_RIGHT, "SHOULDER_RIGHT"),
    std::make_pair(K4ABT_JOINT_ELBOW_RIGHT, "ELBOW_RIGHT"), std::make_pair(K4ABT_JOINT_WRIST_RIGHT, "WRIST_RIGHT"),
    std::make_pair(K4ABT_JOINT_HAND_RIGHT, "HAND_RIGHT"), std::make_pair(K4ABT_JOINT_HANDTIP_RIGHT, "HANDTIP_RIGHT"),
    std::make_pair(K4ABT_JOINT_THUMB_RIGHT, "THUMB_RIGHT"), std::make_pair(K4ABT_JOINT_HIP_LEFT, "HIP_LEFT"),
    std::make_pair(K4ABT_JOINT_KNEE_LEFT, "KNEE_LEFT"), std::make_pair(K4ABT_JOINT_ANKLE_LEFT, "ANKLE_LEFT"),
    std::make_pair(K4ABT_JOINT_FOOT_LEFT, "FOOT_LEFT"), std::make_pair(K4ABT_JOINT_HIP_RIGHT, "HIP_RIGHT"),
    std::make_pair(K4ABT_JOINT_KNEE_RIGHT, "KNEE_RIGHT"), std::make_pair(K4ABT_JOINT_ANKLE_RIGHT, "ANKLE_RIGHT"),
    std::make_pair(K4ABT_JOINT_FOOT_RIGHT, "FOOT_RIGHT"), std::make_pair(K4ABT_JOINT_HEAD, "HEAD"),
    std::make_pair(K4ABT_JOINT_NOSE, "NOSE"), std::make_pair(K4ABT_JOINT_EYE_LEFT, "EYE_LEFT"),
    std::make_pair(K4ABT_JOINT_EAR_LEFT, "EAR_LEFT"), std::make_pair(K4ABT_JOINT_EYE_RIGHT, "EYE_RIGHT"),
    std::make_pair(K4ABT_JOINT_EAR_RIGHT, "EAR_RIGHT")};

string toString(const int number, const unsigned length) noexcept
{
    string num = to_string(number);
    while (num.length() < length) {
        num.insert(0, 1, '0');
    }
    return num;
}

KinectRecord::~KinectRecord()
{
    shutdown();
    cleanup();
}

bool KinectRecord::init(errorCallback error)
{
    // Store callbacks
    m_errorCallback = move(error);

    // Reserve initial memory
    for (auto& i : m_dataBuffer) {
        i.m_joints.reserve(40);
    }

    // Start capture thread running
    m_recordThread = thread(&KinectRecord::run, this);

    return true;
}

void KinectRecord::start(const uint32_t pid) noexcept
{
    m_pid = pid;
    {
        lock_guard<mutex> lock(m_lock);
        m_run = true;
    }
    // Notify wakeup
    m_condition.notify_one();
}

void KinectRecord::stop() noexcept
{
    {
        lock_guard<mutex> lock(m_lock);
        m_run = false;
    }
    // Notify wakeup
    m_condition.notify_one();
}

void KinectRecord::shutdown() noexcept
{
    stop();
    {
        lock_guard<mutex> lock(m_lock);
        m_shutdown = true;
    }
    // Notify wakeup
    m_condition.notify_one();
    // Wait for thread to complete
    if (m_recordThread.joinable()) {
        m_recordThread.join();
    }
}

void KinectRecord::dataCallback(const uint64_t time, const KinectImage& depthImage, const KinectImage& colourImage,
    const KinectImage& irImage, const KinectImage&, const KinectJoints& joints) noexcept
{
    unique_lock<mutex> lock(m_lock);
    if (m_run && m_run2) {
        m_processEncode = false;
        lock.unlock();
        bool validData = false;
        // Only write out data when running and setup has completed

        // Copy data into local
        const uint32_t bufferMod = m_bufferIndex % m_dataBuffer.size();

        m_dataBuffer[bufferMod].m_timeStamp = time;
        if (m_depthImage) {
            if (depthImage.m_image != nullptr) {
                if (!m_encoders[0].addFrame(
                        depthImage.m_image, depthImage.m_width, depthImage.m_height, depthImage.m_stride)) {
                    return;
                }
                m_processEncode = true;
            }
        }
        if (m_colourImage) {
            if (colourImage.m_image != nullptr) {
                if (!m_encoders[1].addFrame(
                        colourImage.m_image, colourImage.m_width, colourImage.m_height, colourImage.m_stride)) {
                    return;
                }
                m_processEncode = true;
            }
        }
        if (m_irImage) {
            if (irImage.m_image != nullptr) {
                if (!m_encoders[2].addFrame(irImage.m_image, irImage.m_width, irImage.m_height, irImage.m_stride)) {
                    return;
                }
                m_processEncode = true;
            }
        }

        if (m_bodySkeleton) {
            if (joints.m_length > 0) {
                m_dataBuffer[bufferMod].m_joints.resize(0);
                m_dataBuffer[bufferMod].m_joints.insert(m_dataBuffer[bufferMod].m_joints.begin(), joints.m_joints,
                    joints.m_joints + static_cast<size_t>(joints.m_length));
                validData = true;
            }
        }
        if (validData) {
            lock.lock();
            ++m_bufferIndex;
            ++m_remainingBuffers;
            if (m_remainingBuffers == static_cast<int32_t>(m_dataBuffer.size()) - 1) {
                // Error buffer overflow
                m_run = false;
                m_errorCallback("Write buffer has overflowed"s);
            }
            lock.unlock();
        }

        // Notify wakeup
        if (validData || m_processEncode) {
            m_condition.notify_one();
        }
    }
}

void KinectRecord::setRecordOptions(
    const bool depthImage, const bool colourImage, const bool irImage, const bool bodySkeleton) noexcept
{
    m_depthImage = depthImage;
    m_colourImage = colourImage;
    m_irImage = irImage;
    m_bodySkeleton = bodySkeleton;
}

void KinectRecord::updateCalibration(const KinectCalibration& calibration) noexcept
{
    m_calibration = calibration;
}

bool KinectRecord::initOutput() noexcept
{
    // Close any existing recordings
    cleanupOutput();

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
    poseFile += ".csv";

    if (m_bodySkeleton) {
        // Create pose file
        m_skeletonFile.open(poseFile, ios::binary);
        if (!m_skeletonFile.is_open()) {
            return false;
        }

        // Write out column names
        m_skeletonFile << "Timestamp,";
        for (auto& i : s_jointNames) {
            m_skeletonFile << i.second << "X,";
            m_skeletonFile << i.second << "Y,";
            m_skeletonFile << i.second << "Z,";
            m_skeletonFile << i.second << "RX,";
            m_skeletonFile << i.second << "RY,";
            m_skeletonFile << i.second << "RZ,";
            m_skeletonFile << i.second << "RW,";
        }
        m_skeletonFile.flush();
    }

    // Start recording
    if (m_depthImage || m_colourImage || m_irImage) {
        uint32_t numThreads = std::max(static_cast<uint32_t>((std::thread::hardware_concurrency() - 4) /
                                           (m_depthImage + m_colourImage + m_irImage)),
            1U);
        numThreads = std::min(numThreads, 8U);

        if (m_depthImage) {
            const float scale =
                65536.0f / static_cast<float>(m_calibration.m_depthRange.y - m_calibration.m_depthRange.x);
            if (!m_encoders[0].init(videoFile + "_depth.mp4", m_calibration.m_depthDimensions.x,
                    m_calibration.m_depthDimensions.y, m_calibration.m_fps, AV_PIX_FMT_GRAY16LE, scale, numThreads,
                    m_errorCallback)) {
                cleanupOutput();
                return false;
            }
        }
        if (m_colourImage) {
            if (!m_encoders[1].init(videoFile + "_colour.mp4", m_calibration.m_colourDimensions.x,
                    m_calibration.m_colourDimensions.y, m_calibration.m_fps, AV_PIX_FMT_BGRA, 1.0f, numThreads,
                    m_errorCallback)) {
                cleanupOutput();
                return false;
            }
        }
        if (m_irImage) {
            const float scale = 65536.0f / static_cast<float>(m_calibration.m_irRange.y - m_calibration.m_irRange.x);
            if (!m_encoders[2].init(videoFile + "_ir.mp4", m_calibration.m_irDimensions.x,
                    m_calibration.m_irDimensions.y, m_calibration.m_fps, AV_PIX_FMT_GRAY16LE, scale, numThreads,
                    m_errorCallback)) {
                cleanupOutput();
                return false;
            }
        }
    }

    return true;
}

void KinectRecord::cleanupOutput() noexcept
{
    // Finalise output files
    if (m_skeletonFile.is_open()) {
        m_skeletonFile.close();
    }
    for (auto& i : m_encoders) {
        i.shutdown();
    }
}

bool KinectRecord::run() noexcept
{
    while (true) {
        // Wait until we receive start notification (or shutdown)
        {
            unique_lock<mutex> lock(m_lock);
            if (!m_run && !m_shutdown) {
                m_condition.wait(lock, [this] { return m_run || m_shutdown; });
            }
            if (m_shutdown) {
                break;
            }
            lock.unlock();
        }
        // State is currently set to run, initialise output
        if (!initOutput()) {
            lock_guard<mutex> lock(m_lock);
            m_run = false;
            continue;
        }
        while (true) {
            // Wait until we have received valid data
            {
                unique_lock<mutex> lock(m_lock);
                m_run2 = true;
                if (m_run && !m_shutdown) {
                    m_condition.wait(lock, [this] {
                        return (m_run && (m_remainingBuffers > 0 || m_processEncode)) || m_shutdown ||
                            (!m_run && m_run2);
                    });
                }
                if (!m_run || m_shutdown) {
                    break;
                }
            }
            if (m_bodySkeleton) {
                // Write out to file
                while (true) {
                    {
                        lock_guard<mutex> lock(m_lock);
                        if (m_remainingBuffers == 0) {
                            break;
                        }
                        --m_remainingBuffers;
                    }
                    m_skeletonFile << "\r\n";
                    m_skeletonFile << m_dataBuffer[m_nextBufferIndex].m_timeStamp << ',';
                    for (auto& i : s_jointNames) {
                        m_skeletonFile << m_dataBuffer[m_nextBufferIndex].m_joints[i.first].m_position.m_position.x
                                       << ',';
                        m_skeletonFile << m_dataBuffer[m_nextBufferIndex].m_joints[i.first].m_position.m_position.y
                                       << ',';
                        m_skeletonFile << m_dataBuffer[m_nextBufferIndex].m_joints[i.first].m_position.m_position.z
                                       << ',';
                        m_skeletonFile << m_dataBuffer[m_nextBufferIndex].m_joints[i.first].m_rotation.m_rotation.x
                                       << ',';
                        m_skeletonFile << m_dataBuffer[m_nextBufferIndex].m_joints[i.first].m_rotation.m_rotation.y
                                       << ',';
                        m_skeletonFile << m_dataBuffer[m_nextBufferIndex].m_joints[i.first].m_rotation.m_rotation.z
                                       << ',';
                        m_skeletonFile << m_dataBuffer[m_nextBufferIndex].m_joints[i.first].m_rotation.m_rotation.w
                                       << ',';
                    }
                    ++m_nextBufferIndex;
                    m_nextBufferIndex = m_nextBufferIndex < m_dataBuffer.size() ? m_nextBufferIndex : 0;
                    m_skeletonFile.flush();
                }
            }

            // Encode images
            if (m_depthImage) {
                if (!m_encoders[0].process()) {
                    lock_guard<mutex> lock(m_lock);
                    break;
                }
            }
            if (m_colourImage) {
                if (!m_encoders[1].process()) {
                    lock_guard<mutex> lock(m_lock);
                    break;
                }
            }
            if (m_irImage) {
                if (!m_encoders[2].process()) {
                    lock_guard<mutex> lock(m_lock);
                    break;
                }
            }
        }
        // Cleanup current run
        {
            unique_lock<mutex> lock(m_lock);
            m_run2 = false;
        }
        cleanupOutput();
    }

    // Cleanup all data
    cleanup();

    return true;
}

void KinectRecord::cleanup() noexcept
{
    cleanupOutput();
}
} // namespace Ak
