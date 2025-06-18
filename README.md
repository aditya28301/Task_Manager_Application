<h1 align="center">🛡️ FileWatcher</h1>
<p align="center">
  <b>A High-Performance, Multi-Threaded File Monitoring System for Linux</b><br>
  <i>Built with Modern C++, inotify, and Real-Time Logging</i>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/C%2B%2B-17-blue?logo=c%2B%2B" alt="C++17" />
  <img src="https://img.shields.io/badge/Platform-Linux-informational?logo=linux" alt="Linux" />
  <img src="https://img.shields.io/badge/Thread-Safe-Yes-success" alt="Thread Safe" />
  <img src="https://img.shields.io/badge/License-MIT-green.svg" alt="License: MIT" />
</p>

<hr>

<h2>🖼️ Screenshot</h2>
<p align="center">
  <img src="screenshots/file_watcher.png" alt="File Watcher Screenshot" width="700"/>
  <img src="screenshots/command_history.png" alt="Command History Screenshot" width="700"/>
</p>

<hr>

<h2>✨ Key Features</h2>

<ul>
  <li>✅ Recursive directory monitoring</li>
  <li>✅ Filter by file extension (<code>*.cpp</code>, <code>*.h</code>, etc.)</li>
  <li>✅ Real-time event tracking using <code>inotify</code></li>
  <li>✅ Dual logging: console and file</li>
  <li>✅ Thread-safe logging using <code>std::mutex</code></li>
  <li>✅ Graceful shutdown via signal handling</li>
  <li>✅ Smart pointers and RAII for memory safety</li>
  <li>✅ Clean architecture with <code>.h</code>/<code>.cpp</code> separation</li>
  <li>✅ Fully documented with <b>Doxygen-style</b> comments</li>
</ul>

<hr>

<h2>📁 File Structure</h2>

<pre>
📦 FileWatcher
├── filewatcher.h        # Class declarations (Logger, FileWatcher)
├── filewatcher.cpp      # Implementation of classes + main()
├── Makefile             # Build system (debug/release/install)
├── README.md            # This file
├── screenshots/         # Demo screenshot(s)
└── bin/                 # Compiled binaries (after build)
</pre>

<hr>

<h2>🔧 Build Instructions</h2>

<h3>🛠️ Step 1: Install Required Tools</h3>

```bash
sudo apt update
sudo apt install build-essential g++ make
