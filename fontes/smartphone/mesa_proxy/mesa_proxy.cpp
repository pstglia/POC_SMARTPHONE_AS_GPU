#include <iostream>
#include <vector>
#include <cstring>
#include <dlfcn.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <GL/gl.h>
#include <GL/glx.h>

// Assinaturas para ganchos do Pipeline e ciclo de Frame
typedef void (*PFNGLBEGINPROC)(GLenum mode);
typedef void (*PFNGLENDPROC)();
typedef void (*PFNGLVERTEX3FPROC)(GLfloat x, GLfloat y, GLfloat z);
typedef void (*PFNGLCOLOR3FPROC)(GLfloat r, GLfloat g, GLfloat b);
typedef void (*PFNGLXSWAPBUFFERSPROC)(Display* dpy, GLXDrawable drawable);

static PFNGLBEGINPROC real_glBegin = nullptr;
static PFNGLENDPROC real_glEnd = nullptr;
static PFNGLVERTEX3FPROC real_glVertex3f = nullptr;
static PFNGLCOLOR3FPROC real_glColor3f = nullptr;
static PFNGLXSWAPBUFFERSPROC real_glXSwapBuffers = nullptr;
static PFNGLXGETPROCADDRESSPROC real_glXGetProcAddress = nullptr; // Usa o tipo oficial do glx.h

static int sockfd = -1;
static struct sockaddr_in servaddr;
static bool rede_inicializada = false;

struct VerticeProxy {
    float x, y, z;
    float r, g, b;
};

// Cache unificado para o FRAME INTEIRO
static std::vector<VerticeProxy> frame_vertices_accumulator;
static VerticeProxy cor_atual = {0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f};
static bool capturando_geometria = false;

void inicializarRedeProxy() {
    if (rede_inicializada) return;
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    std::memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(65432);
    servaddr.sin_addr.s_addr = inet_addr("10.170.47.49"); // Seu IP fixado do G71
    rede_inicializada = true;
    std::cout << "[Mesa Proxy] Acumulador por frame inicializado via USB!" << std::endl;
}

extern "C" void glColor3f(GLfloat r, GLfloat g, GLfloat b) {
    if (!real_glColor3f) real_glColor3f = (PFNGLCOLOR3FPROC)dlsym(RTLD_NEXT, "glColor3f");
    cor_atual.r = r; cor_atual.g = g; cor_atual.b = b;
    real_glColor3f(r, g, b);
}

extern "C" void glVertex3f(GLfloat x, GLfloat y, GLfloat z) {
    if (!real_glVertex3f) real_glVertex3f = (PFNGLVERTEX3FPROC)dlsym(RTLD_NEXT, "glVertex3f");
    
    if (capturando_geometria) {
        VerticeProxy v = {x, y, z, cor_atual.r, cor_atual.g, cor_atual.b};
        frame_vertices_accumulator.push_back(v);
        // Mantemos engolido para sumir no PC
    } else {
        real_glVertex3f(x, y, z);
    }
}

extern "C" void glBegin(GLenum mode) {
    if (!real_glBegin) real_glBegin = (PFNGLBEGINPROC)dlsym(RTLD_NEXT, "glBegin");
    inicializarRedeProxy();
    capturando_geometria = true;
    // Omitimos a chamada real no PC para desviar
}

extern "C" void glEnd() {
    if (!real_glEnd) real_glEnd = (PFNGLENDPROC)dlsym(RTLD_NEXT, "glEnd");
    capturando_geometria = false;
    // Omitimos a chamada real no PC para desviar
}

// 3. INTERCEPTAÇÃO DO FINAL DO FRAME (O GATILHO DE REDE DEFINITIVO)
extern "C" void glXSwapBuffers(Display* dpy, GLXDrawable drawable) {
    if (!real_glXSwapBuffers) real_glXSwapBuffers = (PFNGLXSWAPBUFFERSPROC)dlsym(RTLD_NEXT, "glXSwapBuffers");

    if (rede_inicializada && !frame_vertices_accumulator.empty()) {
        size_t total_bytes = frame_vertices_accumulator.size() * sizeof(VerticeProxy);
        
        // Limita ao buffer seguro do socket (64KB) para testes estáveis de pipeline fixo
        if (total_bytes <= 64 * 1024) {
            std::cout << "[Mesa Proxy] Despachando frame consolidado de " 
                      << frame_vertices_accumulator.size() << " vértices para o Smartphone..." << std::endl;
            
            sendto(sockfd, frame_vertices_accumulator.data(), total_bytes, 0, 
                   (const struct sockaddr *)&servaddr, sizeof(servaddr));
        }
        
        frame_vertices_accumulator.clear();
    }

    // Deixa o PC atualizar a janela local
    real_glXSwapBuffers(dpy, drawable);
}

// 4. MAPEADOR DINÂMICO USANDO __GLXextFuncPtr DO SISTEMA
extern "C" __GLXextFuncPtr glXGetProcAddress(const GLubyte* procName) {
    if (!real_glXGetProcAddress) {
        real_glXGetProcAddress = (PFNGLXGETPROCADDRESSPROC)dlsym(RTLD_NEXT, "glXGetProcAddress");
    }
    
    std::string nome((const char*)procName);
    if (nome == "glBegin") return (__GLXextFuncPtr)glBegin;
    if (nome == "glEnd") return (__GLXextFuncPtr)glEnd;
    if (nome == "glVertex3f") return (__GLXextFuncPtr)glVertex3f;
    if (nome == "glColor3f") return (__GLXextFuncPtr)glColor3f;
    if (nome == "glXSwapBuffers") return (__GLXextFuncPtr)glXSwapBuffers;
    
    return real_glXGetProcAddress(procName);
}

extern "C" __GLXextFuncPtr glXGetProcAddressARB(const GLubyte* procName) { 
    return glXGetProcAddress(procName); 
}
