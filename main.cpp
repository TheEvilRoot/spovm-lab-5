#include <cstdio>
#include <vector>
#include <csignal>
#include <string>
#include <iostream>

#include <aio.h>
#include <cerrno>
#include <fcntl.h>
#include <new>
#include <pthread.h>
#include <unistd.h>

typedef struct aiocb aiocb_t;

struct buffer_t {
  size_t delta_;
  std::vector<std::string> data;

  explicit buffer_t(size_t delta): delta_{ delta } { }
  ~buffer_t() = default;

  void put(size_t offset, char *buffer, size_t buffer_length) {
    size_t index = offset / delta_;
    std::string str_data(buffer);

    if (index >= data.size())
      data.resize(index + 1);
    data[index] = str_data;
  }

  [[nodiscard]] std::string get_string() const {
    std::string ret;
    for (const auto & s : data)
      ret += s;
    return ret;
  }
};

struct req_t {
  char *buffer;
  buffer_t *global_buffer;
  aiocb *op;
  pthread_mutex_t *work_mutex;
  pthread_mutex_t *done_mutex;

  ~req_t() {
    free(buffer);
    free(op);
  }
};

template<typename A>
auto create_struct() {
  auto *mem = static_cast<A *>(calloc(1, sizeof(A)));
  return new (mem) A;
}

req_t *read_file_async(int fd, int sig_no, int length, int offset, buffer_t *buffer, pthread_mutex_t *done_mutex, pthread_mutex_t *work_mutex) {
  auto *op = create_struct<aiocb_t>();
  auto *req = create_struct<req_t>();

  req->buffer = static_cast<char*>(calloc(length + 1, sizeof(char)));
  req->done_mutex = done_mutex;
  req->work_mutex = work_mutex;

  op->aio_fildes = fd;
  op->aio_offset = offset;
  op->aio_nbytes = length;
  op->aio_buf = req->buffer;
  op->aio_lio_opcode = LIO_READ;

  op->aio_sigevent.sigev_notify = SIGEV_SIGNAL;
  op->aio_sigevent.sigev_signo = sig_no;
  op->aio_sigevent.sigev_value.sival_ptr = req;

  req->op = op;
  req->global_buffer = buffer;

  if (aio_read(op) == -1){
    delete req;
    return nullptr;
  }

  return req;
}

void async_handler(int, siginfo_t *info, void*) {
  if (info->si_code == SI_ASYNCIO) {
    if (info->si_value.sival_ptr == nullptr) {
      printf("[S] sigval_ptr is nullptr\n");
      return;
    }
    auto *req = static_cast<req_t *>(info->si_value.sival_ptr);
    auto ret_status = aio_return(req->op);
    auto n_bytes = req->op->aio_nbytes;
    auto offset = req->op->aio_offset;

    req->global_buffer->put(offset, req->buffer, n_bytes);

    if (ret_status < n_bytes) {
      printf("ret_status < n_bytes | %ld < %lu\n", ret_status, n_bytes);
      printf("unlocking done mutex\n");
      pthread_mutex_unlock(req->done_mutex);
    }
    pthread_mutex_unlock(req->work_mutex);
  }
}

buffer_t read_file(const char *filename) {
  auto length = 4u;
  buffer_t buffer(length);

  struct sigaction sa { };
  sa.sa_flags = SA_SIGINFO;
  sa.sa_sigaction = &async_handler;

  if (sigaction(SIGUSR1, &sa, nullptr) == -1) {
    printf("failed sigaction\n");
    return buffer;
  }

  pthread_mutex_t work_mutex{ };
  if (pthread_mutex_init(&work_mutex, nullptr) != 0) {
    printf("failed work mutex init\n");
    return buffer;
  }
  pthread_mutex_t done_mutex{ };
  if (pthread_mutex_init(&done_mutex, nullptr) != 0) {
    printf("failed done mutex\n");
    pthread_mutex_destroy(&work_mutex);
    return buffer;
  }

  if (auto fd = open(filename, O_RDONLY); fd != -1) {
    auto sig_no = SIGUSR1;
    auto off = 0u;
    std::vector<req_t *> requests;

    pthread_mutex_lock(&done_mutex);
    while (true) {
      pthread_mutex_lock(&work_mutex);
      if (pthread_mutex_trylock(&done_mutex) != EBUSY)
        break;
      auto req = read_file_async(fd, sig_no, length, off, &buffer, &done_mutex, &work_mutex);
      requests.push_back(req);
      off += length;
    }

    for (auto *req : requests)
      free(req);
    close(fd);

  }

  pthread_mutex_unlock(&work_mutex);
  pthread_mutex_unlock(&done_mutex);

  pthread_mutex_destroy(&work_mutex);
  pthread_mutex_destroy(&done_mutex);
  return buffer;
}

int main() {
  auto buffer = read_file("/home/theevilroot/file");
  std::cout << "'" << buffer.get_string() << "'\n";
  return 0;
}

