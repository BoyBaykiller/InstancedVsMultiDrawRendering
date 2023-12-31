#include <iostream>
#include <fstream>
#include <format>
#include <vector>
#include <array>
#include <cmath>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

static void GLAPIENTRY MessageCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam)
{
    if (type == 33361)
    {
        // Buffer detailed info: Buffer object 2 (bound to GL_SHADER_STORAGE_BUFFER, and GL_SHADER_STORAGE_BUFFER (0), usage hint is GL_DYNAMIC_DRAW) will use VIDEO memory as the source for buffer object operations.
        return;
    }
    if (type == 33360)
    {
        // Buffer performance warning: Buffer object 2 (bound to GL_SHADER_STORAGE_BUFFER, and GL_SHADER_STORAGE_BUFFER (0), usage hint is GL_DYNAMIC_DRAW) is being copied/moved from VIDEO memory to HOST memory.
        return;
    }
    std::cout << message << '\n';
}

static uint32_t MakeShader(GLenum type, const char* srcCode)
{
    auto shader = glCreateShader(type);
    glShaderSource(shader, 1, &srcCode, 0);
    glCompileShader(shader);

    std::string infoLog(4096, '\0');
    glGetShaderInfoLog(shader, infoLog.size(), nullptr, infoLog.data());
    std::cout << infoLog;

    return shader;
}

static std::string LoadFile(std::string_view path)
{
    std::ifstream file{ path.data(), std::ios::in | std::ios::binary };
    std::string fileData{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
    return fileData;
}

static float RoundTo(float value, uint32_t decimalPlaces)
{
    auto multiplier = std::pow(10.0f, decimalPlaces);
    return std::round(value * multiplier) / multiplier;
}

static void ExitWithMessage(std::string_view message)
{
    std::cout << message;
    std::cout << "Press Enter to exit." << '\n';
    std::cin.get();
    std::exit(EXIT_FAILURE);
}

struct DrawArraysIndirectCommand
{
    uint32_t Count;
    uint32_t InstanceCount;
    uint32_t First;
    uint32_t BaseInstance;
};

struct ShaderInfo
{
    uint32_t SubgroupMaxActiveLanes;
    uint32_t SubgroupSize;
    uint32_t SubgroupCount;
    uint32_t IsSubgroupUniform = 1;
};

static constexpr auto OPENGL_VERSION_MAJOR = 4;
static constexpr auto OPENGL_VERSION_MINOR = 5;
auto Width = 1600;
auto Height = 900;

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, OPENGL_VERSION_MAJOR);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, OPENGL_VERSION_MINOR);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);

    auto window = glfwCreateWindow(Width, Height, "InstancedVsMultiDrawRendering", nullptr, nullptr);
    if (window == nullptr)
    {
        std::string formatted = std::format("Window creation failed. Make sure you have OpenGL {}.{} support. ", OPENGL_VERSION_MAJOR, OPENGL_VERSION_MINOR);
        ExitWithMessage(formatted);
    }

    glfwSetFramebufferSizeCallback(window, [](GLFWwindow* window, int width, int height)
    {
        Width = width;
        Height = height;
        glViewport(0, 0, Width, Height);
    });

    glfwMakeContextCurrent(window);
    glfwSwapInterval(0);
    gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress));

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_MULTISAMPLE);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(MessageCallback, 0);

    // bind a dummy VAO since drawing without one is not allowed by OpenGL
    {
        uint32_t vao;
        glCreateVertexArrays(1, &vao);
        glBindVertexArray(vao);
    }

    // hardcoded uniform locations in the shader program
    constexpr auto uniformLocationUseDrawID = 0;
    constexpr auto uniformLocationCount = 1;
    uint32_t program;
    {
        auto fileData = LoadFile("res/shaders/vertex.glsl");
        auto vertexShader = MakeShader(GL_VERTEX_SHADER, fileData.data());

        fileData = LoadFile("res/shaders/fragment.glsl");
        auto fragmentShader = MakeShader(GL_FRAGMENT_SHADER, fileData.data());

        program = glCreateProgram();
        glAttachShader(program, vertexShader);
        glAttachShader(program, fragmentShader);
        glLinkProgram(program);

        std::string infoLog(4096, '\0');
        glGetProgramInfoLog(program, infoLog.size(), nullptr, infoLog.data());
        std::cout << infoLog;
    }

    // set up draw commands for all triangles
    uint32_t drawCmdBuffer;
    std::vector<DrawArraysIndirectCommand> drawCmds(10'000); // number of triangles - prefer square numbers
    {
        std::fill(drawCmds.begin(), drawCmds.end(), DrawArraysIndirectCommand {
            .Count = 3,
            .InstanceCount = 1,
            .First = 0,
            .BaseInstance = 0,
        });

        glCreateBuffers(1, &drawCmdBuffer);
        glNamedBufferStorage(drawCmdBuffer, sizeof(DrawArraysIndirectCommand) * drawCmds.size(), drawCmds.data(), GL_DYNAMIC_STORAGE_BIT);
    }

    // SSBO used for getting back data from the vertex shader
    uint32_t shaderInfoBuffer;
    {
        glCreateBuffers(1, &shaderInfoBuffer);
        ShaderInfo info{};
        glNamedBufferStorage(shaderInfoBuffer, sizeof(ShaderInfo), &info, GL_DYNAMIC_STORAGE_BIT);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, shaderInfoBuffer);
    }

    // timer query for measuring rendering time
    uint32_t timerQuery;
    {
        glCreateQueries(GL_TIME_ELAPSED, 1, &timerQuery);
    }


    while (!glfwWindowShouldClose(window))
    {
        constexpr float clearColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
        glClearNamedFramebufferfv(0, GL_COLOR, 0, clearColor);

        glProgramUniform1i(program, uniformLocationCount, drawCmds.size());

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, drawCmdBuffer);
        glUseProgram(program);


        float msInstancedRendering = 0.0f;
        ShaderInfo shaderInfoInstancedRendering = {};
        // draw the triangles as a single mesh but multiple instances
        {
            // tell shader program to use gl_InstanceID
            glProgramUniform1ui(program, uniformLocationUseDrawID, false);

            drawCmds[0].InstanceCount = drawCmds.size();
            glNamedBufferSubData(drawCmdBuffer, 0, sizeof(DrawArraysIndirectCommand), drawCmds.data());

            glBeginQuery(GL_TIME_ELAPSED, timerQuery);
            glMultiDrawArraysIndirect(GL_TRIANGLES, nullptr, 1, sizeof(DrawArraysIndirectCommand));
            glEndQuery(GL_TIME_ELAPSED);

            // retrieve measurings from SSBO and reset it
            {
                glGetNamedBufferSubData(shaderInfoBuffer, 0, sizeof(ShaderInfo), &shaderInfoInstancedRendering);
                ShaderInfo info{};
                glNamedBufferSubData(shaderInfoBuffer, 0, sizeof(ShaderInfo), &info);
            }

            // retrieve rendering time
            {
                uint64_t nsTimeElapsed;
                glGetQueryObjectui64v(timerQuery, GL_QUERY_RESULT, &nsTimeElapsed);
                msInstancedRendering = nsTimeElapsed / 1000000.0f;
            }
        }

        float msMeshRendering = 0.0f;
        ShaderInfo shaderInfoMeshRendering = {};
        // draw the triangles as multiple meshes but a single instance
        {
            // tell shader program to use gl_DrawID
            glProgramUniform1ui(program, uniformLocationUseDrawID, true);

            drawCmds[0].InstanceCount = 1;
            glNamedBufferSubData(drawCmdBuffer, 0, sizeof(DrawArraysIndirectCommand), drawCmds.data());

            glBeginQuery(GL_TIME_ELAPSED, timerQuery);
            glMultiDrawArraysIndirect(GL_TRIANGLES, nullptr, drawCmds.size(), sizeof(DrawArraysIndirectCommand));
            glEndQuery(GL_TIME_ELAPSED);

            // retrieve measurings from SSBO and reset it
            {
                glGetNamedBufferSubData(shaderInfoBuffer, 0, sizeof(ShaderInfo), &shaderInfoMeshRendering);
                ShaderInfo info{};
                glNamedBufferSubData(shaderInfoBuffer, 0, sizeof(ShaderInfo), &info);
            }

            // retrieve rendering time
            {
                uint64_t nsTimeElapsed;
                glGetQueryObjectui64v(timerQuery, GL_QUERY_RESULT, &nsTimeElapsed);
                msMeshRendering = nsTimeElapsed / 1000000.0f;
            }
        }

        static bool writeFirstTime = true;
        if (writeFirstTime || glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        {
            if (writeFirstTime)
            {
                std::cout << "SPACE-KEY INSIDE WINDOW TO PRINT UPDATED DATA!\n\n";
            }

            static constexpr auto decimalPlacesTimings = 3;
            static constexpr auto desiredHeadingLength = 66;

            std::string glRenderer(reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
            auto lineLength = desiredHeadingLength - glRenderer.size();
            std::string padding(lineLength / 2, '-');

            auto vertexShaderInvocations = 3 * drawCmds.size();
            std::cout << padding << ' ' << glRenderer << ' ' << padding << '\n';
            {
                std::cout << std::format("* Rendering with gl_InstanceID...: {}ms\n", RoundTo(msInstancedRendering, decimalPlacesTimings));
                std::cout << std::format("* Detected as subgroup-uniform...: {}\n", shaderInfoInstancedRendering.IsSubgroupUniform ? "Yes" : "No");
                std::cout << std::format("* SubgroupCount..................: {}\n", shaderInfoInstancedRendering.SubgroupCount);
                std::cout << std::format("* SubgroupUtilization............: {}/{}\n", shaderInfoInstancedRendering.SubgroupMaxActiveLanes, shaderInfoInstancedRendering.SubgroupSize);
            }
            std::cout << '\n';
            {
                std::cout << std::format("* Rendering with gl_DrawID.......: {}ms\n", RoundTo(msMeshRendering, decimalPlacesTimings));
                std::cout << std::format("* Detected as subgroup-uniform...: {}\n", shaderInfoMeshRendering.IsSubgroupUniform ? "Yes" : "No");
                std::cout << std::format("* SubgroupCount..................: {}\n", shaderInfoMeshRendering.SubgroupCount);
                std::cout << std::format("* SubgroupUtilization............: {}/{}\n", shaderInfoMeshRendering.SubgroupMaxActiveLanes, shaderInfoMeshRendering.SubgroupSize);
            }
            std::cout << '\n';
            std::cout << '\n';

            writeFirstTime = false;
        }

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(window, true);
        }

        glfwPollEvents();
        glfwSwapBuffers(window);
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}