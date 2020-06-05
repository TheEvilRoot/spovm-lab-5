СПОВМ ЛР5 (Linux)
----

How to run?
----

First, create cmake build directory

```bash
mkdir cmake-build
```

Next, configure cmake project

```bash
cd cmake-build
cmake ..
```

After cmake succeded, go back to project folder 

```bash
cd ..
```

then build shared library

```bash
cmake --build ./cmake-build --target io 
```

and program binary

```bash
cmake --build ./cmake-build --target handler
```

or just build all targets using


```bash
cmake --build ./cmake-build --target all
```

`handler` binary and `libio.so` shared object will be stored in `build/` directory.


Usage
-----

`handler` executable apply two arguments: 

 - directory with files to concat
 - output file path

Usage example:


```bash
./handler files/ files_dump
```

libio.so
--------

Libio is special shared library that load dynamically into handler program using `dlopen` function. It's has two C functions (no name mangling): 

 - `append_chunk`
 - `read_chunk`

#### read_chunk

This function take file descriptor, chunk size, offset in the file and working mutex as arguments, perform async read (using `aio.h` library from `librt`) and make it blocking using given mutex. Basically, it's blocking read function, but implemented via async read. Return value is null-terminated byte (char) array with given chunk size. Null-byte is set on the end of read result.

#### append_chunk

This function take file descriptor, bytes buffer, buffer size and working mutex, appends given data into file with given offset. After async write is done, working mutex will be unlocked. Does not return anything.


#### Warning

Before both operations mutexes must be unlocked. After operation complete it **will** be unlocked.

Also, not recommended to use common mutex with both operations (such behaviour is not tested).

Handler output
-----

When handler works it print some logs into error file (cuz it's have no buffer, hate buffering!)

Reader thread output marked as `reader` and writer thread as `writer`. When reader thread puts some data into buffer, it's printing hash sum of this data. After that writer must get control of buffer mutex reader should wait for it. Writer also will print buffer hash sum. It must equals for single chunks. Chunks read-write are separated with empty line in output logs. If hash sums are not equals, something went wrong (race cond.?), create an issue in this repo with logs and working files.


When program is done, last thread alive (except main) is writer thread. It will write done message in standart output (stdout). 

