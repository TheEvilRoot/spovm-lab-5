//
// Created by theevilroot on 28.04.2020.
//
#include <cstdio>
#include <vector>
#include <string>

#include <cerrno>
#include <dirent.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

typedef char*(*chunk_read_t)(int, size_t, size_t, pthread_mutex_t*);
typedef void(*chunk_append_t)(int, char*, size_t, pthread_mutex_t*);
typedef void* shared_library_t;

struct payload_t {
  std::string directory_path;
  std::string output_file_path;
};

shared_library_t async_io;
chunk_append_t append_chunk;
chunk_read_t read_chunk;

pthread_mutex_t reader_work_mutex;
pthread_mutex_t writer_work_mutex;

pthread_mutex_t writer_ready_mutex;
pthread_mutex_t writer_death_mutex;

pthread_mutex_t buffer_mutex;
pthread_mutex_t need_writer_mutex;
pthread_mutex_t writer_working;

sem_t done_semaphore;

std::string global_buffer;

void rlog(const char *s) {
  fprintf(stderr, "reader:\t%s\n", s);
}

void wlog(const char *s) {
  fprintf(stderr, "writer:\t%s\n", s);
}

template<typename Function>
auto get_function(shared_library_t lib, const char *name) {
  if (auto func = dlsym(lib, name); func != nullptr) {
    if (auto f_func = reinterpret_cast<Function>(func); f_func != nullptr) {
      return f_func;
    }
  }
  printf("failed to get symbol %s from shared library\n", name);
  exit(4);
}

void load_library() {
  if ((async_io = dlopen("./libevilasyncio.so", RTLD_LAZY)) == nullptr) {
    printf("failed to load libevilasyncio.so\n");
    exit(2);
  }
  append_chunk = get_function<chunk_append_t>(async_io, "append_chunk");
  read_chunk = get_function<chunk_read_t>(async_io, "read_chunk");
}

auto get_args(const int argc, const char * argv_s[]) {
  std::vector<std::string> args;
  for (int i = 1; i < argc; i++)
    args.emplace_back(argv_s[i]);

  if (args.size() < 2) {
    printf("Invalid arguments\n");
    printf("Use %s <directory> <output_file_path>\n", argv_s[0]);
    exit(1);
  }

  return std::make_pair(args[0], args[1]);
}

auto get_files(const std::string &path) {
  std::string base_path(path);
  if (base_path.back() != '/')
    base_path += '/';

  std::vector<std::string> files;
  if (auto *dir = opendir(path.c_str()); dir != nullptr) {
    dirent *entry;
    while((entry = readdir(dir)) != nullptr) {
      if (entry->d_type == DT_REG)
        files.push_back(base_path + std::string(entry->d_name));
    }
    free(dir);
  }
  return files;
}

void *reader_thread_handler(void *args) {
  auto *payload = static_cast<payload_t *>(args);
  if (auto files = get_files(payload->directory_path); !files.empty()) {
    for (const auto &file_name : files) {
      auto name = file_name.c_str();
      auto file = open(name, O_RDONLY);
      if (file < 0) {
        printf("Failed to open file %s... %d", name, errno);
        continue;
      }
      fprintf(stderr, "%s\n", name);

      size_t chunk_size = 16;
      size_t offset = 0;

      do {
        pthread_mutex_lock(&writer_working);
        pthread_mutex_unlock(&writer_working);
				auto chunk_str = read_chunk(file, chunk_size, offset, &reader_work_mutex);
        auto chunk = std::string(chunk_str);
				delete[] chunk_str;
        pthread_mutex_lock(&buffer_mutex);
        global_buffer = std::string(chunk);
        pthread_mutex_unlock(&buffer_mutex);
        pthread_mutex_unlock(&need_writer_mutex);
        if (chunk.size() < chunk_size)
          break;
        offset += chunk_size;
      } while (true);
      close(file);
    }
  } else {
    printf("Directory is empty or does not exists\n");
  }
  sem_post(&done_semaphore);
  pthread_mutex_unlock(&writer_death_mutex);
  pthread_mutex_unlock(&need_writer_mutex);
  return nullptr;
}

void *writer_thread_handler(void *args) {
  auto *payload = static_cast<payload_t *>(args);
  if (auto file = open(payload->output_file_path.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0775); file >= 0) {
		while (true) {
			pthread_mutex_lock(&need_writer_mutex);
      pthread_mutex_lock(&writer_working);
      if (pthread_mutex_trylock(&writer_death_mutex) == 0)
        break;
      pthread_mutex_lock(&buffer_mutex);
			auto buffer = std::string(global_buffer);
			append_chunk(file, buffer.data(), buffer.size(), &writer_work_mutex);
			pthread_mutex_unlock(&buffer_mutex);
      pthread_mutex_unlock(&writer_working);
		}
		close(file);
		printf("writer done\n");
	} else {
		printf("failed to open output file\n");
	}
  sem_post(&done_semaphore);
  return nullptr;
}

void create_mutex(pthread_mutex_t *mutex, bool lock = false) {
  pthread_mutex_init(mutex, nullptr);
  if (lock)
    pthread_mutex_lock(mutex);
}

void rewrite_if_exists(const std::string &file) {
  if (auto fd = open(file.c_str(), O_WRONLY | O_TRUNC, 0775); fd >= 0) {
    close(fd);
  } else {
    fprintf(stderr, "rewrite: file does not exists\n");
  }
}

int main(const int argc, const char* argv_s[]) {
  auto [directory_path, output_file_path] = get_args(argc, argv_s);
  load_library();
  create_mutex(&writer_working);
  create_mutex(&writer_work_mutex, true);
  create_mutex(&reader_work_mutex, true);
  create_mutex(&writer_ready_mutex, true);
  create_mutex(&writer_death_mutex, true);
  create_mutex(&buffer_mutex);
  create_mutex(&need_writer_mutex, true);
  sem_init(&done_semaphore, 0, 0);

  rewrite_if_exists(output_file_path);

  auto *payload = new payload_t { directory_path, output_file_path };
  pthread_t reader_thread{};
  pthread_t writer_thread{};

  pthread_create(&reader_thread, nullptr, reader_thread_handler, payload);
  pthread_create(&writer_thread, nullptr, writer_thread_handler, payload);

  sem_wait(&done_semaphore);
  sem_wait(&done_semaphore);

  pthread_mutex_destroy(&writer_work_mutex);
  pthread_mutex_destroy(&reader_work_mutex);
  pthread_mutex_destroy(&writer_ready_mutex);
  pthread_mutex_destroy(&writer_death_mutex);
  pthread_mutex_destroy(&buffer_mutex);
  pthread_mutex_destroy(&need_writer_mutex);
  pthread_mutex_destroy(&writer_working);
  sem_destroy(&done_semaphore);

	delete payload;

  return 0;
}
