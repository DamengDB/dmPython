.PHONY: go-bridge wheel wheel-delocated universal2 clean test-install dpi-headers-secret

# Go bridge library
GO_BRIDGE_DIR = dpi_bridge
DYLIB = $(GO_BRIDGE_DIR)/libdmdpi.dylib

# Detect architecture
ARCH := $(shell uname -m)

go-bridge: $(DYLIB)

$(DYLIB): $(wildcard $(GO_BRIDGE_DIR)/*.go) $(GO_BRIDGE_DIR)/go.mod
	cd $(GO_BRIDGE_DIR) && go build -buildmode=c-shared -o libdmdpi.dylib .
	install_name_tool -id @rpath/libdmdpi.dylib $(DYLIB)

# Build wheel (triggers Go build automatically via setup.py)
wheel:
	python -m build --wheel

# Build + delocate for production wheel
wheel-delocated: wheel
	mkdir -p dist_fixed
	DYLD_LIBRARY_PATH=$(GO_BRIDGE_DIR) delocate-wheel -w dist_fixed dist/*.whl -v

# Build universal2 wheel (ARM64 + x86_64)
universal2:
	# Build ARM64 Go bridge
	cd $(GO_BRIDGE_DIR) && CGO_ENABLED=1 GOARCH=arm64 go build -buildmode=c-shared -o libdmdpi_arm64.dylib .
	# Build x86_64 Go bridge
	cd $(GO_BRIDGE_DIR) && CGO_ENABLED=1 GOARCH=amd64 go build -buildmode=c-shared -o libdmdpi_amd64.dylib .
	# Create universal binary
	lipo -create $(GO_BRIDGE_DIR)/libdmdpi_arm64.dylib $(GO_BRIDGE_DIR)/libdmdpi_amd64.dylib -output $(DYLIB)
	install_name_tool -id @rpath/libdmdpi.dylib $(DYLIB)
	# Build wheel with pre-built library
	DMPYTHON_SKIP_GO_BUILD=1 python -m build --wheel
	mkdir -p dist_fixed
	DYLD_LIBRARY_PATH=$(GO_BRIDGE_DIR) delocate-wheel -w dist_fixed dist/*.whl -v
	# Clean up arch-specific dylibs
	rm -f $(GO_BRIDGE_DIR)/libdmdpi_arm64.dylib $(GO_BRIDGE_DIR)/libdmdpi_amd64.dylib

# Test install in a temporary venv
test-install:
	@echo "Creating temporary venv..."
	python -m venv /tmp/dmpython_test_venv
	/tmp/dmpython_test_venv/bin/pip install dist_fixed/*.whl
	/tmp/dmpython_test_venv/bin/python -c "import dmPython; print('dmPython version:', dmPython.version); print('OK')"
	@echo "Test passed! Cleaning up..."
	rm -rf /tmp/dmpython_test_venv

# Package DPI headers as base64 for GitHub Secret
dpi-headers-secret:
	@tar czf - -C dpi_include . | base64 | pbcopy
	@echo "Base64 copied to clipboard. Paste as GitHub secret DPI_HEADERS_TAR_B64"

# Clean build artifacts
clean:
	rm -rf build/ dist/ dist_fixed/ *.egg-info
	rm -f $(GO_BRIDGE_DIR)/libdmdpi.dylib $(GO_BRIDGE_DIR)/libdmdpi.h
	rm -f $(GO_BRIDGE_DIR)/libdmdpi_arm64.dylib $(GO_BRIDGE_DIR)/libdmdpi_amd64.dylib
	rm -f dmPython*.so
