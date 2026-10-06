#include <iostream>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <EGL/egl.h>
#include <GLES3/gl3.h>

#define PORT 65432
#define MAX_BUFFER_SIZE (64 * 1024)

// Resolução expandida para o teste de estresse
#define RES_WIDTH 480
#define RES_HEIGHT 360
#define FRAME_BYTES (RES_WIDTH * RES_HEIGHT * 3) // RGB (518.400 bytes)

const char* vertexShaderSource = "#version 300 es\n"
    "layout(location = 0) in vec3 aPos;\n"
    "layout(location = 1) in vec3 aColor;\n"
    "out vec3 vColor;\n"
    "void main() {\n"
    "   gl_Position = vec4(aPos, 1.0);\n"
    "   vColor = aColor;\n"
    "}\0";

const char* fragmentShaderSource = "#version 300 es\n"
    "precision mediump float;\n"
    "in vec3 vColor;\n"
    "out vec4 fragColor;\n"
    "void main() {\n"
    "   fragColor = vec4(vColor, 1.0);\n" // Renderiza polígonos com preenchimento de cor sólido
    "}\0";

struct ContextoGrafico {
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLContext context = EGL_NO_CONTEXT;
    EGLSurface surface = EGL_NO_SURFACE;
    GLuint vbo = 0;
    GLuint vao = 0;
    GLuint shaderProgram = 0;
    GLuint pbos[2] = {0, 0}; // Ring Buffer de PBOs para leitura assíncrona da GPU
};

GLuint compilarShader(GLenum tipo, const char* fonte) {
    GLuint shader = glCreateShader(tipo);
    glShaderSource(shader, 1, &fonte, nullptr);
    glCompileShader(shader);
    GLint sucesso;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &sucesso);
    if (!sucesso) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "[-] Erro compilando shader: " << infoLog << std::endl;
    }
    return shader;
}

bool inicializarGPU(ContextoGrafico& gfx) {
    gfx.display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(gfx.display, nullptr, nullptr);

    EGLint atributosConfig[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8,
        EGL_DEPTH_SIZE, 24, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_NONE
    };
    EGLConfig config; EGLint numConfigs;
    eglChooseConfig(gfx.display, atributosConfig, &config, 1, &numConfigs);

    EGLint atributosContexto[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
    gfx.context = eglCreateContext(gfx.display, config, EGL_NO_CONTEXT, atributosContexto);

    EGLint atributosPbuffer[] = { EGL_WIDTH, RES_WIDTH, EGL_HEIGHT, RES_HEIGHT, EGL_NONE };
    gfx.surface = eglCreatePbufferSurface(gfx.display, config, atributosPbuffer);
    eglMakeCurrent(gfx.display, gfx.surface, gfx.surface, gfx.context);

    GLuint vertexShader = compilarShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint fragmentShader = compilarShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    gfx.shaderProgram = glCreateProgram();
    glAttachShader(gfx.shaderProgram, vertexShader);
    glAttachShader(gfx.shaderProgram, fragmentShader);
    glLinkProgram(gfx.shaderProgram);

    glGenVertexArrays(1, &gfx.vao);
    glGenBuffers(1, &gfx.vbo);
    glBindVertexArray(gfx.vao);
    glBindBuffer(GL_ARRAY_BUFFER, gfx.vbo);
    glBufferData(GL_ARRAY_BUFFER, MAX_BUFFER_SIZE, nullptr, GL_STREAM_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // --- CONFIGURAÇÃO DO RING BUFFER DE PBOs ---
    glGenBuffers(2, gfx.pbos);
    for(int i = 0; i < 2; i++) {
        glBindBuffer(GL_PIXEL_PACK_BUFFER, gfx.pbos[i]);
        glBufferData(GL_PIXEL_PACK_BUFFER, FRAME_BYTES, nullptr, GL_STREAM_READ);
    }
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

    glViewport(0, 0, RES_WIDTH, RES_HEIGHT);
    glEnable(GL_DEPTH_TEST); // Ativa buffer de profundidade para polígonos 3D sólidos

    std::cout << "[+] Pipeline de Polígonos Sólidos e Ring Buffer (PBO) Pronto!" << std::endl;
    return true;
}

int main() {
    ContextoGrafico gfx;
    if (!inicializarGPU(gfx)) return -1;

    int sockfd; struct sockaddr_in servaddr, cliaddr;
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    
    // Configura tamanho máximo do buffer do socket UDP de saída no celular para aguentar 518KB
    int optval = 1024 * 1024;
    setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &optval, sizeof(optval));

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(PORT);
    bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr));

    std::vector<char> buffer(MAX_BUFFER_SIZE);
    std::vector<unsigned char> frameBufferRetorno(FRAME_BYTES);
    socklen_t len = sizeof(cliaddr);
    
    int indexRingBuffer = 0; // Alternador do Ring Buffer (0 ou 1)

    while (true) {
        ssize_t n = recvfrom(sockfd, buffer.data(), MAX_BUFFER_SIZE, 0, (struct sockaddr *)&cliaddr, &len);
        if (n < 0) break;

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(gfx.shaderProgram);
        glBindVertexArray(gfx.vao);
        glBindBuffer(GL_ARRAY_BUFFER, gfx.vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, n, buffer.data());

        GLsizei vertexCount = static_cast<GLsizei>(n / (6 * sizeof(float)));
        
        // Desenha os polígonos preenchidos dinâmicos
        glDrawArrays(GL_TRIANGLES, 0, vertexCount);

        // --- ENGENHARIA DO RING BUFFER ASSÍNCRONO ---
        int próximoIndex = (indexRingBuffer + 1) % 2;

        // 1. Vincula o PBO atual para iniciar uma cópia assíncrona do frame recém-desenhado
        glBindBuffer(GL_PIXEL_PACK_BUFFER, gfx.pbos[indexRingBuffer]);
        // Passar nullptr faz o OpenGL ler os pixels direto para a memória do PBO na GPU (sem travar a CPU)
        glReadPixels(0, 0, RES_WIDTH, RES_HEIGHT, GL_RGB, GL_UNSIGNED_BYTE, nullptr);

        // 2. Vincula o OUTRO PBO (que já terminou a cópia no frame anterior) para ler os dados para a RAM
        glBindBuffer(GL_PIXEL_PACK_BUFFER, gfx.pbos[próximoIndex]);
        unsigned char* ptrVRAM = (unsigned char*)glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, FRAME_BYTES, GL_MAP_READ_BIT);
        
        if (ptrVRAM) {
            memcpy(frameBufferRetorno.data(), ptrVRAM, FRAME_BYTES);
            glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
        }
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

        // Alterna o ponteiro do anel para o próximo ciclo
        indexRingBuffer = próximoIndex;

        // Devido ao tamanho do pacote (518.400 bytes), o protocolo UDP padrão estoura se jogarmos tudo em um único pacote.
        // Vamos quebrar o envio de retorno em "Chunks" (Pedaços) de 45 KB para garantir a entrega segura via cabo USB.
        const size_t CHUNK_SIZE = 45 * 1024;
        size_t bytesEnviados = 0;
        uint32_t chunkId = 0;

        while (bytesEnviados < FRAME_BYTES) {
            size_t tamanhoAtual = std::min(CHUNK_SIZE, FRAME_BYTES - bytesEnviados);
            
            // Cria um cabeçalho simples de 8 bytes: [FrameId (4 bytes) | ChunkId (4 bytes)]
            std::vector<unsigned char> pacoteChunk(8 + tamanhoAtual);
            std::memcpy(pacoteChunk.data(), &chunkId, 4);
            std::memcpy(pacoteChunk.data() + 4, &tamanhoAtual, 4);
            std::memcpy(pacoteChunk.data() + 8, frameBufferRetorno.data() + bytesEnviados, tamanhoAtual);

            sendto(sockfd, pacoteChunk.data(), pacoteChunk.size(), 0, (struct sockaddr *)&cliaddr, len);
            bytesEnviados += tamanhoAtual;
            chunkId++;
        }
    }

    close(sockfd);
    glDeleteBuffers(2, gfx.pbos);
    return 0;
}
