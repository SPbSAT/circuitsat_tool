all:
	# zip for the paper shold include:
	# - main artifacts, including source code
	# - README:
	#   - link to an artifact
	#   - additional requirements
	#   - usage instruction
	#   - usage on results reproduction
	# - LICENSE
	mkdir -p build/simplify/simplify/
	# Copy main files to build an artifact
	rsync -rv --exclude='*.o' --exclude='*.*.d' app/ build/simplify/simplify/app
	rsync -rv --exclude='*.o' --exclude='*.*.d' benchmark/ build/simplify/simplify/benchmark
	rsync -rv --exclude='*.o' --exclude='*.*.d' databases/ build/simplify/simplify/databases
	rsync -rv --exclude='*.o' --exclude='*.*.d' src/ build/simplify/simplify/src
	rsync -rv --exclude='*.o' --exclude='*.*.d' tests/ build/simplify/simplify/tests
	rsync -rv --exclude='*.o' --exclude='*.*.d' third_party/ build/simplify/simplify/third_party
	rsync -rv --exclude='*.pyc' --exclude='*.pyo' --exclude='__pycache__' --exclude='.env' tools/ build/simplify/simplify/tools
	cp .clang-format build/simplify/simplify/.clang-format
	cp .clang-tidy build/simplify/simplify/.clang-tidy
	cp CMakeLists.txt build/simplify/simplify/CMakeLists.txt
	cp LICENSE build/simplify/simplify/LICENSE
	cp README.md build/simplify/simplify/README.md

	cd build/simplify/; zip -r simplify.zip simplify

.PHONY: rebuild
rebuild:
	$(MAKE) clean
	$(MAKE) all

clean:
	rm -rf build/simplify/simplify/
	rm -f build/simplify/simplify.zip
