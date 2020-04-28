//
// Created by theevilroot on 28.04.2020.
//

#ifndef SPOVM_LAB_5_EVILASYNCIO_H
#define SPOVM_LAB_5_EVILASYNCIO_H

#include <csignal>

#include <aio.h>
#include <pthread.h>
#include <unistd.h>

extern "C" void append_chunk(int file_descriptor, char *buffer, size_t buffer_size, pthread_mutex_t *writer_work_mutex);

extern "C" char *read_chunk(int file_descriptor, size_t chunk_size, size_t offset, pthread_mutex_t *reader_work_mutex);

#endif//SPOVM_LAB_5_EVILASYNCIO_H
