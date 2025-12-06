// WiFi File Server implementation

#include "fileserver.h"
#include <SD.h>
#include <ESPmDNS.h>

// Static members
WebServer* FileServer::server = nullptr;
bool FileServer::running = false;
char FileServer::statusMessage[64] = "Ready";

// Black & white HTML interface
static const char HTML_TEMPLATE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>PORKCHOP File Manager</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body { 
            background: #000; 
            color: #fff; 
            font-family: 'Courier New', monospace;
            padding: 20px;
            max-width: 800px;
            margin: 0 auto;
        }
        h1 { 
            border-bottom: 2px solid #fff; 
            padding-bottom: 10px; 
            margin-bottom: 20px;
            font-size: 1.5em;
        }
        .folder { 
            margin: 10px 0; 
            padding: 10px; 
            border: 1px solid #444;
        }
        .folder-name { 
            font-weight: bold; 
            margin-bottom: 10px;
            color: #aaa;
        }
        .file { 
            display: flex; 
            justify-content: space-between; 
            align-items: center;
            padding: 8px;
            border-bottom: 1px solid #333;
        }
        .file:hover { background: #111; }
        .file-name { flex: 1; }
        .file-size { color: #888; margin: 0 15px; }
        .btn {
            background: #fff;
            color: #000;
            border: none;
            padding: 5px 12px;
            cursor: pointer;
            font-family: inherit;
            font-size: 0.9em;
            margin-left: 5px;
        }
        .btn:hover { background: #ccc; }
        .btn-del { background: #333; color: #fff; border: 1px solid #fff; }
        .btn-del:hover { background: #500; }
        .upload-form {
            margin-top: 20px;
            padding: 15px;
            border: 1px solid #fff;
        }
        .upload-form input[type="file"] { 
            margin: 10px 0;
            color: #fff;
        }
        .status { 
            color: #888; 
            margin-top: 20px; 
            font-size: 0.9em;
        }
        .refresh-btn {
            float: right;
            margin-top: -35px;
        }
        select {
            background: #000;
            color: #fff;
            border: 1px solid #fff;
            padding: 5px;
            font-family: inherit;
        }
    </style>
</head>
<body>
    <h1>PORKCHOP File Manager</h1>
    <button class="btn refresh-btn" onclick="loadFiles()">Refresh</button>
    
    <div id="files"></div>
    
    <div class="upload-form">
        <strong>Upload File</strong><br>
        <form id="uploadForm" enctype="multipart/form-data">
            <select id="uploadDir">
                <option value="/handshakes">/handshakes</option>
                <option value="/wardriving">/wardriving</option>
                <option value="/mldata">/mldata</option>
                <option value="/models">/models</option>
                <option value="/">/</option>
            </select>
            <input type="file" id="fileInput" name="file">
            <button type="submit" class="btn">Upload</button>
        </form>
    </div>
    
    <div class="status" id="status">Ready</div>
    
    <script>
        const dirs = ['/handshakes', '/wardriving', '/mldata', '/models', '/logs'];
        
        async function loadFiles() {
            const container = document.getElementById('files');
            container.innerHTML = 'Loading...';
            let html = '';
            
            for (const dir of dirs) {
                try {
                    const resp = await fetch('/api/ls?dir=' + encodeURIComponent(dir));
                    const files = await resp.json();
                    
                    if (files.length > 0) {
                        html += '<div class="folder">';
                        html += '<div class="folder-name">' + dir + '/</div>';
                        for (const f of files) {
                            html += '<div class="file">';
                            html += '<span class="file-name">' + f.name + '</span>';
                            html += '<span class="file-size">' + formatSize(f.size) + '</span>';
                            html += '<button class="btn" onclick="download(\'' + dir + '/' + f.name + '\')">Download</button>';
                            html += '<button class="btn btn-del" onclick="del(\'' + dir + '/' + f.name + '\')">X</button>';
                            html += '</div>';
                        }
                        html += '</div>';
                    }
                } catch (e) {}
            }
            
            container.innerHTML = html || '<p>No files found</p>';
        }
        
        function formatSize(bytes) {
            if (bytes < 1024) return bytes + ' B';
            if (bytes < 1024*1024) return (bytes/1024).toFixed(1) + ' KB';
            return (bytes/1024/1024).toFixed(1) + ' MB';
        }
        
        function download(path) {
            window.location.href = '/download?f=' + encodeURIComponent(path);
        }
        
        async function del(path) {
            if (!confirm('Delete ' + path + '?')) return;
            const resp = await fetch('/delete?f=' + encodeURIComponent(path));
            if (resp.ok) {
                document.getElementById('status').textContent = 'Deleted: ' + path;
                loadFiles();
            } else {
                document.getElementById('status').textContent = 'Delete failed';
            }
        }
        
        document.getElementById('uploadForm').onsubmit = async function(e) {
            e.preventDefault();
            const fileInput = document.getElementById('fileInput');
            const dir = document.getElementById('uploadDir').value;
            
            if (!fileInput.files.length) {
                alert('Select a file first');
                return;
            }
            
            const formData = new FormData();
            formData.append('file', fileInput.files[0]);
            
            document.getElementById('status').textContent = 'Uploading...';
            
            try {
                const resp = await fetch('/upload?dir=' + encodeURIComponent(dir), {
                    method: 'POST',
                    body: formData
                });
                
                if (resp.ok) {
                    document.getElementById('status').textContent = 'Upload complete!';
                    fileInput.value = '';
                    loadFiles();
                } else {
                    document.getElementById('status').textContent = 'Upload failed';
                }
            } catch (e) {
                document.getElementById('status').textContent = 'Upload error: ' + e.message;
            }
        };
        
        loadFiles();
    </script>
</body>
</html>
)rawliteral";

void FileServer::init() {
    running = false;
    strcpy(statusMessage, "Ready");
}

bool FileServer::start(const char* ssid, const char* password) {
    if (running) return true;
    
    // Check credentials
    if (!ssid || strlen(ssid) == 0) {
        strcpy(statusMessage, "No WiFi SSID configured");
        return false;
    }
    
    strcpy(statusMessage, "Connecting...");
    Serial.printf("[FILESERVER] Connecting to %s\n", ssid);
    
    // Disconnect from any previous connection
    WiFi.disconnect(true);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    
    // Wait for connection (max 15 seconds)
    int timeout = 30;  // 30 * 500ms = 15s
    while (WiFi.status() != WL_CONNECTED && timeout > 0) {
        delay(500);
        Serial.print(".");
        timeout--;
    }
    Serial.println();
    
    if (WiFi.status() != WL_CONNECTED) {
        strcpy(statusMessage, "Connection failed");
        Serial.println("[FILESERVER] Connection failed");
        WiFi.disconnect(true);
        return false;
    }
    
    snprintf(statusMessage, sizeof(statusMessage), "Connected: %s", WiFi.localIP().toString().c_str());
    Serial.printf("[FILESERVER] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
    
    // Start mDNS
    if (MDNS.begin("porkchop")) {
        Serial.println("[FILESERVER] mDNS: porkchop.local");
    }
    
    // Create and configure web server
    server = new WebServer(80);
    
    server->on("/", HTTP_GET, handleRoot);
    server->on("/api/ls", HTTP_GET, handleFileList);
    server->on("/download", HTTP_GET, handleDownload);
    server->on("/upload", HTTP_POST, handleUpload, handleUploadProcess);
    server->on("/delete", HTTP_GET, handleDelete);
    server->onNotFound(handleNotFound);
    
    server->begin();
    running = true;
    
    Serial.println("[FILESERVER] Server started on port 80");
    return true;
}

void FileServer::stop() {
    if (!running) return;
    
    if (server) {
        server->stop();
        delete server;
        server = nullptr;
    }
    
    MDNS.end();
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    
    running = false;
    strcpy(statusMessage, "Stopped");
    Serial.println("[FILESERVER] Stopped");
}

void FileServer::update() {
    if (running && server) {
        server->handleClient();
    }
}

void FileServer::handleRoot() {
    server->send(200, "text/html", HTML_TEMPLATE);
}

void FileServer::handleFileList() {
    String dir = server->arg("dir");
    if (dir.isEmpty()) dir = "/";
    
    // Security: prevent directory traversal
    if (dir.indexOf("..") >= 0) {
        server->send(400, "application/json", "[]");
        return;
    }
    
    File root = SD.open(dir);
    if (!root || !root.isDirectory()) {
        server->send(200, "application/json", "[]");
        return;
    }
    
    String json = "[";
    bool first = true;
    
    File file = root.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            if (!first) json += ",";
            first = false;
            
            // Escape filename for JSON
            String fname = file.name();
            fname.replace("\\", "\\\\");
            fname.replace("\"", "\\\"");
            
            json += "{\"name\":\"";
            json += fname;
            json += "\",\"size\":";
            json += String(file.size());
            json += "}";
        }
        file.close();  // Close each file handle
        file = root.openNextFile();
    }
    
    root.close();  // Close directory handle
    json += "]";
    server->send(200, "application/json", json);
}

void FileServer::handleDownload() {
    String path = server->arg("f");
    if (path.isEmpty()) {
        server->send(400, "text/plain", "Missing file path");
        return;
    }
    
    // Security: prevent directory traversal
    if (path.indexOf("..") >= 0) {
        server->send(400, "text/plain", "Invalid path");
        return;
    }
    
    File file = SD.open(path);
    if (!file || file.isDirectory()) {
        server->send(404, "text/plain", "File not found");
        return;
    }
    
    // Get filename for Content-Disposition
    String filename = path;
    int lastSlash = path.lastIndexOf('/');
    if (lastSlash >= 0) {
        filename = path.substring(lastSlash + 1);
    }
    
    // Determine content type
    String contentType = "application/octet-stream";
    if (path.endsWith(".txt")) contentType = "text/plain";
    else if (path.endsWith(".csv")) contentType = "text/csv";
    else if (path.endsWith(".json")) contentType = "application/json";
    else if (path.endsWith(".pcap")) contentType = "application/vnd.tcpdump.pcap";
    
    server->sendHeader("Content-Disposition", "attachment; filename=\"" + filename + "\"");
    server->streamFile(file, contentType);
    file.close();
}

// File upload state
static File uploadFile;
static String uploadDir;

void FileServer::handleUpload() {
    server->send(200, "text/plain", "OK");
}

void FileServer::handleUploadProcess() {
    HTTPUpload& upload = server->upload();
    
    if (upload.status == UPLOAD_FILE_START) {
        uploadDir = server->arg("dir");
        if (uploadDir.isEmpty()) uploadDir = "/";
        if (!uploadDir.endsWith("/")) uploadDir += "/";
        
        // Security: prevent directory traversal
        String filename = upload.filename;
        if (filename.indexOf("..") >= 0 || uploadDir.indexOf("..") >= 0) {
            Serial.println("[FILESERVER] Path traversal attempt blocked");
            return;
        }
        
        String path = uploadDir + filename;
        Serial.printf("[FILESERVER] Upload start: %s\n", path.c_str());
        
        uploadFile = SD.open(path, FILE_WRITE);
        if (!uploadFile) {
            Serial.println("[FILESERVER] Failed to open file for writing");
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (uploadFile) {
            uploadFile.write(upload.buf, upload.currentSize);
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (uploadFile) {
            uploadFile.close();
            Serial.printf("[FILESERVER] Upload complete: %u bytes\n", upload.totalSize);
        }
    }
}

void FileServer::handleDelete() {
    String path = server->arg("f");
    if (path.isEmpty()) {
        server->send(400, "text/plain", "Missing file path");
        return;
    }
    
    // Security: prevent directory traversal
    if (path.indexOf("..") >= 0) {
        server->send(400, "text/plain", "Invalid path");
        return;
    }
    
    if (SD.remove(path)) {
        server->send(200, "text/plain", "Deleted");
        Serial.printf("[FILESERVER] Deleted: %s\n", path.c_str());
    } else {
        server->send(500, "text/plain", "Delete failed");
    }
}

void FileServer::handleNotFound() {
    server->send(404, "text/plain", "Not found");
}

const char* FileServer::getHTML() {
    return HTML_TEMPLATE;
}
