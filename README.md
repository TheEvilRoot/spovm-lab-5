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
cmake --build ./cmake-build --target evilasyncio
```

and program binary

```bash
cmake --build ./cmake-build --target handler
```

`handler` binary and `libevilasyncio.so` shared object will be stored in `build/` directory.

