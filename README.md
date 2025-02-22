# nz-adbc
## Steps to build
```bash
git clone git@github.com:IBM/nz-adbc.git
cd nz-adbc
git submodule update --init --recursive
make
```
## Building C++ Tests (Optional)

1. **Update the Makefile:**
    Before running the make command, modify the Makefile rule `run_cmake_adbc`.
    Update it as follows:
    ```cmake
    cmake ../c -DCMAKE_BUILD_TYPE=Debug -DADBC_BUILD_TESTS=ON
    ```
2. **Update the symbols.map file:**
    Modify the `arrow-adbc/c/symbols.map` file and add `NetezzaDriverInit` to the file.
3. **Update the CMakeLists.txt file:**
    Before running the make command, update the `arrow-adbc/c/CMakeLists.txt` file. Add the following block of code below the PostgreSQL check:
    ```cmake
    if(ADBC_DRIVER_NETEZZA)
      add_subdirectory(driver/netezza)
    endif()
    ```
