#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (C) 2026 Stephane P. Pericat
"""Local stand-in for awkward radio servers, for the G1 protocol tests (docs/p0/RADIO-COMPATIBILITY.md).

    python3 -I tools/stream_test_server.py clip.mp3 [port [clipB.mp3 clipC.mp3]]

Serves the clip in a loop at real-time pace on 127.0.0.1. Paths:
  /live      plain Icecast-style stream            /redirect  302 -> /live
  /list.m3u  playlist -> /live                     /list.pls  playlist -> /live
  /hls.m3u8  an HLS playlist (unsupported)         /ogg       Content-Type application/ogg (unsupported)
  /icy       legacy Shoutcast "ICY 200 OK" status  /missing   404
  /drop      closes after 3 s, every time          /stall     sends 3 s, then goes silent with the socket open
  /meta      ignores "Icy-MetaData: 0" and interleaves metadata anyway (badly behaved server)
  /a /b /c   three different clips (clip, clipB, clipC), for telling stations apart in the tuner tests
  /wait/N    waits N seconds before answering, then streams like /live
  /silent    accepts the connection and never answers
"""
import socket, sys, threading, time

clip = open(sys.argv[1], 'rb').read()
port = int(sys.argv[2]) if len(sys.argv) > 2 else 8765
clips = {'/a': clip, '/b': open(sys.argv[3], 'rb').read() if len(sys.argv) > 3 else clip, '/c': open(sys.argv[4], 'rb').read() if len(sys.argv) > 4 else clip}
BYTERATE = 16000  # 128 kb/s

def stream(conn, seconds=None, stall=False, meta=False, clip=clip):
    start, sent, pos = time.time(), 0, 0
    burst = BYTERATE * 2                       # like Icecast: a burst first, then real time
    while True:
        chunk = clip[pos:pos + 4000] or clip[:4000]
        pos = (pos + 4000) % len(clip)
        if meta:
            out = b''
            for i in range(0, len(chunk), 1000):
                out += chunk[i:i + 1000] + bytes([1]) + b"StreamTitle='x';".ljust(16, b'\0')
            chunk = out
        conn.sendall(chunk)
        sent += len(chunk)
        if seconds is not None and time.time() - start > seconds:
            if stall: time.sleep(3600)
            return
        ahead = (sent - burst) / BYTERATE - (time.time() - start)
        if ahead > 0: time.sleep(ahead)

def handle(conn):
    try:
        request = conn.recv(4096).decode('latin1')
        path = request.split(' ')[1] if ' ' in request else '/'
        base = f'http://127.0.0.1:{port}'
        def head(status='HTTP/1.0 200 OK', ctype='audio/mpeg', extra=''):
            conn.sendall(f'{status}\r\nContent-Type: {ctype}\r\nicy-name: Local test\r\n{extra}\r\n'.encode())
        def text(body, ctype):
            conn.sendall(f'HTTP/1.0 200 OK\r\nContent-Type: {ctype}\r\nContent-Length: {len(body)}\r\n\r\n{body}'.encode())
        if path == '/live': head(); stream(conn)
        elif path in clips: head(); stream(conn, clip=clips[path])
        elif path.startswith('/wait/'): time.sleep(float(path[6:])); head(); stream(conn)
        elif path == '/silent': time.sleep(3600)
        elif path == '/redirect': conn.sendall(f'HTTP/1.0 302 Found\r\nLocation: {base}/live\r\n\r\n'.encode())
        elif path == '/list.m3u': text(f'#EXTM3U\n#EXTINF:-1,Local\n{base}/live\n', 'audio/x-mpegurl')
        elif path == '/list.pls': text(f'[playlist]\nNumberOfEntries=1\nFile1={base}/live\nTitle1=Local\n', 'audio/x-scpls')
        elif path == '/hls.m3u8': text('#EXTM3U\n#EXT-X-VERSION:3\n#EXT-X-TARGETDURATION:6\n#EXTINF:6,\nseg0.ts\n', 'application/vnd.apple.mpegurl')
        elif path == '/ogg': head(ctype='application/ogg'); stream(conn, 2)
        elif path == '/icy': head(status='ICY 200 OK'); stream(conn)
        elif path == '/drop': head(); stream(conn, 3)
        elif path == '/stall': head(); stream(conn, 3, stall=True)
        elif path == '/meta': head(extra='icy-metaint: 1000\r\n'); stream(conn, meta=True)
        else: conn.sendall(b'HTTP/1.0 404 Not Found\r\nContent-Length: 0\r\n\r\n')
    except OSError:
        pass
    finally:
        conn.close()

server = socket.socket()
server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
server.bind(('127.0.0.1', port))
server.listen(8)
while True:
    threading.Thread(target=handle, args=(server.accept()[0],), daemon=True).start()
