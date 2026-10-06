import socket
import time
import struct
import math

TARGET_IP = '192.168.87.42' # Mantenha o IP atual do seu RNDIS
TARGET_PORT = 65432

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.settimeout(1.0)

INTERVALO_FRAME = 1.0 / 30.0  # 30 FPS
NUM_VERTICES = 500  # Gerará 500 vértices dinâmicos a cada frame!

print(f"[*] Enviando geometria dinâmica ({NUM_VERTICES} vértices) a 30 FPS...")

frame_id = 0
angulo = 0.0

try:
    while True:
        start_frame_time = time.time()
        
        # Lista para agrupar os bytes crús de floats (X, Y, Z, R, G, B)
        dados_geometria = bytearray()
        
        # Gera uma malha procedural que muda levemente a cada frame (Animação)
        for i in range(NUM_VERTICES):
            # Onda senoidal simples para animar a posição X/Y
            x = math.sin(angulo + i * 0.05) * 0.5
            y = math.cos(angulo + i * 0.05) * 0.5
            z = 0.0
            
            # Cores dinâmicas baseadas no índice
            r = abs(math.sin(angulo))
            g = abs(math.cos(angulo))
            b = float(i) / float(NUM_VERTICES)
            
            # Empacota em floats binários nativos de 32 bits (Little-Endian)
            dados_geometria.extend(struct.pack('<ffffff', x, y, z, r, g, b))
            
        t_saida = time.time()
        sock.sendto(dados_geometria, (TARGET_IP, TARGET_PORT))
        
        try:
            # Mantém o loop de Ping-Pong esperando o retorno do smartphone
            data, addr = sock.recvfrom(1024)
            latencia_ms = ((time.time() - t_saida) * 1000) / 2
            
            if frame_id % 30 == 0:
                # 6 floats * 4 bytes = 24 bytes por vértice
                tamanho_kb = (NUM_VERTICES * 24) / 1024
                print(f"[Frame {frame_id}] {tamanho_kb:.2f} KB de Geometria | Latência: {latencia_ms:.2f}ms")
        except socket.timeout:
            print(f"[Aviso] Timeout no frame {frame_id}")
            
        frame_id += 1
        angulo += 0.02
        
        tempo_gasto = time.time() - start_frame_time
        tempo_espera = INTERVALO_FRAME - tempo_gasto
        if tempo_espera > 0:
            time.sleep(tempo_espera)
            
except KeyboardInterrupt:
    print("\n[*] Encerrado.")
finally:
    sock.close()

