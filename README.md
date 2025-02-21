# nz-adbc
## Steps
```
git clone git@github.com:IBM/nz-adbc.git
cd nz-adbc
git submodule update --init --recursive
make

If you wish to build cpp tests :

-> Before running the make command update the Makefile rule run_cmake_adbc and change it to:

cmake ../c -DCMAKE_BUILD_TYPE=Debug -D -DADBC_BUILD_TESTS=ON 

-> Update the file arrow-adbc/c/symbols.map and add NetezzaDriverInit

-> After running the make command update the arrow-adbc/c/CMakeLists.txt file and add the following code below the postgresql check:

if(ADBC_DRIVER_NETEZZA)
  add_subdirectory(driver/netezza)
endif()

```
