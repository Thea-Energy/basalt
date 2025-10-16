.PHONY: conf build clean bind

conf:
	cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH="${CONDA_PREFIX}" -DCMAKE_EXPORT_COMPILE_COMMANDS=TRUE

build:
	cmake --build build -j 8

clean:
	rm -rf build

bind:
# # 	find include -name '*.h' | sort | awk '{print "#include \"" $$0 "\""}' > extern/all_includes.hpp
# 	rm -rf bindings/*
# 	./extern/build/source/binder --root-module sms --prefix bindings extern/all_includes.hpp --bind "" --annotate-includes -- -std=c++11 -I./ -Iinclude/gmcore -Iinclude/mscore -Iinclude/pskrnl -DNDEBUG


# 	find includetest -name '*.h' | sort | awk '{print "#include <" $$0 ">"}' > extern/all_includes.hpp
	find includetest -name '*.h' -exec basename {} \; | sort | awk '{print "#include <" $$0 ">"}' > extern/all_includes.hpp
	rm -rf bindings/*
	./extern/build/source/binder --single-file --root-module sms --prefix bindings extern/all_includes.hpp --bind "" --annotate-includes -- -std=c++17 -I./ -Iincludetest -DNDEBUG

bind-comp:
	pybase=`which python3`
	g++ \
	-O3 \
	-I${pybase::-12}/include/python3.11m -I$PWD/../../../build/pybind11/include -I$PWD/../include \
	-I$PWD/../../../source -shared  \
	-std=c++11  -c test_struct.cpp  \
	-o test_struct.o -fPIC