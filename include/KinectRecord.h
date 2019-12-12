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

#include "DataTypes.h"
#include "Encoder.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <fstream>
#include <functional>
#include <mutex>
#include <thread>

namespace Ak {
class KinectRecord
{
public:
    KinectRecord() = default;

    KinectRecord(const KinectRecord& other) = delete;

    KinectRecord(KinectRecord&& other) noexcept = delete;

    KinectRecord& operator=(const KinectRecord& other) = delete;

    KinectRecord& operator=(KinectRecord&& other) noexcept = delete;

    ~KinectRecord();

    using errorCallback = std::function<void(const std::string&)>;

    /**
     * Initializes the data recorder.
     * @param error (Optional) The callback used to signal errors.
     * @returns True if it succeeds, false if it fails.
     */
    bool init(errorCallback error = nullptr);

    /**
     * Notify to start acquisition.
     * @note This function is asynchronous and may not result in an immediate start.
     * @param pid The player ID used to identify output data.
     */
    void start(uint32_t pid) noexcept;

    /** Notify to end recording.
     * @note This function is asynchronous and may not result in an immediate shutdown.
     */
    void stop() noexcept;

    /** Notify to shutdown.
     * @note This function is synchronous and will block until thread has completed.
     */
    void shutdown() noexcept;

    /**
     * Callback used by the camera thread when new image information is available.
     * @param time        The timestamp of the capture.
     * @param depthImage  The depth image data.
     * @param colourImage The colour image data.
     * @param irImage     The IR image data.
     * @param shadowImage The body shadow image data.
     * @param joints      The joint data.
     */
    void dataCallback(uint64_t time, const KinectImage& depthImage, const KinectImage& colourImage,
        const KinectImage& irImage, const KinectImage& shadowImage, const KinectJoints& joints) noexcept;

    /**
     * Sets what types of data should be recorded.
     * @param depthImage   True to render depth image.
     * @param colourImage  True to render colour image.
     * @param irImage      True to render IR image.
     * @param bodySkeleton True to render body skeleton.
     */
    void setRecordOptions(bool depthImage, bool colourImage, bool irImage, bool bodySkeleton) noexcept;

    /**
     * Updates the calibration information for the camera
     * @param calibration The calibration data.
     */
    void updateCalibration(const KinectCalibration& calibration) noexcept;

private:
    std::atomic_bool m_shutdown = false;
    std::atomic_bool m_run = false;
    std::atomic_bool m_run2 = false;
    std::mutex m_lock;
    std::condition_variable m_condition;

    struct DataBuffers
    {
        uint64_t m_timeStamp;
        std::vector<Joint> m_joints;
    };

    std::array<DataBuffers, 16 /*must be power of 2*/> m_dataBuffer;
    std::atomic_uint32_t m_bufferIndex = 0;
    std::atomic_int32_t m_remainingBuffers = 0;
    uint32_t m_nextBufferIndex = 0;
    bool m_depthImage = true;
    bool m_colourImage = false;
    bool m_irImage = false;
    bool m_bodySkeleton = true;
    std::ofstream m_skeletonFile;
    std::atomic_uint32_t m_pid = 0;
    std::array<Encoder, 3> m_encoders;
    std::thread m_recordThread;
    errorCallback m_errorCallback = nullptr;
    KinectCalibration m_calibration;

    /**
     * Initializes the output files for recording.
     * @returns True if it succeeds, false if it fails.
     */
    [[nodiscard]] bool initOutput() noexcept;

    /** Cleanup output files opened during @initOutput. */
    void cleanupOutput() noexcept;

    /**
     * Run data recording and processing.
     * @note init() must be called before this function can be used.
     * @returns True if it succeeds, false if it fails.
     */
    [[nodiscard]] bool run() noexcept;
};
} // namespace Ak
