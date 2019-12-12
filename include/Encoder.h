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

#include "Filter.h"

#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/rational.h>
}

namespace Ak {
class OutputFormatContextPtr
{
    friend class Encoder;

    OutputFormatContextPtr() = default;

    explicit OutputFormatContextPtr(AVFormatContext* formatContext) noexcept;

    [[nodiscard]] AVFormatContext* get() const noexcept;

    AVFormatContext* operator->() const noexcept;

    std::shared_ptr<AVFormatContext> m_formatContext = nullptr;
};

class CodecContextPtr
{
public:
    CodecContextPtr() = default;

    explicit CodecContextPtr(AVCodecContext* codecContext) noexcept;

    [[nodiscard]] AVCodecContext* get() const noexcept;

    AVCodecContext* operator->() const noexcept;

    std::shared_ptr<AVCodecContext> m_codecContext = nullptr;
};

class FramePtr
{
public:
    FramePtr() noexcept = default;

    explicit FramePtr(AVFrame* frame) noexcept;

    [[nodiscard]] AVFrame* get() const noexcept;

    const AVFrame* operator->() const noexcept;

    std::shared_ptr<AVFrame> m_frame = nullptr;
};

class Encoder
{
public:
    using errorCallback = std::function<void(const std::string&)>;

    Encoder() = default;

    ~Encoder();

    Encoder(const Encoder& other) = delete;

    Encoder(Encoder&& other) noexcept = delete;

    Encoder& operator=(const Encoder& other) = delete;

    Encoder& operator=(Encoder&& other) noexcept = delete;

    /**
     * Initialise the encoder.
     * @param filename   Filename of the file.
     * @param width      The input width.
     * @param height     The input height.
     * @param fps        The input FPS.
     * @param format     The input frame pixel format.
     * @param scale      The scale that needs to be applied to input pixels.
     * @param numThreads Number of threads to use.
     * @param error      (Optional) The callback used to signal errors.
     * @returns True if it succeeds, false if it fails.
     */
    bool init(const std::string& filename, uint32_t width, uint32_t height, uint32_t fps, int32_t format, float scale,
        uint32_t numThreads, errorCallback error = nullptr) noexcept;

    /**
     * Adds a frame to be processed.
     * @param [in] data   The image data.
     * @param      width  The image width.
     * @param      height The image height.
     * @param      stride The image stride.
     * @returns True if it succeeds, false if it fails.
     */
    bool addFrame(uint8_t* data, uint32_t width, uint32_t height, uint32_t stride) noexcept;

    /** Notify to shutdown.
     * @note This function is synchronous and will block until thread has completed.
     */
    void shutdown() noexcept;

private:
    std::atomic_bool m_shutdown = false;
    std::mutex m_lock;
    std::condition_variable m_condition;

    std::array<FramePtr, 16 /*must be power of 2*/> m_dataBuffer;
    std::atomic_uint32_t m_bufferIndex = 0;
    std::atomic_int32_t m_remainingBuffers = 0;
    uint32_t m_nextBufferIndex = 0;
    int32_t m_format = 0;
    uint64_t m_frameNumber = 0;

    OutputFormatContextPtr m_formatContext;
    CodecContextPtr m_codecContext;
    AVRational m_timebase;
    Filter m_filter;
    std::thread m_thread;
    errorCallback m_errorCallback = nullptr;

    /**
     * Initializes the output files and encoders/filters.
     * @param filename   Filename of the file.
     * @param width      The input width.
     * @param height     The input height.
     * @param format     The input frame pixel format.
     * @param scale      The scale that needs to be applied to input pixels.
     * @param numThreads Number of threads to use.
     * @returns True if it succeeds, false if it fails.
     */
    [[nodiscard]] bool initOutput(const std::string& filename, uint32_t width, uint32_t height, int32_t format,
        float scale, uint32_t numThreads) noexcept;

    /** Cleanup output files opened during @initOutput. */
    void cleanupOutput() noexcept;

    /**
     * Run image recording and processing.
     * @note init() must be called before this function can be used.
     * @returns True if it succeeds, false if it fails.
     */
    [[nodiscard]] bool run() noexcept;

    /**
     * Process any pending frames.
     * @returns True if it succeeds, false if it fails.
     */
    bool process() noexcept;

    /**
     * Process the input frame.
     * @param [in,out] frame The frame.
     * @returns True if it succeeds, false if it fails.
     */
    bool processFrame(FramePtr& frame) const noexcept;

    /**
     * Encode frame.
     * @param frame The frame.
     * @returns True if it succeeds, false if it fails.
     */
    [[nodiscard]] bool encodeFrame(const FramePtr& frame) const noexcept;

    /**
     * Writes encoded frames to output.
     * @returns True if it succeeds, false if it fails.
     */
    [[nodiscard]] bool muxFrames() const noexcept;
};
} // namespace Ak
