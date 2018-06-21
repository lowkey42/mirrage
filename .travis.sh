#!/bin/sh
#
# Build script for travis-ci.org builds to handle compiles and static
# analysis when ANALYZE=true.
#

if [ $ANALYZE = "true" ]; then
    if [ "$CC" = "clang" ]; then
        docker exec build scan-build --use-c++=clang++ --use-cc=clang cmake -DMIRRAGE_FORCE_LIBCPP=ON -DMIRRAGE_ENABLE_COTIRE=OFF -G "Unix Makefiles" -H/repo -B/build
        docker exec build scan-build --use-c++=clang++ --use-cc=clang -enable-checker deadcode.DeadStores \
          -enable-checker security.insecureAPI.UncheckedReturn \
          --status-bugs -v \
          cmake --build /build
    fi
else
  docker exec build cmake -DCMAKE_BUILD_TYPE=Release -H/repo -B/build
  docker exec build cmake --build /build
fi

