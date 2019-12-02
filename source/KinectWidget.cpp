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

#include "KinectWidget.h"

#include "AzureKinectWindow.h"

#include <QOpenGLContext>
#include <QResource>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/type_aligned.hpp>
using namespace glm;
using namespace std;

namespace Ak {
KinectWidget::KinectWidget(QWidget* parent) noexcept
    : QOpenGLWidget(parent)
{}

KinectWidget::~KinectWidget() noexcept
{
    cleanup();
}

void KinectWidget::setRenderOptions(const bool depthImage, const bool colourImage, const bool irImage,
    const bool bodyShadow, const bool bodySkeleton) noexcept
{
    m_depthImage = depthImage;
    m_colourImage = colourImage;
    m_irImage = irImage;
    m_bodyShadowImage = bodyShadow;
    m_bodySkeletonImage = bodySkeleton;
}

void KinectWidget::imageSlot(const AzureKinect::KinectImage depthImage, const AzureKinect::KinectImage colourImage,
    AzureKinect::KinectImage irImage) noexcept
{
    // Only inbuilt types can be used as parameters for signal/slot connections. Hence why unsigned must be used instead
    // of uint32_t as otherwise connections are not triggered properly

    if (m_depthImage) {
        // Copy depth image data
        if (depthImage.m_stride / 2 != depthImage.m_width) {
            glPixelStorei(GL_UNPACK_ROW_LENGTH, depthImage.m_stride / 2);
        }
        glBindTexture(GL_TEXTURE_2D, m_depthTexture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, depthImage.m_width, depthImage.m_height, GL_DEPTH_COMPONENT,
            GL_UNSIGNED_SHORT, reinterpret_cast<const GLvoid*>(depthImage.m_image));
        if (depthImage.m_stride / 2 != depthImage.m_width) {
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        }
        glBindTexture(GL_TEXTURE_2D, 0);
    } else if (m_colourImage) {
        // Copy colour image data
        if (colourImage.m_stride / 4 != colourImage.m_width) {
            glPixelStorei(GL_UNPACK_ROW_LENGTH, colourImage.m_stride / 4);
        }
        glBindTexture(GL_TEXTURE_2D, m_colourTexture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, colourImage.m_width, colourImage.m_height, GL_BGRA, GL_UNSIGNED_BYTE,
            reinterpret_cast<const GLvoid*>(colourImage.m_image));
        if (colourImage.m_stride / 4 != colourImage.m_width) {
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        }
        glBindTexture(GL_TEXTURE_2D, 0);
    } else if (m_irImage) {
        // Copy colour image data
        if (irImage.m_stride / 2 != irImage.m_width) {
            glPixelStorei(GL_UNPACK_ROW_LENGTH, irImage.m_stride / 2);
        }
        glBindTexture(GL_TEXTURE_2D, m_irTexture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, irImage.m_width, irImage.m_height, GL_DEPTH_COMPONENT,
            GL_UNSIGNED_SHORT, reinterpret_cast<const GLvoid*>(irImage.m_image));
        if (irImage.m_stride / 2 != irImage.m_width) {
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        }
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    if (m_bodyShadowImage) {
        // TODO:****
    }

    if (m_bodySkeletonImage) {
        // TODO:****
    }

    // Signal that widget needs to be rendered with new data
    update();
}

void KinectWidget::initializeGL() noexcept
{
    initializeOpenGLFunctions();
    // Connect cleanup handler
    connect(context(), &QOpenGLContext::aboutToBeDestroyed, this, &KinectWidget::cleanup);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // Set the cleared back buffer to black
    glCullFace(GL_BACK);                  // Set back-face culling
    glEnable(GL_CULL_FACE);               // Enable use of back/front face culling
    glEnable(GL_DEPTH_TEST);              // Enable use of depth testing
    glDisable(GL_STENCIL_TEST);           // Disable stencil test for speed

    // Load shaders
    GLuint vertexShader;
    if (!loadShader(vertexShader, GL_VERTEX_SHADER, (GLchar*)QResource(":/AzureKinect/FullScreenQuad.vert").data())) {
        return;
    }
    GLuint fragmentShader;
    if (!loadShader(fragmentShader, GL_FRAGMENT_SHADER, (GLchar*)QResource(":/AzureKinect/DepthImage.frag").data())) {
        return;
    }
    if (!loadShaders(m_depthProgram, vertexShader, fragmentShader)) {
        return;
    }
    glDeleteShader(fragmentShader);

    if (!loadShader(fragmentShader, GL_FRAGMENT_SHADER, (GLchar*)QResource(":/AzureKinect/ColourImage.frag").data())) {
        return;
    }
    if (!loadShaders(m_colourProgram, vertexShader, fragmentShader)) {
        return;
    }
    glDeleteShader(fragmentShader);

    if (!loadShader(fragmentShader, GL_FRAGMENT_SHADER, (GLchar*)QResource(":/AzureKinect/IRImage.frag").data())) {
        return;
    }
    if (!loadShaders(m_irProgram, vertexShader, fragmentShader)) {
        return;
    }

    // Clean up unneeded shaders
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // Generate the full screen quad
    glGenVertexArrays(1, &m_quadVAO);
    glGenBuffers(1, &m_quadVBO);
    glGenBuffers(1, &m_quadIBO);
    glBindVertexArray(m_quadVAO);

    // Create VBO data
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
    GLfloat vertexData[] = {-1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f};
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertexData), vertexData, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), nullptr);
    glEnableVertexAttribArray(0);

    // Create IBO data
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_quadIBO);
    GLubyte indexData[] = {0, 1, 3, 1, 2, 3};
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_quadIBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indexData), indexData, GL_STATIC_DRAW);
    glBindVertexArray(0);

    // K4A image dimensions are as follows:
    //  K4A_DEPTH_MODE_NFOV_2X2BINNED= {320, 288};
    //  K4A_DEPTH_MODE_NFOV_UNBINNED= {640, 576};
    //  K4A_DEPTH_MODE_WFOV_2X2BINNED= {512, 512};
    //  K4A_DEPTH_MODE_WFOV_UNBINNED= {1024, 1024};
    //  K4A_DEPTH_MODE_PASSIVE_IR= {1024, 1024}; otherwise IR equals same as depth

    // Create depth texture
    glGenTextures(1, &m_depthTexture);
    glBindTexture(GL_TEXTURE_2D, m_depthTexture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT16, 640, 576);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    std::vector<uint8_t> initialBlank(1280 * 720 * 10, 0);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 640, 576, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT,
        reinterpret_cast<const GLvoid*>(initialBlank.data()));
    glBindTexture(GL_TEXTURE_2D, 0);

    // Create colour texture
    glGenTextures(1, &m_colourTexture);
    glBindTexture(GL_TEXTURE_2D, m_colourTexture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 1280, 720);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1280, 720, GL_BGRA, GL_UNSIGNED_BYTE,
        reinterpret_cast<const GLvoid*>(initialBlank.data()));
    glBindTexture(GL_TEXTURE_2D, 0);

    // Create IR texture
    glGenTextures(1, &m_irTexture);
    glBindTexture(GL_TEXTURE_2D, m_irTexture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT16, 640, 576);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 640, 576, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT,
        reinterpret_cast<const GLvoid*>(initialBlank.data()));
    glBindTexture(GL_TEXTURE_2D, 0);

    // Setup inverse resolution
    glGenBuffers(1, &m_inverseResUBO);
    vec2 inverseRes = 1.0f / vec2(1280.0f, 720.0f);
    glBindBuffer(GL_UNIFORM_BUFFER, m_inverseResUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(vec2), &inverseRes, GL_STATIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 2, m_inverseResUBO);

    // Create the viewProjection buffers
    glGenBuffers(1, &m_cameraUBO);
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, m_cameraUBO);
}

void KinectWidget::resizeGL(const int width, const int height) noexcept
{
    // Keep the viewport at a fixed 16:9 ratio
    int32_t newWidth = width, newHeight = height;
    do {
        const int32_t widthScale = ((newHeight * 16) / 9);
        const int32_t heightScale = ((newWidth * 9) / 16);
        const int32_t widthOffset = heightScale >= newHeight ? widthScale : newWidth;
        const int32_t heightOffset = widthScale >= newWidth ? heightScale : newHeight;
        newWidth = widthOffset;
        newHeight = heightOffset;
    } while (newWidth * 9 - newHeight * 16 != 0);
    m_viewportX = (width - newWidth) / 2;
    m_viewportY = (height - newHeight) / 2;
    m_viewportW = newWidth;
    m_viewportH = newHeight;

    // Update the projection matrix
    //  If camera is set to wide field of view then FOV=120 otherwise it is 65
    mat4 viewProjection =
        perspective(65.0f, static_cast<float>(newWidth) / static_cast<float>(newHeight), 0.1f, 150.0f);
    glBindBuffer(GL_UNIFORM_BUFFER, m_cameraUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(mat4), &viewProjection, GL_DYNAMIC_DRAW);

    // Update inverse resolution
    vec2 inverseRes = 1.0f / vec2(static_cast<float>(newWidth), static_cast<float>(newHeight));
    glBindBuffer(GL_UNIFORM_BUFFER, m_inverseResUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(vec2), &inverseRes, GL_STATIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void KinectWidget::paintGL() noexcept
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Set correct viewport as it gets changed by Qt
    glViewport(m_viewportX, m_viewportY, m_viewportW, m_viewportH);

    if (m_depthImage) {
        // Render depth image
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glUseProgram(m_depthProgram);
        glBindVertexArray(m_quadVAO);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, m_depthTexture);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, nullptr);

        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
    } else if (m_colourImage) {
        // Render colour image
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glUseProgram(m_colourProgram);
        glBindVertexArray(m_quadVAO);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, m_colourTexture);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, nullptr);

        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
    } else if (m_irImage) {
        // Render IR image
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glUseProgram(m_irProgram);
        glBindVertexArray(m_quadVAO);
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, m_irTexture);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, nullptr);

        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
    }

    if (m_bodyShadowImage) {
        // Render body shadow
        // TODO:****
    }

    if (m_bodySkeletonImage) {
        // Render body skeleton
        // TODO:****
    }
}

void KinectWidget::cleanup() noexcept
{
    // Make sure the context is current and then destroy all underlying resources.
    makeCurrent();

    // Free all resources
    glDeleteProgram(m_depthProgram);
    glDeleteProgram(m_colourProgram);
    glDeleteProgram(m_irProgram);

    glDeleteBuffers(1, &m_quadVBO);
    glDeleteBuffers(1, &m_quadIBO);
    glDeleteVertexArrays(1, &m_quadVAO);

    glDeleteTextures(1, &m_depthTexture);
    glDeleteTextures(1, &m_colourTexture);
    glDeleteTextures(1, &m_irTexture);

    glDeleteBuffers(1, &m_inverseResUBO);
    glDeleteBuffers(1, &m_cameraUBO);

    doneCurrent();
}

bool KinectWidget::loadShader(GLuint& shader, const GLenum shaderType, const GLchar* shaderCode) noexcept
{
    // Build and link the shader program
    shader = glCreateShader(shaderType);
    glShaderSource(shader, 1, &shaderCode, nullptr);
    glCompileShader(shader);

    // Check for errors
    GLint testReturn;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &testReturn);
    if (testReturn == GL_FALSE) {
        GLchar infolog[1024];
        int32_t errorLength;
        glGetShaderInfoLog(shader, 1024, &errorLength, infolog);
        emit errorSignal(tr("Failed to compile shader: ") + infolog);
        glDeleteShader(shader);
        return false;
    }
    return true;
}

bool KinectWidget::loadShaders(GLuint& shader, const GLuint vertexShader, const GLuint fragmentShader) noexcept
{
    // Link the shaders
    shader = glCreateProgram();
    glAttachShader(shader, vertexShader);
    glAttachShader(shader, fragmentShader);
    glLinkProgram(shader);

    // Check for error in link
    GLint testReturn;
    glGetProgramiv(shader, GL_LINK_STATUS, &testReturn);
    if (testReturn == GL_FALSE) {
        GLchar infolog[1024];
        int32_t errorLength;
        glGetShaderInfoLog(shader, 1024, &errorLength, infolog);
        emit errorSignal(tr("Failed to link shaders: ") + infolog);
        glDeleteProgram(shader);
        return false;
    }
    return true;
}
} // namespace Ak
