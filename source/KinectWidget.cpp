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

#define _USE_MATH_DEFINES
#include "KinectWidget.h"

#include "AzureKinectWindow.h"

#include <QOpenGLContext>
#include <QResource>
#include <glm/gtc/matrix_transform.hpp>
using namespace glm;
using namespace std;

namespace Ak {
#if _DEBUG
static QString s_severity[] = {"High", "Medium", "Low", "Notification"};
static QString s_type[] = {"Error", "Deprecated", "Undefined", "Portability", "Performance", "Other"};
static QString s_source[] = {"OpenGL", "OS", "GLSL Compiler", "3rd Party", "Application", "Other"};
void APIENTRY debugCallback(const uint32_t source, const uint32_t type, uint32_t, const uint32_t severity, int32_t,
    const char* message, void* userParam)
{
    // Get the severity
    uint32_t sevID;
    switch (severity) {
        case GL_DEBUG_SEVERITY_HIGH:
            sevID = 0;
            break;
        case GL_DEBUG_SEVERITY_MEDIUM:
            sevID = 1;
            break;
        case GL_DEBUG_SEVERITY_LOW:
            sevID = 2;
            break;
        case GL_DEBUG_SEVERITY_NOTIFICATION:
        default:
            sevID = 3;
            break;
    }

    // Get the type
    uint32_t typeID;
    switch (type) {
        case GL_DEBUG_TYPE_ERROR:
            typeID = 0;
            break;
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
            typeID = 1;
            break;
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
            typeID = 2;
            break;
        case GL_DEBUG_TYPE_PORTABILITY:
            typeID = 3;
            break;
        case GL_DEBUG_TYPE_PERFORMANCE:
            typeID = 4;
            break;
        case GL_DEBUG_TYPE_OTHER:
        default:
            typeID = 5;
            break;
    }

    // Get the source
    uint32_t sourceID;
    switch (source) {
        case GL_DEBUG_SOURCE_API:
            sourceID = 0;
            break;
        case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
            sourceID = 1;
            break;
        case GL_DEBUG_SOURCE_SHADER_COMPILER:
            sourceID = 2;
            break;
        case GL_DEBUG_SOURCE_THIRD_PARTY:
            sourceID = 3;
            break;
        case GL_DEBUG_SOURCE_APPLICATION:
            sourceID = 4;
            break;
        case GL_DEBUG_SOURCE_OTHER:
        default:
            sourceID = 5;
            break;
    }

    // Output to message callback
    if (severity == GL_DEBUG_SEVERITY_HIGH) {
        static_cast<KinectWidget*>(userParam)->errorSignal(QObject::tr("OpenGL Debug: Severity=") + s_severity[sevID] +
            QObject::tr(", Type=") + s_type[typeID] + QObject::tr(", Source=") + s_source[sourceID] +
            QObject::tr(" - ") + message);
    }
}
#endif

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

KinectWidget::KinectWidget(QWidget* parent) noexcept
    : QOpenGLWidget(parent)
{}

KinectWidget::~KinectWidget() noexcept
{
    makeCurrent();
    cleanup();
    doneCurrent();
}

void KinectWidget::setRenderOptions(const bool depthImage, const bool colourImage, const bool irImage,
    const bool bodyShadow, const bool bodySkeleton) noexcept
{
    m_depthImage = depthImage;
    m_colourImage = colourImage;
    m_irImage = irImage;
    m_bodyShadowImage = bodyShadow;
    m_bodySkeletonImage = bodySkeleton;
    m_refreshRender = true;
    resizeGL(width(), height());
}

void KinectWidget::dataSlot(const KinectImage depthImage, const KinectImage colourImage, const KinectImage irImage,
    const KinectImage shadowImage, const KinectJoints joints) noexcept
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
        // Copy shadow image data
        glBindTexture(GL_TEXTURE_2D, m_shadowTexture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, shadowImage.m_width, shadowImage.m_height, GL_RED, GL_UNSIGNED_BYTE,
            reinterpret_cast<const GLvoid*>(shadowImage.m_image));
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    if (m_bodySkeletonImage) {
        // Copy skeleton data
        if (joints.m_length > 0) {
            m_sphereTransforms.resize(0);
            mat4 scale = glm::scale(mat4(1.0f), vec3(0.024f));
            mat4 spaceConvert(1.0f);
            if (m_colourImage) {
                // Need to convert from depth space to colour space
                // TODO: These values are currently hardcoded from k4a calibration, they should really be passed from
                // k4a in case any settings are changed Note: These values obtained from
                // calibration->extrinsics[source_camera(0)][target_camera(1)]
                spaceConvert = mat4(vec4(0.999987721f, 0.00121299317f, 0.00480594439f, 0.0f),
                    vec4(-0.000721473712f, 0.994887710f, -0.100984767f, 0.0f),
                    vec4(-0.00490386877f, 0.100980058f, 0.994876385f, 0.0f),
                    vec4(-32.1208534f * 0.001f, -2.06779432f * 0.001f, 3.97831678f * 0.001f, 1.0f));
            }
            auto pointer = joints.m_joints;
            for (uint32_t i = 0; i < joints.m_length; ++i, ++pointer) {
                m_sphereTransforms.emplace_back(
                    spaceConvert * translate(mat4(1.0f), pointer->m_position.m_position * 0.001f) * scale);
            }
            glBindBuffer(GL_ARRAY_BUFFER, m_sphereInstanceBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(mat4) * joints.m_length,
                reinterpret_cast<const GLvoid*>(m_sphereTransforms.data()), GL_STATIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }
        m_sphereInstances = joints.m_length;
    }

    if (m_refreshRender) {
        // This means that the display image has changed so we must update values

        // Update the view/projection matrix
        const mat4 view = lookAt(vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 0.0f, 1.0f), vec3(0.0, -1.0, 0.0));
        mat4 projection = m_colourImage ? perspective(radians(59.0f), 90.0f / 59.0f, 0.01f, 30.0f) :
                                          perspective(radians(65.0f), 75.0f / 65.0f, 0.01f, 30.0f);
        projection[0][0] = -projection[0][0]; // Fix for mirroring
        mat4 viewProjection = projection * view;
        glBindBuffer(GL_UNIFORM_BUFFER, m_cameraUBO);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(mat4), &viewProjection, GL_DYNAMIC_DRAW);

        // Update transform buffer
        struct Transform
        {
            vec2 m_c;
            vec2 m_f;
            vec2 m_k14;
            vec2 m_k25;
            vec2 m_k36;
            vec2 m_p;
        };
        Transform trans;
        // TODO: These values are currently hardcoded from k4a calibration, they should really be passed from k4a in
        // case any settings are changed
        if (m_colourImage) {
            trans = {{637.340027f, 365.660858f}, {610.812317f, 610.779541f}, {0.685845017f, 0.561531842f},
                {-2.91037941f, -2.73197317f}, {1.64123452, 1.56802034}, {0.000656008604, -1.53675392e-5f}};
        } else {
            trans = {{337.062073f, 341.250916f}, {504.216888f, 504.104401f}, {0.663816869f, 1.00248814f},
                {0.385305285f, 0.549431324f}, {0.0257309489f, 0.121330343f}, {-0.000142980512, 9.78828975e-5f}};
        }
        glBindBuffer(GL_UNIFORM_BUFFER, m_transformUBO);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(Transform), &trans, GL_STATIC_DRAW);

        vec2 inverseRes;
        if (m_depthImage) {
            inverseRes = 1.0f / vec2(depthImage.m_width, depthImage.m_height);
        } else if (m_colourImage) {
            inverseRes = 1.0f / vec2(colourImage.m_width, colourImage.m_height);
        } else if (m_irImage) {
            inverseRes = 1.0f / vec2(irImage.m_width, irImage.m_height);
        }
        glBindBuffer(GL_ARRAY_BUFFER, m_imageUBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vec2), &inverseRes, GL_STATIC_DRAW);

        glBindBuffer(GL_UNIFORM_BUFFER, 0);
        m_refreshRender = false;
    }

    // Signal that widget needs to be rendered with new data
    update();
}

void KinectWidget::initializeGL() noexcept
{
    initializeOpenGLFunctions();

#if _DEBUG
    // Allow for synchronous callbacks.
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);

    // Set up the debug info callback
    glDebugMessageCallback(reinterpret_cast<GLDEBUGPROC>(&debugCallback), this);

    // Set up the type of debug information we want to receive
    uint32_t uiUnusedIDs = 0;
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, &uiUnusedIDs, GL_TRUE); // Enable all
    glDebugMessageControl(
        GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE); // Disable notifications
#endif

    // Connect cleanup handler
    connect(context(), &QOpenGLContext::aboutToBeDestroyed, this, &KinectWidget::cleanup, Qt::DirectConnection);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // Set the cleared back buffer to black
    glCullFace(GL_BACK);                  // Set back-face culling
    glEnable(GL_CULL_FACE);               // Enable use of back/front face culling
    glEnable(GL_DEPTH_TEST);              // Enable use of depth testing
    glDisable(GL_STENCIL_TEST);           // Disable stencil test for speed

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

    // Create shadow texture
    glGenTextures(1, &m_shadowTexture);
    glBindTexture(GL_TEXTURE_2D, m_shadowTexture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_R8, 640, 576);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 640, 576, GL_RED, GL_UNSIGNED_BYTE,
        reinterpret_cast<const GLvoid*>(initialBlank.data()));

    // Create sphere object
    glGenVertexArrays(1, &m_sphereVAO);

    // Create VBO and IBOs
    glGenBuffers(1, &m_sphereVBO);
    glGenBuffers(1, &m_sphereIBO);

    // Bind the Sphere VAO
    glBindVertexArray(m_sphereVAO);

    // Create Sphere VBO and IBO data
    m_sphereElements = generateSphere(12, 6, m_sphereVBO, m_sphereIBO);

    // Create sphere instance buffer
    glGenBuffers(1, &m_sphereInstanceBO);
    glBindBuffer(GL_ARRAY_BUFFER, m_sphereInstanceBO);
    glBufferData(
        GL_ARRAY_BUFFER, sizeof(mat4) * 32, reinterpret_cast<const GLvoid*>(initialBlank.data()), GL_STATIC_DRAW);
    for (uint32_t i = 0; i < 4; i++) {
        // Set up the vertex attribute
        glVertexAttribPointer(2 + i, 4, GL_FLOAT, GL_FALSE, sizeof(mat4), reinterpret_cast<void*>(sizeof(vec4) * i));
        glEnableVertexAttribArray(2 + i);
        glVertexAttribDivisor(2 + i, 1);
    }
    glBindVertexArray(0);
    m_sphereTransforms.reserve(32);

    // Setup inverse resolution
    glGenBuffers(1, &m_inverseResUBO);
    vec2 inverseRes = 1.0f / vec2(1280.0f, 720.0f);
    glBindBuffer(GL_UNIFORM_BUFFER, m_inverseResUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(vec2), &inverseRes, GL_STATIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 2, m_inverseResUBO);

    // Create the viewProjection buffers
    glGenBuffers(1, &m_cameraUBO);
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, m_cameraUBO);

    // Create image space transform buffer
    glGenBuffers(1, &m_transformUBO);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, m_transformUBO);

    // Create image resolution buffer
    glGenBuffers(1, &m_imageUBO);
    glBindBufferBase(GL_UNIFORM_BUFFER, 3, m_imageUBO);

    // Bind defaults to prevent potential contamination
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

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
    glDeleteShader(fragmentShader);

    if (!loadShader(fragmentShader, GL_FRAGMENT_SHADER, (GLchar*)QResource(":/AzureKinect/ShadowImage.frag").data())) {
        return;
    }
    if (!loadShaders(m_shadowProgram, vertexShader, fragmentShader)) {
        return;
    }

    // Clean up unneeded shaders
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    if (!loadShader(vertexShader, GL_VERTEX_SHADER, (GLchar*)QResource(":/AzureKinect/Skeleton.vert").data())) {
        return;
    }
    if (!loadShader(fragmentShader, GL_FRAGMENT_SHADER, (GLchar*)QResource(":/AzureKinect/Skeleton.frag").data())) {
        return;
    }
    if (!loadShaders(m_skeletonProgram, vertexShader, fragmentShader)) {
        return;
    }

    // Clean up unneeded shaders
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}

void KinectWidget::resizeGL(const int width, const int height) noexcept
{
    // Keep the viewport at a fixed 16:9 ratio for colour and (10/9 for depth/ir)
    const uint32_t widthRatio = m_colourImage ? 16 : 10;
    constexpr uint32_t heightRatio = 9;
    int32_t newWidth = width, newHeight = height;
    do {
        const int32_t widthScale = ((newHeight * widthRatio) / heightRatio);
        const int32_t heightScale = ((newWidth * heightRatio) / widthRatio);
        const int32_t widthOffset = heightScale >= newHeight ? widthScale : newWidth;
        const int32_t heightOffset = widthScale >= newWidth ? heightScale : newHeight;
        newWidth = widthOffset;
        newHeight = heightOffset;
    } while (newWidth * heightRatio - newHeight * widthRatio != 0);
    m_viewportX = (width - newWidth) / 2;
    m_viewportY = (height - newHeight) / 2;
    m_viewportW = newWidth;
    m_viewportH = newHeight;

    // K4A depth camera FOV is as follows:
    //  K4A_DEPTH_MODE_NFOV_2X2BINNED= 75x65;
    //  K4A_DEPTH_MODE_NFOV_UNBINNED= 75x65;
    //  K4A_DEPTH_MODE_WFOV_2X2BINNED= 120x120;
    //  K4A_DEPTH_MODE_WFOV_UNBINNED= 120x120;
    //  K4A_DEPTH_MODE_PASSIVE_IR= NA
    // K4A colour camera FOV is as follows:
    //  K4A_COLOR_RESOLUTION_720P= 90x59
    //  K4A_COLOR_RESOLUTION_1080P= 90x59
    //  K4A_COLOR_RESOLUTION_1440P= 90x59
    //  K4A_COLOR_RESOLUTION_1536P= 90x74.3
    //  K4A_COLOR_RESOLUTION_2160P= 90x59
    //  K4A_COLOR_RESOLUTION_3072P= 90x74.3

    // Update inverse resolution
    struct ResolutionBuffer
    {
        vec2 m_inverseRes;
        vec2 m_windowsOffset;
    };
    ResolutionBuffer resBuffer = {
        1.0f / vec2(static_cast<float>(m_viewportW), static_cast<float>(m_viewportH)), vec2(m_viewportX, m_viewportY)};
    glBindBuffer(GL_UNIFORM_BUFFER, m_inverseResUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(ResolutionBuffer), &resBuffer, GL_STATIC_DRAW);

    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void KinectWidget::paintGL() noexcept
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Set correct viewport as it gets changed by Qt
    glViewport(m_viewportX, m_viewportY, m_viewportW, m_viewportH);

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    if (m_depthImage) {
        // Render depth image
        glUseProgram(m_depthProgram);
        glBindVertexArray(m_quadVAO);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, m_depthTexture);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, nullptr);
    } else if (m_colourImage) {
        // Render colour image
        glUseProgram(m_colourProgram);
        glBindVertexArray(m_quadVAO);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, m_colourTexture);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, nullptr);
    } else if (m_irImage) {
        // Render IR image
        glUseProgram(m_irProgram);
        glBindVertexArray(m_quadVAO);
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, m_irTexture);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, nullptr);
    }

    if (m_bodyShadowImage && !m_colourImage) {
        // Render body shadow
        glEnable(GL_BLEND); // Enable blending
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glUseProgram(m_shadowProgram);
        glBindVertexArray(m_quadVAO);
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, m_shadowTexture);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, nullptr);
        glDisable(GL_BLEND);
    }

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    if (m_bodySkeletonImage) {
        // Render skeleton joints
        glEnable(GL_BLEND); // Enable blending
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        if (m_sphereInstances > 0) {
            glUseProgram(m_skeletonProgram);
            glBindVertexArray(m_sphereVAO);
            glDrawElementsInstanced(GL_TRIANGLES, m_sphereElements, GL_UNSIGNED_INT, nullptr, m_sphereInstances);
        }
        // Render body skeleton
        //
        //                // Visualize bones
        //        vector<Bone> bodyBones(s_boneList.size());
        //        for (auto& bone : s_boneList) {
        //            const k4abt_joint_id_t joint1 = bone.first;
        //            const k4abt_joint_id_t joint2 = bone.second;
        //
        //            if (body.skeleton.joints[joint1].confidence_level >= K4ABT_JOINT_CONFIDENCE_LOW &&
        //                body.skeleton.joints[joint2].confidence_level >= K4ABT_JOINT_CONFIDENCE_LOW) {
        //                const k4a_float3_t& joint1Position = body.skeleton.joints[joint1].position;
        //                const k4a_float3_t& joint2Position = body.skeleton.joints[joint2].position;
        //
        //                bodyBones.emplace_back(Position{joint1Position.xyz.x, joint1Position.xyz.y,
        //                joint1Position.xyz.z},
        //                    Position{joint2Position.xyz.x, joint2Position.xyz.y, joint2Position.xyz.z},
        //                    body.skeleton.joints[joint1].confidence_level >= K4ABT_JOINT_CONFIDENCE_MEDIUM &&
        //                        body.skeleton.joints[joint2].confidence_level >= K4ABT_JOINT_CONFIDENCE_MEDIUM);
        //            }
        //        }
        // TODO:****
        glDisable(GL_BLEND);
    }
    glBindVertexArray(0);
}

void KinectWidget::cleanup() noexcept
{
    // Free all resources
    glDeleteProgram(m_depthProgram);
    glDeleteProgram(m_colourProgram);
    glDeleteProgram(m_irProgram);
    glDeleteProgram(m_shadowProgram);
    glDeleteProgram(m_skeletonProgram);

    glDeleteBuffers(1, &m_quadVBO);
    glDeleteBuffers(1, &m_quadIBO);
    glDeleteVertexArrays(1, &m_quadVAO);

    glDeleteTextures(1, &m_depthTexture);
    glDeleteTextures(1, &m_colourTexture);
    glDeleteTextures(1, &m_irTexture);
    glDeleteTextures(1, &m_shadowTexture);

    glDeleteBuffers(1, &m_sphereVBO);
    glDeleteBuffers(1, &m_sphereIBO);
    glDeleteVertexArrays(1, &m_sphereVAO);

    glDeleteBuffers(1, &m_inverseResUBO);
    glDeleteBuffers(1, &m_cameraUBO);
    glDeleteBuffers(1, &m_transformUBO);
    glDeleteBuffers(1, &m_imageUBO);
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
        emit errorSignal(tr("Failed to compile shader: ") + infolog + shaderCode);
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

GLsizei KinectWidget::generateSphere(
    const uint32_t tessU, const uint32_t tessV, const GLuint vertexBO, const GLuint indexBO)
{
    // Init params
    const float dPhi = static_cast<float>(M_PI) / static_cast<float>(tessV);
    const float dTheta = static_cast<float>(M_PI + M_PI) / static_cast<float>(tessU);

    // Determine required parameters
    const uint32_t numVertices = (tessU * (tessV - 1)) + 2;
    const uint32_t numIndices = (tessU * 6) + (tessU * (tessV - 2) * 6);

    // Create the new primitive
    vector<CustomVertex> vertexBuffer;
    vertexBuffer.reserve(numVertices);
    vector<GLuint> indexBuffer;
    indexBuffer.reserve(numIndices);

    // Set the top vertex
    vertexBuffer.emplace_back(vec3(0.0f, 1.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));

    float fPhi = dPhi;
    for (uint32_t i = 0; i < tessV - 1; i++) {
        // Calculate initial value
        const float rSinPhi = sinf(fPhi);
        const float rCosPhi = cosf(fPhi);

        const float y = rCosPhi;

        float fTheta = 0.0f;
        for (uint32_t j = 0; j < tessU; j++) {
            // Calculate positions
            const float cosTheta = cosf(fTheta);
            const float sinTheta = sinf(fTheta);

            // Determine position
            const float x = rSinPhi * cosTheta;
            const float z = rSinPhi * sinTheta;

            // Create vertex

            vertexBuffer.emplace_back(vec3(x, y, z), vec3(x, y, z));
            fTheta += dTheta;
        }
        fPhi += dPhi;
    }

    // Set the bottom vertex
    vertexBuffer.emplace_back(vec3(0.0f, -1.0f, 0.0f), vec3(0.0f, -1.0f, 0.0f));

    // Create top
    for (GLuint j = 1; j <= tessU; j++) {
        // Top triangles all share same vertex point at pos 0
        indexBuffer.emplace_back(0);
        // Loop back to start if required
        indexBuffer.emplace_back(((j + 1) > tessU) ? 1 : j + 1);
        indexBuffer.emplace_back(j);
    }

    // Create inner triangles
    for (GLuint i = 0; i < tessV - 2; i++) {
        for (GLuint j = 1; j <= tessU; j++) {
            // Create indexes for each quad face (pair of triangles)
            indexBuffer.emplace_back(j + (i * tessU));
            // Loop back to start if required
            const GLuint index = ((j + 1) > tessU) ? 1 : j + 1;
            indexBuffer.emplace_back(index + (i * tessU));
            indexBuffer.emplace_back(j + ((i + 1) * tessU));

            indexBuffer.emplace_back(*(indexBuffer.end() - 2));
            // Loop back to start if required
            indexBuffer.emplace_back(index + ((i + 1) * tessU));
            indexBuffer.emplace_back(*(indexBuffer.end() - 3));
        }
    }

    // Create bottom
    for (GLuint j = 1; j <= tessU; j++) {
        // Bottom triangles all share same vertex point at pos numVertices - 1
        indexBuffer.emplace_back(j + ((tessV - 2) * tessU));
        // Loop back to start if required
        const GLuint index = ((j + 1) > tessU) ? 1 : j + 1;
        indexBuffer.emplace_back(index + ((tessV - 2) * tessU));
        indexBuffer.emplace_back(numVertices - 1);
    }

    // Fill Vertex Buffer Object
    glBindBuffer(GL_ARRAY_BUFFER, vertexBO);
    glBufferData(GL_ARRAY_BUFFER, vertexBuffer.size() * sizeof(CustomVertex), vertexBuffer.data(), GL_STATIC_DRAW);

    // Fill Index Buffer Object
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexBuffer.size() * sizeof(GLuint), indexBuffer.data(), GL_STATIC_DRAW);

    // Specify location of data within buffer
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(CustomVertex), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(CustomVertex),
        reinterpret_cast<const GLvoid*>(offsetof(CustomVertex, m_normal)));
    glEnableVertexAttribArray(1);

    return numIndices;
}
} // namespace Ak
