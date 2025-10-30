.PHONY: conf build clean bind

conf:
	cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH="${CONDA_PREFIX}" -DCMAKE_EXPORT_COMPILE_COMMANDS=TRUE

build:
	cmake --build build -j 8 -t install

clean:
	rm -rf build phnx/_core