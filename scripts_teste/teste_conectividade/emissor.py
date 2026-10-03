import socket
import time

TARGET_IP = '192.168.228.184' # IP do celular
TARGET_PORT = 65432

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.settimeout(1.0) # Evita travar o PC se o celular perder um pacote

TAMANHO_FRAME_BYTES = 50 * 1024 
INTERVALO_FRAME = 1.0 / 30.0  # 30 FPS

print(f"[*] Testando latência real (Ping-Pong) contra {TARGET_IP}...")

frame_id = 0
total_latencia = 0
latencias_validas = 0

try:
    while True:
        start_frame_time = time.time()
        
        # Cria o pacote simulado de 50KB
        pacote_grafico = b'\x00' * TAMANHO_FRAME_BYTES
        
        # Envia e marca o tempo exato de saída
        t_saida = time.time()
        sock.sendto(pacote_grafico, (TARGET_IP, TARGET_PORT))
        
        try:
            # Aguarda o retorno do celular
            data, addr = sock.recvfrom(65535)
            t_chegada = time.time()
            
            # Latência de ida (estimada como metade do tempo total de ida e volta)
            latencia_ms = ((t_chegada - t_saida) * 1000) / 2
            total_latencia += latencia_ms
            latencias_validas += 1
            
            if frame_id % 30 == 0:
                lat_media = total_latencia / latencias_validas if latencias_validas > 0 else 0
                print(f"[PC -> Frame {frame_id}] Latência Real Estimada (Ida): {latencia_ms:.2f}ms | Média: {lat_media:.2f}ms")
        except socket.timeout:
            print(f"[Aviso] Frame {frame_id} perdeu o tempo de resposta (Timeout)")
            
        frame_id += 1
        
        # Frame pacing para manter 30 FPS
        tempo_gasto = time.time() - start_frame_time
        tempo_espera = INTERVALO_FRAME - tempo_gasto
        if tempo_espera > 0:
            time.sleep(tempo_espera)
            
except KeyboardInterrupt:
    print("\n[*] Transmissão encerrada.")
finally:
    sock.close()

