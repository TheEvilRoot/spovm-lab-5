СПОВМ ЛР4 (Linux)
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

and build binaries

```bash
cmake --build ./cmake-build --target all -j 2
```

`0004` binary will be stored in `build/` directory.

