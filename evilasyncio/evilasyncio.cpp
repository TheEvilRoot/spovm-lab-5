//
// Created by theevilroot on 28.04.2020.
//

#include "evilasyncio.h"

char *read_chunk(int file_descriptor, size_t chunk_size, size_t offset, pthread_mutex_t *reader_work_mutex) {
  auto *op = new aiocb{0};
  auto *buffer = new char[chunk_size + 1];

  op->aio_fildes = file_descriptor;
  op->aio_nbytes = chunk_size;
  op->aio_offset = offset;
  op->aio_lio_opcode = LIO_READ;
  op->aio_buf = buffer;

  op->aio_sigevent.sigev_notify = SIGEV_SIGNAL;
  op->aio_sigevent.sigev_signo = SIGUSR1;
  op->aio_sigevent.sigev_value.sival_ptr = reader_work_mutex;

  struct sigaction action{};
  action.sa_flags = SA_SIGINFO | SA_RESTART;
  action.sa_sigaction = [](int, siginfo_t *info, void*) -> void {
    pthread_mutex_unlock(static_cast<pthread_mutex_t *>(info->si_value.sival_ptr));
  };
  sigaction(SIGUSR1, &action, nullptr);

  aio_read(op);
  pthread_mutex_lock(reader_work_mutex);

  auto actual_size = aio_return(op);
  buffer[actual_size] = 0;

  delete op;
  sigaction(SIGUSR1, nullptr, nullptr);

  return buffer;
}

void append_chunk(int file_descriptor, char *buffer, size_t buffer_size, pthread_mutex_t *writer_work_mutex) {
  auto *op = new aiocb{0};

  op->aio_fildes = file_descriptor;
  op->aio_nbytes = buffer_size;
  op->aio_offset = 0;
  op->aio_buf = buffer;
  op->aio_lio_opcode = LIO_WRITE;

  op->aio_sigevent.sigev_notify = SIGEV_SIGNAL;
  op->aio_sigevent.sigev_signo = SIGUSR2;
  op->aio_sigevent.sigev_value.sival_ptr = writer_work_mutex;

  struct sigaction action{};
  action.sa_flags = SA_SIGINFO | SA_RESTART;
  action.sa_sigaction = [](int, siginfo_t *info, void*) -> void {
    pthread_mutex_unlock(static_cast<pthread_mutex_t*>(info->si_value.sival_ptr));
  };
  sigaction(SIGUSR2, &action, nullptr);

  aio_write(op);
  pthread_mutex_lock(writer_work_mutex);

  delete op;
}