import socket

HOST, PORT = "0.0.0.0", 6666

with socket.socket() as s:
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind((HOST, PORT))
    s.listen(1)
    print("Waiting...")
    conn, addr = s.accept()
    with conn:
        print("Connected: ", addr)
        while True:
            data = conn.recv(1024)
            if not data:
                break
            print("Arrived: ", data.decode(errors="replace"))
            conn.sendall(b"ACK")