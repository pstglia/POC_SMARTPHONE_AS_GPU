#include <iostream>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <EGL/egl.h>
#include <GLES3/gl3.h>

#define PORT 65432
#define MAX_BUFFER_SIZE (64 * 1024) // Buffer seguro de 64 KB

struct ContextoGrafico {
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLContext context = EGL_NO_CONTEXT;
    EGLSurface surface = EGL_NO_SURFACE;
    GLuint vbo = 0; // Ponteiro para o buffer de memória na GPU
};

bool inicializarGPU(ContextoGrafico& gfx) {
    gfx.display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (!eglInitialize(gfx.display, nullptr, nullptr)) return false;

    EGLint atributosConfig[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_NONE
    };

    EGLConfig config; EGLint numConfigs;
    eglChooseConfig(gfx.display, atributosConfig, &config, 1, &numConfigs);

    EGLint atributosContexto[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
    gfx.context = eglCreateContext(gfx.display, config, EGL_NO_CONTEXT, atributosContexto);

    EGLint atributosPbuffer[] = { EGL_WIDTH, 1280, EGL_HEIGHT, 720, EGL_NONE };
    gfx.surface = eglCreatePbufferSurface(gfx.display, config, atributosPbuffer);
    eglMakeCurrent(gfx.display, gfx.surface, gfx.surface, gfx.context);

    // --- ALOCAÇÃO DE MEMÓRIA NA GPU ADRENO ---
    // Cria um objeto de buffer de vértices (VBO) na GPU
    glGenBuffers(1, &gfx.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, gfx.vbo);
    
    // Reserva 64KB de espaço físico inicial na VRAM da GPU (Modo STREAM para atualizações a cada frame)
    glBufferData(GL_ARRAY_BUFFER, MAX_BUFFER_SIZE, nullptr, GL_STREAM_DRAW);
    
    std::cout << "[+] GPU Adreno 506 Pronta. Buffer de VRAM Alocado!" << std::endl;
    return true;
}

int main() {
    ContextoGrafico gfx;
    if (!inicializarGPU(gfx)) return -1;

    int sockfd; struct sockaddr_in servaddr, cliaddr;
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(PORT);
    bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr));

    std::cout << "[*] Aguardando geometria procedural na porta " << PORT << "..." << std::endl;

    std::vector<char> buffer(MAX_BUFFER_SIZE);
    socklen_t len = sizeof(cliaddr);
    unsigned int frameCount = 0;

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

    while (true) {
        ssize_t n = recvfrom(sockfd, buffer.data(), MAX_BUFFER_SIZE, 0, (struct sockaddr *)&cliaddr, &len);
        if (n < 0) break;

        // --- ENGENHARIA DE CO-PROCESSAMENTO (UPLOADING REAL PARA A GPU) ---
        // 1. Limpa o Framebuffer virtual
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        // 2. Vincula o nosso VBO alocado na VRAM
        glBindBuffer(GL_ARRAY_BUFFER, gfx.vbo);
        
        // 3. Força o mapeamento direto de memória do pacote de rede direto para a GPU Adreno
        glBufferSubData(GL_ARRAY_BUFFER, 0, n, buffer.data());
        
        // 4. Sincroniza a GPU para garantir que o upload terminou
        glFinish();

        frameCount++;
        if (frameCount % 30 == 0) {
            std::cout << "[GPU] Upload efetuado com sucesso para " << n << " bytes de vértices." << std::endl;
        }

        // Devolve uma resposta curta de confirmação para o PC (Apenas os primeiros 4 bytes para reduzir tráfego de retorno)
        sendto(sockfd, buffer.data(), 4, 0, (struct sockaddr *)&cliaddr, len);
    }

    close(sockfd);
    glDeleteBuffers(1, &gfx.vbo);
    return 0;
}

