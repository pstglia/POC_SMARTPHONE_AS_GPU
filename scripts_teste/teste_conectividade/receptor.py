import socket

HOST = '0.0.0.0'
PORT = 65432

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((HOST, PORT))

print(f"[*] Servidor espelho aguardando dados na porta {PORT}...")

try:
    while True:
        data, addr = sock.recvfrom(65535)
        if not data:
            break
        # Devolve o pacote imediatamente para quem enviou
        sock.sendto(data, addr)
except KeyboardInterrupt:
    print("\n[*] Parando receptor...")
finally:
    sock.close()

