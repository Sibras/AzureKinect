﻿/**
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

#include "Encoder.h"

#include "Filter.h"

extern "C" {
#include <libavfilter/avfilter.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
}

using namespace std;

namespace Ak {
extern void logHandler(const std::string& message);

std::string getFfmpegErrorString(const int errorCode) noexcept
{
    char buffer[AV_ERROR_MAX_STRING_SIZE];
    av_make_error_string(buffer, AV_ERROR_MAX_STRING_SIZE, errorCode);
    return buffer;
}

static int s_prefix = 1;

void logCallback(void* avclass, const int level, const char* format, const va_list vl)
{
    char buffer[1024];
    av_log_format_line(avclass, level, format, vl, buffer, 1024, &s_prefix);
    logHandler(buffer);
}

OutputFormatContextPtr::OutputFormatContextPtr(AVFormatContext* formatContext) noexcept
    : m_formatContext(formatContext, [](AVFormatContext* p) { avformat_free_context(p); })
{}

AVFormatContext* OutputFormatContextPtr::get() const noexcept
{
    return m_formatContext.get();
}

AVFormatContext* OutputFormatContextPtr::operator->() const noexcept
{
    return m_formatContext.get();
}

CodecContextPtr::CodecContextPtr(AVCodecContext* codecContext) noexcept
    : m_codecContext(codecContext, [](AVCodecContext* p) { avcodec_free_context(&p); })
{}

AVCodecContext* CodecContextPtr::operator->() const noexcept
{
    return m_codecContext.get();
}

AVCodecContext* CodecContextPtr::get() const noexcept
{
    return m_codecContext.get();
}

FramePtr::FramePtr(AVFrame* frame) noexcept
    : m_frame(frame, [](AVFrame* p) { av_frame_free(&p); })
{}

AVFrame* FramePtr::get() const noexcept
{
    return m_frame.get();
}

const AVFrame* FramePtr::operator->() const noexcept
{
    return m_frame.get();
}

DevicePtr::DevicePtr(AVBufferRef* device) noexcept
    : m_device(device, [](AVBufferRef* p) { av_buffer_unref(&p); })
{}

AVBufferRef* DevicePtr::get() const noexcept
{
    return m_device.get();
}

const AVBufferRef* DevicePtr::operator->() const noexcept
{
    return m_device.get();
}

Encoder::~Encoder()
{
    shutdown();
    cleanupOutput();
}

bool Encoder::init(const string& filename, const uint32_t width, const uint32_t height, const uint32_t fps,
    const int32_t format, const float scale, const uint32_t numThreads, const bool useGPU, errorCallback error) noexcept
{
    m_errorCallback = move(error);
    m_format = format;
    m_timebase = {1, static_cast<int32_t>(fps)};
    m_shutdown = false;
    m_useGPU = useGPU;

    // Set the ffmpeg callback for receiving log messages
#ifdef _DEBUG
    av_log_set_level(AV_LOG_INFO);
#else
    av_log_set_level(AV_LOG_ERROR);
#endif
    av_log_set_callback(logCallback);

    // Initialise the output
    if (!initOutput(filename, width, height, format, scale, numThreads)) {
        return false;
    }

    // Start encode thread running
    m_thread = thread(&Encoder::run, this);

    return true;
}

bool Encoder::addFrame(uint8_t* data, const uint32_t width, const uint32_t height, const uint32_t stride) noexcept
{
    // Copy data into local
    const uint32_t bufferMod = m_bufferIndex % m_dataBuffer.size();

    // Create ffmpeg frame
    FramePtr frame2(av_frame_alloc());
    if (frame2.get() == nullptr) {
        if (m_errorCallback != nullptr) {
            m_errorCallback("Failed to allocate new host frame"s);
        }
        return false;
    }
    frame2.m_frame->format = m_format;
    frame2.m_frame->height = height;
    frame2.m_frame->width = width;
    auto ret = av_frame_get_buffer(frame2.get(), 0);
    if (ret < 0) {
        if (m_errorCallback != nullptr) {
            m_errorCallback("Failed allocating new frame storage, "s += getFfmpegErrorString(ret));
        }
        return false;
    }
    const uint32_t pixSize = m_format == AV_PIX_FMT_BGRA ? 4 : 2; // TODO: Use ffmpeg internal functions for this
    uint8_t* srcData[4];
    int32_t srcLine[4];
    // Determine input buffer alignment
    int32_t align = 1;
    for (; align <= static_cast<int32_t>(av_cpu_max_align()); align += align) {
        if (FFALIGN(width * pixSize, align) == stride) {
            break;
        }
    }

    // Copy data into new frame
    ret = av_image_fill_arrays(srcData, srcLine, data, static_cast<AVPixelFormat>(frame2.m_frame->format),
        frame2.m_frame->width, frame2.m_frame->height, align);
    if (ret < 0) {
        if (m_errorCallback != nullptr) {
            m_errorCallback("Failed to copy new frame, "s += getFfmpegErrorString(ret));
        }
        return false;
    }

    const uint8_t* srcData2[4];
    memcpy(srcData2, srcData, sizeof(srcData2));
    av_image_copy(frame2.m_frame->data, frame2.m_frame->linesize, srcData2, srcLine,
        static_cast<AVPixelFormat>(frame2.m_frame->format), frame2.m_frame->width, frame2.m_frame->height);

    // Fill in frame number
    frame2.m_frame->best_effort_timestamp = m_frameNumber++;
    frame2.m_frame->display_picture_number = static_cast<int32_t>(frame2.m_frame->best_effort_timestamp);
    frame2.m_frame->pkt_dts = frame2.m_frame->best_effort_timestamp;
    frame2.m_frame->pts = frame2.m_frame->best_effort_timestamp;
    frame2.m_frame->sample_aspect_ratio = {1, 1};

    // Place frame on pending stack
    m_dataBuffer[bufferMod] = move(frame2);
    ++m_bufferIndex;
    {
        lock_guard<mutex> lock(m_lock);
        ++m_remainingBuffers;
        if (m_remainingBuffers == static_cast<int32_t>(m_dataBuffer.size())) {
            // Error buffer overflow
            m_errorCallback("Encode buffer has overflowed"s);
            return false;
        }
    }
    // Notify wakeup
    m_condition.notify_one();

    return true;
}

void Encoder::shutdown() noexcept
{
    {
        lock_guard<mutex> lock(m_lock);
        m_shutdown = true;
    }
    // Notify wakeup
    m_condition.notify_one();
    // Wait for thread to complete
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

bool Encoder::initOutput(const string& filename, const uint32_t width, const uint32_t height, const int32_t format,
    const float scale, const uint32_t numThreads) noexcept
{
    // Initialise the filter for pixel conversion
    if (!m_filter.init(width, height, av_inv_q(m_timebase), format, scale, numThreads, m_errorCallback)) {
        return false;
    }

    AVFormatContext* formatPtr = nullptr;
    auto ret = avformat_alloc_output_context2(&formatPtr, nullptr, nullptr, filename.c_str());
    OutputFormatContextPtr tempFormat(formatPtr);
    if (ret < 0) {
        if (m_errorCallback != nullptr) {
            m_errorCallback("Failed to open output stream "s += getFfmpegErrorString(ret));
        }
        return false;
    }
    const auto outStream = avformat_new_stream(tempFormat.get(), nullptr);
    if (outStream == nullptr) {
        if (m_errorCallback != nullptr) {
            m_errorCallback("Failed to create an output stream"s);
        }
        return false;
    }

    // Find the required encoder
    AVCodec* encoder;
    DevicePtr tempDevice;
    if (m_useGPU) {
        AVBufferRef* devicePtr = nullptr;
        ret = av_hwdevice_ctx_create(&devicePtr, AV_HWDEVICE_TYPE_CUDA, nullptr, nullptr, 0);
        tempDevice = DevicePtr(devicePtr);
        if (ret < 0) {
            if (m_errorCallback != nullptr) {
                m_errorCallback("Failed to create NVEncoder device "s += getFfmpegErrorString(ret));
            }
            return false;
        }

        encoder = avcodec_find_encoder_by_name("h264_nvenc");
    } else {
        encoder = avcodec_find_encoder(AV_CODEC_ID_H264);
    }
    if (!encoder) {
        if (m_errorCallback != nullptr) {
            m_errorCallback("Requested encoder is not supported");
        }
        return false;
    }
    CodecContextPtr tempCodec(avcodec_alloc_context3(encoder));
    if (tempCodec.get() == nullptr) {
        if (m_errorCallback != nullptr) {
            m_errorCallback("Failed allocating encoder context");
        }
        return false;
    }

    // Setup encoding parameters
    tempCodec->height = m_filter.getHeight();
    tempCodec->width = m_filter.getWidth();
    tempCodec->sample_aspect_ratio = {1, 1};
    tempCodec->pix_fmt = m_useGPU ? AV_PIX_FMT_CUDA : m_filter.getPixelFormat();
    tempCodec->framerate = m_filter.getFrameRate();
    tempCodec->time_base = av_inv_q(tempCodec->framerate);
    av_opt_set_int(tempCodec.get(), "refcounted_frames", 1, 0);

    if (tempFormat->oformat->flags & AVFMT_GLOBALHEADER) {
        tempCodec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    // Setup the desired encoding options
    AVDictionary* opts = nullptr;
    if (m_useGPU) {
        AVBufferRef* framesRef;
        framesRef = av_hwframe_ctx_alloc(tempDevice.get());
        if (!framesRef) {
            if (m_errorCallback != nullptr) {
                m_errorCallback("Failed to create NVEncoder frame context"s);
            }
            return false;
        }
        auto* frames = reinterpret_cast<AVHWFramesContext*>(framesRef->data);
        frames->format = AV_PIX_FMT_CUDA;
        frames->sw_format = m_filter.getPixelFormat();
        frames->width = width;
        frames->height = height;
        frames->initial_pool_size = static_cast<int>(m_dataBuffer.size());

        ret = av_hwframe_ctx_init(framesRef);
        if (ret < 0) {
            if (m_errorCallback != nullptr) {
                m_errorCallback("Failed to initialise NVEncoder frame context "s += getFfmpegErrorString(ret));
            }
            av_buffer_unref(&framesRef);
            return false;
        }
        tempCodec->hw_frames_ctx = av_buffer_ref(framesRef);
        av_buffer_unref(&framesRef);
        if (!tempCodec->hw_frames_ctx) {
            if (m_errorCallback != nullptr) {
                m_errorCallback("Failed to attach NVEncoder frame context"s);
            }
            return false;
        }

        av_dict_set(&opts, "rc", "vbr", 0);
        av_dict_set(&opts, "cq", to_string(23).c_str(), 0);
        av_dict_set(&opts, "preset", "llhp", 0);
    } else {
        av_dict_set(&opts, "crf", to_string(23).c_str(), 0);
        av_dict_set(&opts, "preset", "veryfast", 0);

        if (numThreads != 0) {
            av_dict_set(&opts, "threads", to_string(numThreads).c_str(), 0);
        }
    }

    // Open the encoder
    ret = avcodec_open2(tempCodec.get(), encoder, &opts);
    if (ret < 0) {
        if (m_errorCallback != nullptr) {
            m_errorCallback("Failed opening video encoder: "s += getFfmpegErrorString(ret));
        }
        return false;
    }
    ret = avcodec_parameters_from_context(outStream->codecpar, tempCodec.get());
    if (ret < 0) {
        if (m_errorCallback != nullptr) {
            m_errorCallback("Failed copying parameters to encoder context: "s += getFfmpegErrorString(ret));
        }
        return false;
    }

    // Set the output stream timebase
    outStream->time_base = tempCodec->time_base;
    outStream->r_frame_rate = tempCodec->framerate;
    outStream->avg_frame_rate = tempCodec->framerate;

    // Open output file if required
    if (!(tempFormat->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&tempFormat->pb, filename.c_str(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            if (m_errorCallback != nullptr) {
                m_errorCallback(("Failed to open output file: "s += filename) += ", "s += getFfmpegErrorString(ret));
            }
            return false;
        }
    }

    // Init the muxer and write out file header
    ret = avformat_write_header(tempFormat.get(), nullptr);
    if (ret < 0) {
        if (m_errorCallback != nullptr) {
            m_errorCallback(
                ("Failed writing header to output file: "s += filename) += ", "s += getFfmpegErrorString(ret));
        }
        return false;
    }

    // Make the new encoder
    m_formatContext = move(tempFormat);
    m_codecContext = move(tempCodec);
    m_bufferIndex = 0;
    m_remainingBuffers = 0;
    m_nextBufferIndex = 0;
    m_frameNumber = 0;

    return true;
}

void Encoder::cleanupOutput() noexcept
{
    // Flush any remaining frames
    if (m_filter.m_filterGraph.m_filterGraph != nullptr) {
        FramePtr temp(nullptr);
        while (processFrame(temp)) {
        }
    }

    // Finalise the encoder
    if (m_formatContext.m_formatContext != nullptr) {
        FramePtr temp2(nullptr);
        (void)encodeFrame(temp2);
        m_codecContext = CodecContextPtr(nullptr);
        m_formatContext = OutputFormatContextPtr(nullptr);
    }
}

bool Encoder::run() noexcept
{
    while (true) {
        // Wait until we have received valid data
        {
            unique_lock<mutex> lock(m_lock);
            if (!m_shutdown) {
                m_condition.wait(lock, [this] { return (m_remainingBuffers > 0) || m_shutdown; });
            }
            if (m_shutdown) {
                break;
            }
        }
        // Process pending frames
        if (!process()) {
            break;
        }
    }
    // Cleanup
    cleanupOutput();

    return true;
}

bool Encoder::process() noexcept
{
    // Get frame to be processed
    while (true) {
        {
            unique_lock<mutex> lock(m_lock);
            if (m_remainingBuffers == 0) {
                break;
            }
            --m_remainingBuffers;
        }
        FramePtr frame(move(m_dataBuffer[m_nextBufferIndex]));
        ++m_nextBufferIndex;
        m_nextBufferIndex = m_nextBufferIndex < m_dataBuffer.size() ? m_nextBufferIndex : 0;

        // Process new frame
        if (!processFrame(frame)) {
            return false;
        }
    }
    return true;
}

bool Encoder::processFrame(FramePtr& frame) const noexcept
{
    // Pass into filter chain
    if (!m_filter.sendFrame(frame)) {
        av_frame_unref(frame.get());
        return false;
    }

    // Run filter chain
    if (!m_filter.receiveFrame(frame)) {
        av_frame_unref(frame.get());
        return false;
    }

    // Pass to encoder
    if (!encodeFrame(frame)) {
        return false;
    }

    return true;
}

bool Encoder::encodeFrame(FramePtr& frame) const noexcept
{
    if (frame.m_frame != nullptr) {
        if (m_useGPU) {
            // Create a new frame to hold device memory
            FramePtr frame2(av_frame_alloc());
            if (frame2.get() == nullptr) {
                if (m_errorCallback != nullptr) {
                    m_errorCallback("Failed to allocate new device frame"s);
                }
                return false;
            }

            // Create a new buffer for the hardware frame
            auto ret = av_hwframe_get_buffer(m_codecContext->hw_frames_ctx, frame2.get(), 0);
            if (ret < 0) {
                if (m_errorCallback != nullptr) {
                    m_errorCallback("Failed to get device frame storage, "s += getFfmpegErrorString(ret));
                }
                return false;
            }
            if (!frame2->hw_frames_ctx) {
                if (m_errorCallback != nullptr) {
                    m_errorCallback("Failed to init device frame storage");
                }
                return false;
            }

            // Transfer data from host to device
            ret = av_hwframe_transfer_data(frame2.get(), frame.get(), 0);
            if (ret < 0) {
                if (m_errorCallback != nullptr) {
                    m_errorCallback("Failed to transfer device frame, "s += getFfmpegErrorString(ret));
                }
                return false;
            }
            frame2.get()->best_effort_timestamp = frame.get()->best_effort_timestamp;
            frame = move(frame2);
        }

        // Send frame to encoder
        frame.m_frame->best_effort_timestamp =
            av_rescale_q(frame.get()->best_effort_timestamp, m_timebase, m_codecContext->time_base);
        frame.m_frame->pts = frame.get()->best_effort_timestamp;
        const auto ret = avcodec_send_frame(m_codecContext.get(), frame.get());
        if (ret < 0) {
            if (m_errorCallback != nullptr) {
                m_errorCallback("Failed to send packet to encoder: "s += getFfmpegErrorString(ret));
            }
            return false;
        }
        if (!muxFrames()) {
            return false;
        }
    } else {
        // Send a flush frame
        auto ret = avcodec_send_frame(m_codecContext.get(), nullptr);
        if (ret < 0) {
            if (m_errorCallback != nullptr) {
                m_errorCallback("Failed to send flush packet to encoder: "s += getFfmpegErrorString(ret));
            }
            return false;
        }
        if (!muxFrames()) {
            return false;
        }
        av_interleaved_write_frame(m_formatContext.get(), nullptr);
        ret = av_write_trailer(m_formatContext.get());
        if (ret < 0) {
            if (m_errorCallback != nullptr) {
                m_errorCallback("Failed to write file trailer: "s += getFfmpegErrorString(ret));
            }
            return false;
        }
    }
    return true;
}

bool Encoder::muxFrames() const noexcept
{
    // Get all encoder packets
    AVPacket packet;
    while (true) {
        packet.data = nullptr;
        packet.size = 0;
        av_init_packet(&packet);
        auto ret = avcodec_receive_packet(m_codecContext.get(), &packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            av_packet_unref(&packet);
            break;
        } else if (ret < 0) {
            av_packet_unref(&packet);
            if (m_errorCallback != nullptr) {
                m_errorCallback("Failed to receive encoded frame: "s += getFfmpegErrorString(ret));
            }
            return false;
        }

        // Setup packet for muxing
        packet.stream_index = 0;
        packet.duration = av_rescale_q(1, av_inv_q(m_codecContext->framerate), m_codecContext->time_base);
        av_packet_rescale_ts(&packet, m_codecContext->time_base, m_formatContext->streams[0]->time_base);
        packet.pos = -1;

        // Mux encoded frame
        ret = av_interleaved_write_frame(m_formatContext.get(), &packet);
        if (ret < 0) {
            av_packet_unref(&packet);
            if (m_errorCallback != nullptr) {
                m_errorCallback("Failed to write encoded frame: "s += getFfmpegErrorString(ret));
            }
            return false;
        }

        av_packet_unref(&packet);
    }
    return true;
}
} // namespace Ak
