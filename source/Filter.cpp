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

#include "Encoder.h"

#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
}

using namespace std;

namespace Ak {
extern std::string getFfmpegErrorString(int errorCode) noexcept;

Filter::FilterGraphPtr::FilterGraphPtr(AVFilterGraph* filterGraph) noexcept
    : m_filterGraph(filterGraph, [](AVFilterGraph* p) { avfilter_graph_free(&p); })
{}

AVFilterGraph* Filter::FilterGraphPtr::get() const noexcept
{
    return m_filterGraph.get();
}

AVFilterGraph* Filter::FilterGraphPtr::operator->() const noexcept
{
    return m_filterGraph.get();
}

bool Filter::init(const uint32_t width, const uint32_t height, const AVRational fps, const int32_t format,
    const float scale, const uint32_t numThreads, errorCallback error) noexcept
{
    m_errorCallback = move(error);

    // Make a filter graph to perform any required conversions
    FilterGraphPtr tempGraph(avfilter_graph_alloc());
    const auto bufferIn = avfilter_get_by_name("buffer");
    const auto bufferOut = avfilter_get_by_name("buffersink");

    if (tempGraph.get() == nullptr || bufferIn == nullptr || bufferOut == nullptr) {
        if (m_errorCallback != nullptr) {
            m_errorCallback("Unable to create filter graph"s);
        }
        return false;
    }
    tempGraph->nb_threads = numThreads;

    // Create the input and output buffers
    const auto bufferInContext = avfilter_graph_alloc_filter(tempGraph.get(), bufferIn, "src");
    const auto bufferOutContext = avfilter_graph_alloc_filter(tempGraph.get(), bufferOut, "sink");
    if (bufferInContext == nullptr || bufferOutContext == nullptr) {
        if (m_errorCallback != nullptr) {
            m_errorCallback("Could not allocate the filter buffer instance"s);
        }
        return false;
    }

    // Set the input buffer parameters
    auto inParams = av_buffersrc_parameters_alloc();
    inParams->format = format;
    inParams->frame_rate = fps;
    inParams->height = height;
    inParams->width = width;
    inParams->sample_aspect_ratio = {1, 1};
    inParams->time_base = av_inv_q(inParams->frame_rate);
    auto ret = av_buffersrc_parameters_set(bufferInContext, inParams);
    if (ret < 0) {
        av_free(inParams);
        if (m_errorCallback != nullptr) {
            m_errorCallback("Failed setting filter input parameters: "s += getFfmpegErrorString(ret));
        }
        return false;
    }
    av_free(inParams);
    ret = avfilter_init_str(bufferInContext, nullptr);
    if (ret < 0) {
        if (m_errorCallback != nullptr) {
            m_errorCallback("Could not initialize the filter input instance: "s += getFfmpegErrorString(ret));
        }
        return false;
    }

    // Set the output buffer parameters
    enum AVPixelFormat pixelFormats[] = {AV_PIX_FMT_YUV420P};
    ret = av_opt_set_bin(bufferOutContext, "pix_fmts", reinterpret_cast<const uint8_t*>(pixelFormats),
        sizeof(pixelFormats), AV_OPT_SEARCH_CHILDREN);
    ret = (ret < 0) ? ret : avfilter_init_str(bufferOutContext, nullptr);
    if (ret < 0) {
        if (m_errorCallback != nullptr) {
            m_errorCallback("Could not initialize the filter output instance: "s += getFfmpegErrorString(ret));
        }
        return false;
    }
    AVFilterContext* nextFilter = bufferInContext;
    auto hflip = [&]() {
        const auto mirrorFilter = avfilter_get_by_name("hflip");
        if (mirrorFilter == nullptr) {
            if (m_errorCallback != nullptr) {
                m_errorCallback("Unable to create hflip filter"s);
            }
            return false;
        }
        const auto mirrorContext = avfilter_graph_alloc_filter(tempGraph.get(), mirrorFilter, "hflip");
        if (mirrorContext == nullptr) {
            if (m_errorCallback != nullptr) {
                m_errorCallback("Unable to create hflip filter context"s);
            }
            return false;
        }

        // Link the filter into chain
        ret = avfilter_link(nextFilter, 0, mirrorContext, 0);
        if (ret < 0) {
            if (m_errorCallback != nullptr) {
                m_errorCallback("Unable to link hflip filter"s);
            }
            return false;
        }
        nextFilter = mirrorContext;
        return true;
    };

    if (format == AV_PIX_FMT_GRAY16LE) {
        // Do hflip first as gray16 requires fewer operations than the format colorlevels uses
        if (!hflip()) {
            return false;
        }

        const auto brightFilter = avfilter_get_by_name("colorlevels");
        if (brightFilter == nullptr) {
            if (m_errorCallback != nullptr) {
                m_errorCallback("Unable to create colorlevels filter"s);
            }
            return false;
        }
        const auto brightContext = avfilter_graph_alloc_filter(tempGraph.get(), brightFilter, "colorlevels");
        if (brightContext == nullptr) {
            if (m_errorCallback != nullptr) {
                m_errorCallback("Unable to create colorlevels filter context"s);
            }
            return false;
        }
        const auto scaleString = to_string(1.0f / scale);
        av_opt_set(brightContext, "rimax", scaleString.c_str(), AV_OPT_SEARCH_CHILDREN);
        av_opt_set(brightContext, "gimax", scaleString.c_str(), AV_OPT_SEARCH_CHILDREN);
        av_opt_set(brightContext, "bimax", scaleString.c_str(), AV_OPT_SEARCH_CHILDREN);

        // Link the filter into chain
        ret = avfilter_link(nextFilter, 0, brightContext, 0);
        if (ret < 0) {
            if (m_errorCallback != nullptr) {
                m_errorCallback("Unable to link colorlevels filter"s);
            }
            return false;
        }
        nextFilter = brightContext;
    } else {
        const auto scaleFilter = avfilter_get_by_name("scale");
        if (scaleFilter == nullptr) {
            if (m_errorCallback != nullptr) {
                m_errorCallback("Unable to create scale filter"s);
            }
            return false;
        }
        const auto scaleContext = avfilter_graph_alloc_filter(tempGraph.get(), scaleFilter, "scale");
        if (scaleContext == nullptr) {
            if (m_errorCallback != nullptr) {
                m_errorCallback("Unable to create scale filter context"s);
            }
            return false;
        }

        const float aspect = static_cast<float>(height) / static_cast<float>(width);
        const auto heightScale = static_cast<uint32_t>(640.0f * aspect);
        av_opt_set(scaleContext, "w", "640", AV_OPT_SEARCH_CHILDREN);
        av_opt_set(scaleContext, "h", to_string(heightScale).c_str(), AV_OPT_SEARCH_CHILDREN);

        av_opt_set(scaleContext, "flags", "point", AV_OPT_SEARCH_CHILDREN);

        // Link the filter into chain
        ret = avfilter_link(nextFilter, 0, scaleContext, 0);
        if (ret < 0) {
            if (m_errorCallback != nullptr) {
                m_errorCallback("Unable to link scale filter"s);
            }
            return false;
        }
        nextFilter = scaleContext;

        // Do hflip after resizing to reduce number of pixels worked on
        if (!hflip()) {
            return false;
        }
    }

    // Link final filter sequence
    ret = avfilter_link(nextFilter, 0, bufferOutContext, 0);
    if (ret < 0) {
        if (m_errorCallback != nullptr) {
            m_errorCallback("Could not set the filter links: "s += getFfmpegErrorString(ret));
        }
        return false;
    }

    // Configure the completed graph
    ret = avfilter_graph_config(tempGraph.get(), nullptr);
    if (ret < 0) {
        if (m_errorCallback != nullptr) {
            m_errorCallback("Failed configuring filter graph: "s += getFfmpegErrorString(ret));
        }
        return false;
    }

    // Make a new filter
    m_filterGraph = move(tempGraph);
    m_source = bufferInContext;
    m_sink = bufferOutContext;

    return true;
}

bool Filter::sendFrame(FramePtr& frame) const noexcept
{
    const auto err = av_buffersrc_add_frame(m_source, frame.get());
    if (err < 0) {
        if (m_errorCallback != nullptr) {
            m_errorCallback("Failed to submit frame to filter graph: "s += getFfmpegErrorString(err));
        }
        return false;
    }
    return true;
}

bool Filter::receiveFrame(FramePtr& frame) const noexcept
{
    // Get the next available frame
    const auto err = av_buffersink_get_frame(m_sink, frame.get());
    if (err < 0) {
        if ((err == AVERROR(EAGAIN)) || (err == AVERROR_EOF)) {
            return false;
        }
        if (m_errorCallback != nullptr) {
            m_errorCallback("Failed to receive frame from filter graph: "s += getFfmpegErrorString(err));
        }
        return false;
    }
    return true;
}

uint32_t Filter::getWidth() const noexcept
{
    return av_buffersink_get_w(m_sink);
}

uint32_t Filter::getHeight() const noexcept
{
    return av_buffersink_get_h(m_sink);
}

AVPixelFormat Filter::getPixelFormat() const noexcept
{
    return static_cast<AVPixelFormat>(av_buffersink_get_format(m_sink));
}

AVRational Filter::getFrameRate() const noexcept
{
    return av_buffersink_get_frame_rate(m_sink);
}
} // namespace Ak
