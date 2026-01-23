#!/bin/bash

# Blackheart Plugin Verification Script
# Checks plugin installation and basic validation

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║           BLACKHEART PLUGIN VERIFICATION                     ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

PASS_COUNT=0
FAIL_COUNT=0

check_pass() {
    echo -e "${GREEN}[PASS]${NC} $1"
    ((PASS_COUNT++))
}

check_fail() {
    echo -e "${RED}[FAIL]${NC} $1"
    ((FAIL_COUNT++))
}

check_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

# 1. Check VST3 plugin exists
echo "=== Checking Plugin Installation ==="

VST3_PATH="/Users/$USER/Library/Audio/Plug-Ins/VST3/Blackheart.vst3"
if [ -d "$VST3_PATH" ]; then
    check_pass "VST3 plugin installed at $VST3_PATH"
else
    check_fail "VST3 plugin not found at $VST3_PATH"
fi

# 2. Check AU plugin exists
AU_PATH="/Users/$USER/Library/Audio/Plug-Ins/Components/Blackheart.component"
if [ -d "$AU_PATH" ]; then
    check_pass "AU plugin installed"
else
    check_warn "AU plugin not found (may not be installed)"
fi

# 3. Check Standalone app exists
BUILD_PATH="/Users/overlord/Documents/Dev/Blackheart/Builds/MacOSX/build/Debug/Blackheart.app"
if [ -d "$BUILD_PATH" ]; then
    check_pass "Standalone app built at $BUILD_PATH"
else
    check_fail "Standalone app not found"
fi

# 4. Verify VST3 bundle structure
echo ""
echo "=== Checking VST3 Bundle Structure ==="

if [ -d "$VST3_PATH" ]; then
    if [ -f "$VST3_PATH/Contents/MacOS/Blackheart" ]; then
        check_pass "VST3 binary exists"
    else
        check_fail "VST3 binary missing"
    fi

    if [ -f "$VST3_PATH/Contents/Info.plist" ]; then
        check_pass "VST3 Info.plist exists"
    else
        check_fail "VST3 Info.plist missing"
    fi

    if [ -f "$VST3_PATH/Contents/Resources/moduleinfo.json" ]; then
        check_pass "VST3 moduleinfo.json exists"
    else
        check_warn "VST3 moduleinfo.json missing (optional)"
    fi
fi

# 5. Check binary architecture
echo ""
echo "=== Checking Binary Architecture ==="

if [ -f "$VST3_PATH/Contents/MacOS/Blackheart" ]; then
    ARCH=$(file "$VST3_PATH/Contents/MacOS/Blackheart")
    if [[ "$ARCH" == *"arm64"* ]]; then
        check_pass "Binary is ARM64 (Apple Silicon native)"
    elif [[ "$ARCH" == *"x86_64"* ]]; then
        check_pass "Binary is x86_64 (Intel)"
    else
        check_warn "Unknown architecture: $ARCH"
    fi
fi

# 6. Check code signing
echo ""
echo "=== Checking Code Signing ==="

if [ -d "$VST3_PATH" ]; then
    CODESIGN_RESULT=$(codesign -v "$VST3_PATH" 2>&1)
    if [ $? -eq 0 ]; then
        check_pass "VST3 code signature valid"
    else
        check_warn "VST3 not properly signed (may require signing for distribution)"
    fi
fi

# 7. Validate plugin with pluginval (if available)
echo ""
echo "=== Plugin Validation ==="

if command -v pluginval &> /dev/null; then
    echo "Running pluginval..."
    pluginval --validate "$VST3_PATH" --strictness-level 5 2>&1 | head -20
else
    check_warn "pluginval not installed (install from https://github.com/Tracktion/pluginval)"
fi

# 8. Check for common issues in source
echo ""
echo "=== Source Code Analysis ==="

SOURCE_DIR="/Users/overlord/Documents/Dev/Blackheart/Source"

# Check for potential memory leaks (raw new without smart pointers)
RAW_NEW_COUNT=$(grep -r "new " "$SOURCE_DIR" --include="*.cpp" --include="*.h" | grep -v "make_unique\|make_shared\|std::unique_ptr\|std::shared_ptr\|JUCE" | wc -l)
if [ "$RAW_NEW_COUNT" -lt 5 ]; then
    check_pass "Minimal raw 'new' usage (good memory management)"
else
    check_warn "Found $RAW_NEW_COUNT potential raw 'new' allocations"
fi

# Check for audio thread allocations
AUDIO_ALLOC=$(grep -r "processBlock" -A 50 "$SOURCE_DIR/PluginProcessor.cpp" | grep -E "new |malloc|vector\(" | wc -l)
if [ "$AUDIO_ALLOC" -eq 0 ]; then
    check_pass "No obvious allocations in processBlock"
else
    check_warn "Found $AUDIO_ALLOC potential allocations in processBlock"
fi

# Summary
echo ""
echo "╔══════════════════════════════════════════════════════════════╗"
echo "║                      SUMMARY                                 ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo -e "Passed: ${GREEN}$PASS_COUNT${NC}"
echo -e "Failed: ${RED}$FAIL_COUNT${NC}"
echo ""

if [ $FAIL_COUNT -eq 0 ]; then
    echo -e "${GREEN}All critical checks passed!${NC}"
    exit 0
else
    echo -e "${RED}Some checks failed. Please review above.${NC}"
    exit 1
fi
