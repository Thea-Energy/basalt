.PHONY: conf build

conf:
	cmake -S . -B build -DCMAKE_PREFIX_PATH="${CONDA_PREFIX}" -DCMAKE_EXPORT_COMPILE_COMMANDS=TRUE

build:
	cmake --build build -j 8

clean:
	rm -rf build