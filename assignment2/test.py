import sys
import subprocess
import time
import os
import threading
from colorama import init, Fore, Style

def main():
    # Initialize colorama
    init(autoreset=True)

    if len(sys.argv) != 2:
        print("Usage: python3 test_runner.py test-{something}")
        sys.exit(1)

    test_name = sys.argv[1]
    exe_path = os.path.join('test', test_name)

    if not os.path.isfile(exe_path) or not os.access(exe_path, os.X_OK):
        print(f"Executable {exe_path} not found or not executable.")
        sys.exit(1)

    output_lines = []
    start_time = time.time()
    total_timeout = 5.0  # seconds
    timeout_occurred = False

    # Add stdbuf to force line-buffering for stdout
    try:
        proc = subprocess.Popen(
            ['stdbuf', '-oL', exe_path],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            universal_newlines=True
        )
    except Exception as e:
        print(f"Failed to start {exe_path}: {e}")
        sys.exit(1)

    # Function to read output
    def read_output():
        try:
            for line in proc.stdout:
                output_lines.append(line.rstrip('\n'))
        except Exception:
            pass  # Ignore exceptions

    # Start a thread to read output
    reader_thread = threading.Thread(target=read_output)
    reader_thread.start()

    # Wait for the process to complete or timeout
    while True:
        if proc.poll() is not None:
            break  # Process finished
        elapsed_time = time.time() - start_time
        if elapsed_time > total_timeout:
            proc.kill()
            timeout_occurred = True
            break
        time.sleep(0.1)  # Sleep briefly to avoid busy waiting

    # Wait for the reader thread to finish
    reader_thread.join()

    if timeout_occurred:
        # Print raw process output on timeout, and exit without showing tests
        print("\nTest timed out. Process was killed. Raw output:\n")
        for line in output_lines:
            print(line)
        sys.exit(1)  # Exit since the test didn't complete

    # Print the collected output only if no timeout occurred
    print("Test output collected.")

    # Now process the output
    tests_passed = 0
    tests_total = 0
    i = 0
    while i < len(output_lines):
        line = output_lines[i]
        if line.startswith('### '):
            # Expected output line
            expected_line = line[4:].strip()

            # Handle '(or ...)' in expected output
            if '(or ' in expected_line and expected_line.endswith(')'):
                idx = expected_line.find('(or ')
                expected_line1 = expected_line[:idx].strip()
                alternative = expected_line[idx + 4:-1].strip()
                expected_outputs = [expected_line1, alternative]
            else:
                expected_outputs = [expected_line]

            # Collect the actual output line(s)
            i += 1
            actual_lines = []
            while i < len(output_lines):
                next_line = output_lines[i]
                if next_line.startswith('### ') or next_line == "Tests completed.":
                    # Next expected output or "Tests completed", break to compare current pair
                    i -= 1  # Adjust index to not skip this line
                    break
                else:
                    actual_lines.append(next_line.strip())
                i += 1

            # Join actual output and strip any trailing spaces/newlines
            actual_output = ' '.join(actual_lines).rstrip()

            # Strip trailing spaces/newlines from expected output
            expected_outputs = [output.rstrip() for output in expected_outputs]

            tests_total += 1

            if actual_output in expected_outputs:
                tests_passed += 1
                # Print in green
                print(f"{Fore.GREEN}Test {tests_total}: PASS{Style.RESET_ALL}")
            else:
                # Print in red
                print(f"{Fore.RED}Test {tests_total}: FAIL{Style.RESET_ALL}")
            print(f"Expected: {expected_outputs[0]}")  # Only print the first expected output
            print(f"Got     : {actual_output}\n")

        elif line == "Tests completed.":
            break  # Stop processing if "Tests completed." is found

        else:
            # Non-expected output line, ignore or process as needed
            i += 1

    # Print summary
    if tests_total == 0:
        print("No tests were run.")
    else:
        percentage = (tests_passed / tests_total) * 100
        print(f"Tests passed: {tests_passed}/{tests_total} ({percentage:.1f}%)")


if __name__ == '__main__':
    main()
