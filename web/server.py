#!/usr/bin/env python3
import http.server
import socketserver
import json
import subprocess
import os
import tempfile
import sys
import re
import webbrowser
import threading
import time

DEFAULT_PORT = 8080
ROOT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.abspath(os.path.join(ROOT_DIR, ".."))
BIN_MAIN = os.path.join(PROJECT_DIR, "querylang.exe")
BIN_V1 = os.path.join(PROJECT_DIR, "v1_baseline", "querylang_v1.exe")
BIN_V2 = os.path.join(PROJECT_DIR, "v2_simd", "querylang_v2.exe")
UPLOADS_DIR = os.path.join(PROJECT_DIR, "data", "uploads")

os.makedirs(UPLOADS_DIR, exist_ok=True)

class QueryLangHandler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=ROOT_DIR, **kwargs)

    def do_GET(self):
        if self.path == "/api/datasets":
            datasets = self.get_available_datasets()
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()
            self.wfile.write(json.dumps(datasets).encode("utf-8"))
        else:
            super().do_GET()

    def get_available_datasets(self):
        datasets = []
        
        # Built-in sample datasets
        sample_files = [
            ("employees.csv", "data/employees.csv", "Standard Employee Records (10 rows, salary, departments)"),
            ("students.csv", "data/students.csv", "Student Academic Records (5 rows, GPA, branch)"),
            ("large_employees.csv", "data/large_employees.csv", "Large Employee Scale Dataset (250K rows)"),
            ("benchmark_100k.csv", "data/benchmark_100k.csv", "100K Benchmark Dataset (Performance testing)")
        ]

        for fname, rel_path, desc in sample_files:
            full_path = os.path.join(PROJECT_DIR, rel_path.replace("/", os.sep))
            if os.path.exists(full_path):
                cols = []
                try:
                    with open(full_path, "r", encoding="utf-8", errors="ignore") as f:
                        header = f.readline().strip()
                        cols = [c.strip() for c in header.split(",") if c.strip()]
                except Exception:
                    pass
                size_kb = os.path.getsize(full_path) / 1024.0
                size_str = f"{size_kb:.1f} KB" if size_kb < 1024 else f"{(size_kb/1024):.2f} MB"
                datasets.append({
                    "name": fname,
                    "path": rel_path,
                    "isSample": True,
                    "description": desc,
                    "columns": cols,
                    "size": size_str
                })

        # Uploaded datasets
        if os.path.exists(UPLOADS_DIR):
            for fname in sorted(os.listdir(UPLOADS_DIR)):
                if fname.lower().endswith(".csv"):
                    full_path = os.path.join(UPLOADS_DIR, fname)
                    rel_path = f"data/uploads/{fname}"
                    cols = []
                    row_count = 0
                    try:
                        with open(full_path, "r", encoding="utf-8", errors="ignore") as f:
                            header = f.readline().strip()
                            cols = [c.strip() for c in header.split(",") if c.strip()]
                            for _ in f:
                                row_count += 1
                    except Exception:
                        pass
                    size_kb = os.path.getsize(full_path) / 1024.0
                    size_str = f"{size_kb:.1f} KB" if size_kb < 1024 else f"{(size_kb/1024):.2f} MB"
                    datasets.append({
                        "name": fname,
                        "path": rel_path,
                        "isSample": False,
                        "description": f"User Uploaded ({row_count} rows)",
                        "columns": cols,
                        "size": size_str
                    })

        return datasets

    def do_POST(self):
        if self.path == "/api/upload":
            length = int(self.headers.get("Content-Length", 0))
            body = self.rfile.read(length)
            try:
                data = json.loads(body.decode("utf-8"))
                filename = data.get("filename", "dataset.csv")
                content = data.get("content", "")

                clean_name = re.sub(r'[^a-zA-Z0-9_\-\.]', '_', os.path.basename(filename))
                if not clean_name.lower().endswith(".csv"):
                    clean_name += ".csv"

                target_path = os.path.join(UPLOADS_DIR, clean_name)
                with open(target_path, "w", encoding="utf-8") as f:
                    f.write(content)

                lines = [line.strip() for line in content.splitlines() if line.strip()]
                columns = []
                preview_rows = []
                if lines:
                    columns = [c.strip() for c in lines[0].split(",") if c.strip()]
                    for row in lines[1:6]:
                        preview_rows.append([cell.strip() for cell in row.split(",")])

                row_count = max(0, len(lines) - 1)
                rel_path = f"data/uploads/{clean_name}"

                resp = {
                    "success": True,
                    "filename": clean_name,
                    "path": rel_path,
                    "columns": columns,
                    "preview": preview_rows,
                    "rowCount": row_count,
                    "message": f"Successfully uploaded '{clean_name}' with {row_count} rows and {len(columns)} columns."
                }
            except Exception as e:
                resp = {"success": False, "error": str(e)}

            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()
            self.wfile.write(json.dumps(resp).encode("utf-8"))

        elif self.path == "/api/execute":
            length = int(self.headers.get("Content-Length", 0))
            body = self.rfile.read(length)
            req = json.loads(body.decode("utf-8"))

            query = req.get("query", "")
            stage = req.get("stage", "all")
            engine = req.get("engine", "simd")
            benchmark = req.get("benchmark", False)

            exe = BIN_MAIN
            if engine == "scalar" and os.path.exists(BIN_V1):
                exe = BIN_V1
            elif engine == "simd" and os.path.exists(BIN_V2):
                exe = BIN_V2

            flag_map = {
                "lex": "--lex",
                "syntax": "--syntax",
                "ast": "--ast",
                "symtab": "--symtab",
                "semantic": "--semantic",
                "tac": "--tac",
                "exec": "--exec",
                "all": "--all"
            }
            flag = flag_map.get(stage, "--all")

            cmd = [exe, flag]
            if benchmark and "--benchmark" not in cmd and flag in ["--all", "--exec"]:
                cmd.append("-b")
            if engine == "scalar" and "--scalar" not in cmd:
                cmd.append("--scalar")
            elif engine == "simd" and "--simd" not in cmd:
                cmd.append("--simd")

            with tempfile.NamedTemporaryFile(mode="w", suffix=".ql", delete=False, dir=PROJECT_DIR) as f:
                f.write(query)
                temp_filename = f.name

            try:
                cmd.append(temp_filename)
                res = subprocess.run(cmd, cwd=PROJECT_DIR, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
                output = res.stdout
            except Exception as e:
                output = f"Execution error: {str(e)}"
            finally:
                if os.path.exists(temp_filename):
                    try:
                        os.remove(temp_filename)
                    except Exception:
                        pass

            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()
            self.wfile.write(json.dumps({"output": output}).encode("utf-8"))
        else:
            self.send_error(404, "Endpoint Not Found")

def open_browser_delayed(url):
    time.sleep(0.6)
    print(f"[*] Opening browser at {url} ...", flush=True)
    try:
        webbrowser.open(url)
    except Exception as e:
        print(f"[!] Could not open browser automatically: {e}", flush=True)

def main():
    os.chdir(PROJECT_DIR)
    socketserver.TCPServer.allow_reuse_address = True

    # Try port 8080 first, fallback to 8081..8090 if busy
    selected_port = DEFAULT_PORT
    httpd = None
    for port in range(DEFAULT_PORT, DEFAULT_PORT + 10):
        try:
            httpd = socketserver.TCPServer(("", port), QueryLangHandler)
            selected_port = port
            break
        except OSError:
            print(f"Port {port} is busy, trying next port...", flush=True)

    if not httpd:
        print("ERROR: Could not bind to any port between 8080 and 8090.", flush=True)
        sys.exit(1)

    url = f"http://localhost:{selected_port}"
    print("============================================================", flush=True)
    print(" QueryLang Interactive Compiler IDE & Visualizer Frontend", flush=True)
    print(f" Running at: {url}", flush=True)
    print(" Press Ctrl+C in this terminal window to stop the server.", flush=True)
    print("============================================================", flush=True)

    # Launch browser only after server is successfully bound and running
    threading.Thread(target=open_browser_delayed, args=(url,), daemon=True).start()

    with httpd:
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\nShutting down QueryLang server.", flush=True)

if __name__ == "__main__":
    main()
