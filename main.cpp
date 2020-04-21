#include <cstdio>
#include <vector>
#include <csignal>
#include <string>

#include <unistd.h>
#include <pthread.h>
#include <aio.h>
#include <fcntl.h>
#include <cerrno>

typedef struct aiocb aiocb_t;

struct req_t {
  int index;
  int status;
  char *buffer;
  aiocb_t *op;
};

static std::string gBuffer;

void asyncReady(int, siginfo_t *info, void *) {
  printf("info: %p\n", info);
  printf("code: %d\n", info->si_code);
  printf("is async: %d\n", info->si_code == SI_ASYNCIO);
  printf("status: %d\n", info->si_status);
  printf("signo: %d\n", info->si_signo);
  printf("errno: %d\n", info->si_errno);
  printf("value: %d\n", info->si_value);
  printf("value ptr: %p\n", info->si_value.sival_ptr);
}

void asyncFunction(sigval val) {
  printf("async function ptr: %p\n", val.sival_ptr);
}

req_t * asyncRead(const char *filename, size_t length, int sigNo) {
  auto *op = (aiocb_t*) calloc(1, sizeof(aiocb_t));
  auto *req = (req_t*) calloc(1, sizeof(req_t));

  req->buffer = (char*) calloc(length, sizeof(char));

  op->aio_fildes = open(filename, O_RDONLY);
  op->aio_offset = 0;
  op->aio_nbytes = length;
  op->aio_buf = req->buffer;
  op->aio_lio_opcode = LIO_READ;

  op->aio_sigevent.sigev_notify = SIGEV_SIGNAL;
  op->aio_sigevent.sigev_signo = sigNo;
  op->aio_sigevent.sigev_value.sival_int = 1223;
  op->aio_sigevent.sigev_notify_function = &asyncFunction;

  req->op = op;
  req->status = EINPROGRESS;

  if (aio_read(op) == -1)
    exit(1123);
  return req;
}

int main() {
  struct sigaction sa { };
  sa.sa_flags = SA_SIGINFO;
  sa.sa_sigaction = &asyncReady;
  if (sigaction(SIGUSR1, &sa, nullptr) == -1)
    printf("failed sigaction\n");
  asyncRead("/dev/stdin", 4, SIGUSR1);
  while (true) getchar();
}

