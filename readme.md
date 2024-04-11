# Configuring and building

## Release

```bash
mkdir -p release
rm -rf release/*
cd release
cmake ../ -DCMAKE_BUILD_TYPE=Release
cmake --build . --verbose -j$(nproc)
```

## Debug

```bash
mkdir -p debug
rm -rf debug/*
cd debug
cmake ../ -DCMAKE_BUILD_TYPE=Debug
cmake --build . --verbose -j$(nproc)
```

# Running

```bash
$ ./task <base> <order>
```

To meet the requirements of the task

```bash
$ ./task 13 13
```
