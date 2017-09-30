#!/bin/sh
#
# Build script for travis-ci.org builds to handle compiles and static
# analysis when ANALYZE=true.
#

if [ $ANALYZE = "true" ]; then
    if [ "$CC" = "clang" ]; then
        docker exec build scan-build cmake -G "Unix Makefiles" -H/repo -B/build
        docker exec build scan-build cmake -G "Unix Makefiles" -H/repo -B/build
        docker exec build scan-build -enable-checker deadcode.DeadStores \
          -enable-checker security.insecureAPI.UncheckedReturn \
          --status-bugs -v \
          cmake --build /build
    fi
else
  docker exec build cmake -DSF2_BUILD_TESTS=ON -H/repo -B/build
  docker exec build cmake -DSF2_BUILD_TESTS=ON -H/repo -B/build
  docker exec build cmake --build /build
fi

