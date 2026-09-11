#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "$0")/.." && pwd)"
test_binary="${TMPDIR:-/tmp}/fengyin-breath-mapper-test"
profile_test_binary="${TMPDIR:-/tmp}/fengyin-device-profile-test"
detector_test_binary="${TMPDIR:-/tmp}/fengyin-controller-detector-test"
swam_test_binary="${TMPDIR:-/tmp}/fengyin-swam-classifier-test"

python3 -m unittest discover -s "$project_dir/tests" -p 'test_*.py' -v
clang++ -std=c++20 -Wall -Wextra -Werror "$project_dir/tests/test_breath_mapper.cpp" -o "$test_binary"
"$test_binary"
clang++ -std=c++20 -Wall -Wextra -Werror "$project_dir/tests/test_device_profile.cpp" -o "$profile_test_binary"
"$profile_test_binary"
clang++ -std=c++20 -Wall -Wextra -Werror "$project_dir/tests/test_controller_detector.cpp" -o "$detector_test_binary"
"$detector_test_binary"
clang++ -std=c++20 -Wall -Wextra -Werror "$project_dir/tests/test_swam_classifier.cpp" -o "$swam_test_binary"
"$swam_test_binary"

echo "All FengYin tests passed."
