#include "theevilasyncio.h"

__declspec(dllexport) char * readChunk(HANDLE file, DWORD chunkSize, DWORD offset) {
  _OVERLAPPED overlapped{ 0 };
  overlapped.Offset = offset;
  overlapped.OffsetHigh = 0;
  overlapped.hEvent = CreateEvent(nullptr, true, false, nullptr);
  char* localBuffer = (char*) calloc(chunkSize + 1, sizeof(char));
  DWORD currentRead = 0;

  DWORD result = ReadFile(file, localBuffer, chunkSize, &currentRead, &overlapped);
  if (!result && GetLastError() != ERROR_IO_PENDING) {
    printf("Chunk with offset %d is failed with error %d\n", offset, GetLastError());
    return localBuffer;
  }

  DWORD waitResult = WaitForSingleObject(overlapped.hEvent, INFINITE);

  if (waitResult == WAIT_FAILED) {
    printf("Wait result : %d\n", waitResult);
    printf("Failed to wait event with error %d\n", GetLastError());
    return localBuffer;
  }

  CloseHandle(overlapped.hEvent);
  return localBuffer;
}

__declspec(dllexport) void appendChunk(HANDLE file, const char *buffer, size_t bufferSize) {
  _OVERLAPPED overlapped{ 0 };
  overlapped.Offset = 0;
  overlapped.OffsetHigh = 0;
  overlapped.hEvent = CreateEvent(nullptr, true, false, nullptr);
  DWORD currentWrite = 0;

  auto result = WriteFile(file, buffer, bufferSize, &currentWrite, &overlapped);

  if (!result && GetLastError() != ERROR_IO_PENDING) {
    printf("Chunk write failed with error %d\n", GetLastError());
    return;
  }

  auto waitResult = WaitForSingleObject(overlapped.hEvent, INFINITE);

  if (waitResult == WAIT_FAILED) {
    printf("Wait result : %d\n", waitResult);
    printf("Failed to wait event with error %d\n", GetLastError());
    return;
  }

  Sleep(700); // for future bug fixes and perfomance improvments
}