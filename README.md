# nz-adbc
## Steps to build
```bash
git clone git@github.com:IBM/nz-adbc.git
cd nz-adbc
git submodule update --init --recursive
```

If you wish to build the test cases for cpp run
```bash
make
```
If you wish to build the project without building test cases run
```bash
make compile_without_tests
```

## Building C++ Tests (Optional)

1. **Update the symbols.map file:**
    Modify the `arrow-adbc/c/symbols.map` file and add `NetezzaDriverInit` to the file.
2. **Update the CMakeLists.txt file:**
    Before running the make command, update the `arrow-adbc/c/CMakeLists.txt` file. Add the following block of code below the PostgreSQL check:
    ```cmake
    if(ADBC_DRIVER_NETEZZA)
      add_subdirectory(driver/netezza)
    endif()
    ```
