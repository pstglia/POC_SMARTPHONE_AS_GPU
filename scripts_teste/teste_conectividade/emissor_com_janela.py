import socket
import time
import struct
import math
import pygame
import sys

TARGET_IP = '192.168.87.42' # Seu IP RNDIS
TARGET_PORT = 65432

# Configuração da Janela de Exibição no PC
LARGURA, ALTURA = 160, 120
pygame.init()
tela = pygame.display.set_mode((LARGURA, ALTURA))
pygame.display.set_caption("PC Xeon - Renderização do Smartphone")

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
# Aumenta o buffer de recepção do PC para não truncar os 57.600 bytes da imagem
sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1024 * 1024)
sock.settimeout(1.0)

INTERVALO_FRAME = 1.0 / 30.0
NUM_VERTICES = 120 # Quantidade reduzida para testes rápidos

frame_id = 0
angulo = 0.0

print(f"[*] Solicitando e exibindo renderização remota em {LARGURA}x{ALTURA}...")

try:
    while True:
        start_frame_time = time.time()
        
        # Fecha a aplicação se fechar a janela do Pygame
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                sys.exit()

        # Gera geometria procedural animada
        dados_geometria = bytearray()
        for i in range(NUM_VERTICES):
            x = math.sin(angulo + i * 0.1) * 0.8
            y = math.cos(angulo + i * 0.1) * 0.8
            z = 0.0
            r = abs(math.sin(angulo))
            g = abs(math.cos(angulo))
            b = float(i) / float(NUM_VERTICES)
            dados_geometria.extend(struct.pack('<ffffff', x, y, z, r, g, b))

        # Envia a malha 3D pelo cabo USB
        t_saida = time.time()
        sock.sendto(dados_geometria, (TARGET_IP, TARGET_PORT))

        try:
            # Aguarda a imagem RGB bruta de retorno (57600 bytes)
            dados_imagem, addr = sock.recvfrom(65535)
            latencia_ms = (time.time() - t_saida) * 1000

            if len(dados_imagem) == LARGURA * ALTURA * 3:
                # Converte os bytes crús recebidos em uma superfície do Pygame e joga na tela
                imagem_surface = pygame.image.fromstring(dados_imagem, (LARGURA, ALTURA), "RGB")
                
                # Inverte verticalmente pois o OpenGL conta os pixels de baixo para cima
                imagem_surface = pygame.transform.flip(imagem_surface, False, True)
                
                tela.blit(imagem_surface, (0, 0))
                pygame.display.flip()

            if frame_id % 30 == 0:
                print(f"[Frame {frame_id}] Imagem Recebida | Latência de Loop Completo: {latencia_ms:.2f}ms")
        except socket.timeout:
            print(f"[Aviso] Timeout no frame {frame_id}")

        frame_id += 1
        angulo += 0.05

        tempo_gasto = time.time() - start_frame_time
        tempo_espera = INTERVALO_FRAME - tempo_gasto
        if tempo_espera > 0:
            time.sleep(tempo_espera)

except KeyboardInterrupt:
    print("\n[*] Parando emissor.")
finally:
    sock.close()
    pygame.quit()
