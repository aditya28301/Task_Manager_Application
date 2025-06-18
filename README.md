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

---

## 🖼️ Screenshot

<p align="center">
  <img src="screenshots/filewatcher_demo.png" alt="Demo Screenshot" width="700"/>
</p>

---

## ✨ Key Features

✅ Recursive directory monitoring  
✅ Filter by file extension (`*.cpp`, `*.h`, etc.)  
✅ Real-time event tracking using `inotify`  
✅ Dual logging: console and file  
✅ Thread-safe logging using `std::mutex`  
✅ Graceful shutdown via signal handling  
✅ Smart pointers and RAII for memory safety  
✅ Clean architecture with `.h`/`.cpp` separation  
✅ Fully documented with **Doxygen-style** comments  

---

## 📁 File Structure

```bash
📦 FileWatcher
├── filewatcher.h        # Class declarations (Logger, FileWatcher)
├── filewatcher.cpp      # Implementation of classes + main()
├── Makefile             # Build system (debug/release/install)
├── README.md            # This file
├── screenshots/         # Demo screenshot(s)
└── bin/                 # Compiled binaries (after build)
