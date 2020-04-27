#pragma once
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>

extern "C" __declspec(dllexport) char* readChunk(HANDLE file, DWORD chunkSize, DWORD offset);

extern "C" __declspec(dllexport) void appendChunk(HANDLE file, const char *buffer, size_t bufferSize);