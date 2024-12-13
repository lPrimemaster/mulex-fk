import socket
import struct
import time
import threading
import numpy as np
import pandas as pd

# Define the host and port
HOST = "127.0.0.1"  # Localhost
PORTT = 2468 # Port to listen on
PORTS = 1357

signals = []
temperatures = []

# Create a TCP socket and bind it to the address
def listen_temp(port):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server_socket:
        server_socket.bind((HOST, port))
        server_socket.listen()
        print(f"Server listening on {HOST}:{port}")

        # Accept connections in a continuous loop
        try:
            while True:
                conn, addr = server_socket.accept()
                print(f"Connected by {addr}")
                try:
                    while True:
                        response_float = np.random.normal(loc=27.5, scale=1.0)
                        temperatures.append((time.time(), response_float))
                        conn.sendall(struct.pack('<f', response_float))
                        print(f"Sent response float to {addr}: {response_float}")
                        time.sleep(1)
                except Exception as e:
                    print(f"Error with client {addr}: {e}")
                finally:
                    conn.close()
                    print(f"Connection with {addr} closed.")
                    break
        except KeyboardInterrupt:
            pass

def listen_signal(port):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server_socket:
        server_socket.bind((HOST, port))
        server_socket.listen()
        print(f"Server listening on {HOST}:{port}")

        # Accept connections in a continuous loop
        try:
            while True:
                conn, addr = server_socket.accept()
                print(f"Connected by {addr}")
                try:
                    while True:
                        response = np.floor(np.clip(np.random.normal(5.0, 2.0, 1000), 0, 10) * (255 / 10)).astype(np.uint8)
                        signals.append((time.time(), response))
                        conn.sendall(bytes(response))
                        print(f"Sent response to {addr}")
                        time.sleep(0.1)
                except Exception as e:
                    print(f"Error with client {addr}: {e}")
                finally:
                    conn.close()
                    print(f"Connection with {addr} closed.")
                    break
        except KeyboardInterrupt:
            pass

tt = threading.Thread(target=listen_temp, args=(PORTT,))
tt.start()

ts = threading.Thread(target=listen_signal, args=(PORTS,))
ts.start()

tt.join()
ts.join()

ts, s = zip(*signals)
tt, t = zip(*temperatures)

i = np.digitize(ts, tt) - 1

print(i)

df = pd.DataFrame({
    'timestamp': ts,
    'signal': s,
    'temperature': np.array(t)[i]
})

df.to_pickle('python_coincidence.pkl')
