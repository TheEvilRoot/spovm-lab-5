#include <Windows.h>
#include <cstdio>
#include <cstdlib>

#include <string>
#include <vector>
#include <iostream>

struct WriterPayload {
  HANDLE readerThread;
  std::string dumpFilePath;
};

typedef HANDLE Event;
typedef CRITICAL_SECTION Critical;
typedef HMODULE Library;

typedef const char* (*chunkRead_t)(HANDLE, DWORD, DWORD);
typedef void (*chunkAppend_t)(HANDLE, const char *, size_t);

std::string   globalBuffer;
Event         needWriter;
Critical      bufferLock;
Event         writerReady;

Library       theEvilAsyncIo;
chunkRead_t   readChunk;
chunkAppend_t appendChunk;

std::vector<std::string> getFilesInDir(const std::string& path) {
  WIN32_FIND_DATAA fileData{ 0 };

  std::string basePath = std::string(path);
  if (basePath.back() != '/')
    basePath += '/';

  auto file = FindFirstFileA((basePath + "*").c_str(), &fileData);
  if (file == INVALID_HANDLE_VALUE) {
    return  { };
  }

  std::vector <std::string> files;
  do {
    if (!(fileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
      files.push_back(basePath + std::string(fileData.cFileName));
    }
  } while (FindNextFileA(file, &fileData) != 0);

  return files;
}

DWORD getThreadExitCode(HANDLE thread) {
  DWORD ret;
  GetExitCodeThread(thread, &ret);
  return ret;
}

DWORD readerThread(void* args) {
  auto diretoryPath = *static_cast<std::string *>(args);
  auto files = getFilesInDir(diretoryPath);
  if (files.empty()) {
    printf("No files in directory or directory does not exists...\n");
    return 0;
  }
  for (const auto& fileName : files) {
    auto* fileNameStr = fileName.c_str();

    auto file = CreateFileA(
      fileNameStr,
      FILE_GENERIC_READ,
      FILE_SHARE_READ,
      nullptr,
      OPEN_EXISTING, 
      FILE_FLAG_OVERLAPPED, 
      nullptr);
    fprintf(stderr, "%s => %d\n", fileNameStr, GetLastError());

    WaitForSingleObject(writerReady, INFINITE);

    DWORD offset = 0;
    DWORD chunkSize = 128;
    do {
      printf("%04d bytes\r", offset + chunkSize);
      auto chunk = std::string(readChunk(file, chunkSize, offset));
      printf("%04d bytes\r", offset + chunk.size());

      EnterCriticalSection(&bufferLock);
      globalBuffer = std::string(chunk);
      LeaveCriticalSection(&bufferLock);
      SetEvent(needWriter);

      if (chunk.length() < chunkSize)
        break;
      offset += chunkSize;
    } while (true);
    printf("\n");
    CloseHandle(file);
  }
  return 0;
}

DWORD writerThread(void* args) {
  auto payload = static_cast<WriterPayload*>(args);
  auto outFile = CreateFileA(payload->dumpFilePath.c_str(), FILE_APPEND_DATA, FILE_SHARE_WRITE, nullptr,
    CREATE_NEW, FILE_FLAG_OVERLAPPED, nullptr);
  fprintf(stderr, "Dump file on %s : %d\n", payload->dumpFilePath.c_str(), GetLastError());

  SetEvent(writerReady);

  while ((WaitForSingleObject(needWriter, INFINITE) || true) && 
    getThreadExitCode(payload->readerThread) == STILL_ACTIVE) {
    ResetEvent(needWriter);

    EnterCriticalSection(&bufferLock);
    auto buffer = std::string(globalBuffer);
    appendChunk(outFile, buffer.c_str(), buffer.size());
    LeaveCriticalSection(&bufferLock);
  }
  CloseHandle(outFile);
  return 0;
}

template<typename Function>
Function getFunction(const std::string& name) {
  FARPROC func = GetProcAddress(theEvilAsyncIo, name.c_str());
  if (func == nullptr) {
    throw name + " cannot be found in dll\n";
  }
  Function f = reinterpret_cast<Function> (func);
  if (f == nullptr) {
    throw name + " cannot be caster to that\n";
  }

  printf("%s loaded\n", name.c_str());
  return f;
}

void loadFunctions() {
  readChunk = getFunction<chunkRead_t>("readChunk");
  appendChunk = getFunction<chunkAppend_t>("appendChunk");
}

int main(int argc, const char *argv_s[]) {
  std::vector<std::string> argv;
  for (int i = 1; i < argc; i++) {
    argv.push_back(std::string(argv_s[i]));
  }

  if (argv.size() < 2) {
    printf("Invalid arguments\n");
    printf("Use directory path as first argument\n");
    printf("Dump file path as second one\n");
    return 1;
  }

  theEvilAsyncIo = LoadLibraryA("TheEvilAsyncIO.dll");
  if (theEvilAsyncIo == nullptr) {
    printf("Failed to load TheEvilAsyncIO.dll\n");
    return 2;
  }

  try {
    loadFunctions();
  } catch (std::string & what) {
    printf("%s", what.c_str());
    FreeLibrary(theEvilAsyncIo);
    return 2;
  }

  std::string directoryPath = argv[0];
  std::string dumpFilePath = argv[1];

  needWriter = CreateEvent(nullptr, true, false, nullptr);
  printf("Create needWriter Last Error : %d\n", GetLastError());

  InitializeCriticalSection(&bufferLock);
  printf("Create bufferLock Last Error : %d\n", GetLastError());

  writerReady = CreateEventA(nullptr, true, false, nullptr);
  printf("Create writerReady Last Error : %d\n", GetLastError());

  DeleteFileA(dumpFilePath.c_str());

  DWORD threadId;

  auto reader = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE) &readerThread, &directoryPath, 0, &threadId);
  auto * writerPayload = new WriterPayload { reader, dumpFilePath };
  auto writer = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE) &writerThread, writerPayload, 0, &threadId);

  WaitForSingleObject(reader, INFINITE);
  SetEvent(needWriter); // we need writer... to kill himself :D
  WaitForSingleObject(writer, INFINITE); 

  CloseHandle(reader);
  CloseHandle(writer);

  DeleteCriticalSection(&bufferLock);
  CloseHandle(writerReady);
  CloseHandle(needWriter);

  delete writerPayload;

  FreeLibrary(theEvilAsyncIo);

  return 0;
}