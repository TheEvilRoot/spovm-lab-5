#include <cstdio>
#include <vector>
#include <csignal>
#include <string>

#include <unistd.h>
#include <pthread.h>
#include <aio.h>
#include <fcntl.h>
#include <cerrno>
#include <memory>

typedef struct aiocb aiocb_t;

struct req_t {
  std::size_t index;
  int status;
  char *buffer;
  aiocb *op;
  pthread_mutex_t *mutex;
};

void asyncReady(int, siginfo_t *info, void *) {
  if (info->si_code == SI_ASYNCIO) {
    if (info->si_value.sival_ptr == nullptr) {
      printf("[T] sigval_ptr is nullptr\n");
      return;
    }
    auto req = static_cast<req_t *>(info->si_value.sival_ptr);
    printf("nbytes: %zu\n", req->op->aio_nbytes);
    printf("return: %ld\n", aio_return(req->op));
    for (int i = 0; i < req->op->aio_nbytes; i++)
      printf("%02x ", static_cast<int>(req->buffer[i]));
    printf("\n");
    pthread_mutex_unlock(req->mutex);
  }
}

void asyncFunction(sigval val) {
  printf("async function ptr: %p\n", val.sival_ptr);
}

req_t * asyncRead(const char *filename) {
  pthread_mutex_t mutex { };
  pthread_mutex_init(&mutex, nullptr);

  auto fd = open(filename, O_RDONLY);
  auto signo = SIGUSR1;
  auto length = 4u;
  auto off = 0u;

  while (true) {
    auto *op = (aiocb_t *) calloc(1, sizeof(aiocb_t));
    auto *req = (req_t *) calloc(1, sizeof(req_t));

    req->mutex = &mutex;
    req->buffer = (char *) calloc(length, sizeof(char));

    op->aio_fildes = fd;
    op->aio_offset = off;
    op->aio_nbytes = length;
    op->aio_buf = req->buffer;
    op->aio_lio_opcode = LIO_READ;

    op->aio_sigevent.sigev_notify = SIGEV_SIGNAL;
    op->aio_sigevent.sigev_signo = signo;
    op->aio_sigevent.sigev_value.sival_ptr = req;

    req->op = op;
    req->status = EINPROGRESS;

    pthread_mutex_lock(&mutex);
    if (aio_read(op) == -1) {
      pthread_mutex_unlock(&mutex);
      exit(1123);
    }
  }

  return req;
}

int main() {
  pthread_mutex_init(&mutex, nullptr);
  struct sigaction sa { };
  sa.sa_flags = SA_SIGINFO;
  sa.sa_sigaction = &asyncReady;
  if (sigaction(SIGUSR1, &sa, nullptr) == -1)
    printf("failed sigaction\n");
  auto file = open("/home/theevilroot/file", O_RDONLY);
  auto length = 6u;
  for (int i = 0; i < 8; i++) {
    asyncRead(file, i * length, length, SIGUSR1);
  }
  while (true) usleep(10000000);
}

