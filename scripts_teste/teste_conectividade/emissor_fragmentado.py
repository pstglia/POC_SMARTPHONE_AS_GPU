import socket
import time
import struct
import math
import pygame
import sys

TARGET_IP = '10.170.47.49'
TARGET_PORT = 65432

LARGURA, ALTURA = 480, 360
FRAME_BYTES = LARGURA * ALTURA * 3

pygame.init()
tela = pygame.display.set_mode((LARGURA, ALTURA))
pygame.display.set_caption(f"PC Xeon - Renderização Sólida {LARGURA}x{ALTURA}")

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4 * 1024 * 1024) # 4MB de Buffer no PC
sock.settimeout(0.5)

INTERVALO_FRAME = 1.0 / 30.0
# Vamos enviar triângulos preenchidos acoplados (Modo TRIANGLES: múltiplos de 3 vértices)
NUM_VERTICES = 180 

frame_id = 0
angulo = 0.0

# Dicionário/Buffer para remontar a imagem recebida por pedaços
frame_buffer_remontado = bytearray(FRAME_BYTES)

print(f"[*] Solicitando renderização sólida em {LARGURA}x{ALTURA} com reconstrução de fragmentos...")

try:
    while True:
        start_frame_time = time.time()
        
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                sys.exit()

        # Gera múltiplos triângulos sólidos dinâmicos intersecionados
        dados_geometria = bytearray()
        for i in range(0, NUM_VERTICES, 3):
            # Vértice 1
            x1 = math.sin(angulo + i * 0.05) * 0.6
            y1 = math.cos(angulo + i * 0.05) * 0.6
            dados_geometria.extend(struct.pack('<ffffff', x1, y1, 0.0, 1.0, 0.0, 0.0)) # Vermelho
            
            # Vértice 2
            x2 = x1 + 0.2
            y2 = y1 + 0.3
            dados_geometria.extend(struct.pack('<ffffff', x2, y2, 0.0, 0.0, 1.0, 0.0)) # Verde
            
            # Vértice 3
            x3 = x1 - 0.2
            y3 = y1 + 0.3
            dados_geometria.extend(struct.pack('<ffffff', x3, y3, 0.0, 0.0, 0.0, 1.0)) # Azul

        t_saida = time.time()
        sock.sendto(dados_geometria, (TARGET_IP, TARGET_PORT))

        bytes_reunidos = 0
        try:
            # Loop interno para pescar todos os Chunks enviados pelo celular para este frame
            while bytes_reunidos < FRAME_BYTES:
                packet, addr = sock.recvfrom(65535)
                if len(packet) < 8:
                    continue
                
                # Desempacota o cabeçalho do Chunk
                chunk_id, chunk_size = struct.unpack('<II', packet[:8])
                payload = packet[8:]
                
                # Insere o pedaço na posição exata da imagem final
                offset = chunk_id * (45 * 1024)
                frame_buffer_remontado[offset:offset+len(payload)] = payload
                bytes_reunidos += len(payload)

            latencia_ms = (time.time() - t_saida) * 1000

            # Joga na tela do PC
            imagem_surface = pygame.image.fromstring(bytes(frame_buffer_remontado), (LARGURA, ALTURA), "RGB")
            imagem_surface = pygame.transform.flip(imagem_surface, False, True)
            tela.blit(imagem_surface, (0, 0))
            pygame.display.flip()

            if frame_id % 30 == 0:
                print(f"[Frame {frame_id}] Recomposto: {bytes_reunidos/1024:.1f} KB | Latência: {latencia_ms:.2f}ms")
                
        except socket.timeout:
            print(f"[Aviso] Frame {frame_id} perdeu fragmentos no cabo (Timeout)")

        frame_id += 1
        angulo += 0.03
        
        tempo_gasto = time.time() - start_frame_time
        tempo_wait = INTERVALO_FRAME - tempo_gasto
        if tempo_wait > 0:
            time.sleep(tempo_wait)

except KeyboardInterrupt:
    print("\n[*] Parando.")
finally:
    sock.close()
    pygame.quit()
