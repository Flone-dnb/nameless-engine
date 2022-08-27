package main

import (
	"fmt"
	"io/ioutil"
	"os"
	"path/filepath"
	"strings"
	"time"
)

type GeneratedMetadata struct {
	Included_generated_file_count int
}

// Expects the following arguments:
// 1. Directory where generated source code is located.
// 2. Name of the file to include all generated implementation files to.
func main() {
	var time_start = time.Now()

	var args_count = len(os.Args[1:])
	if args_count < 2 {
		fmt.Println("ERROR: merge_generated_reflection.go: not enough arguments.")
		os.Exit(1)
	}

	// Save command line arguments.
	var path_to_generated_dir = os.Args[1]
	var reflection_file_name = os.Args[2]

	// Check that generated source directory exists.
	var _, err = os.Stat(path_to_generated_dir)
	if os.IsNotExist(err) {
		fmt.Println("ERROR: merge_generated_reflection.go: the specified path to the directory "+
			"with generated source files", path_to_generated_dir, "does not exist")
		os.Exit(1)
	}

	// Check that reflection file exists.
	var reflection_file_path = filepath.Join(path_to_generated_dir, reflection_file_name)
	_, err = os.Stat(reflection_file_path)
	if os.IsNotExist(err) {
		fmt.Println("ERROR: merge_generated_reflection.go: reflection file",
			reflection_file_path, "does not exist")
		os.Exit(1)
	}

	// Remove old version of reflection file.
	err = os.Remove(reflection_file_path)
	if err != nil {
		fmt.Println("ERROR: merge_generated_reflection.go: failed to remove reflection file: ", err)
		os.Exit(1)
	}

	// Create new file to fill.
	file, err := os.Create(reflection_file_path)
	if err != nil {
		fmt.Println("ERROR: merge_generated_reflection.go: failed to create reflection file: ", err)
		os.Exit(1)
	}
	file.WriteString("#pragma once\n\n")
	file.Close()

	include_generated_directory(path_to_generated_dir, reflection_file_path)

	var time_elapsed = time.Since(time_start)
	fmt.Println("SUCCESS: merge_generated_reflection.go: took", time_elapsed.Milliseconds(), "ms")
}

func include_generated_directory(path_to_generated_dir string, reflection_file_path string) {
	// Get all implementation files from generated directory.
	items, err := ioutil.ReadDir(path_to_generated_dir)
	if err != nil {
		fmt.Println("ERROR: merge_generated_reflection.go:", err)
		os.Exit(1)
	}

	for _, item := range items {
		if item.IsDir() {
			fmt.Println("ERROR: merge_generated_reflection.go: found a directory at", filepath.Join(path_to_generated_dir, item.Name()))
			os.Exit(1)
		}

		add_generated_file(filepath.Join(path_to_generated_dir, item.Name()), reflection_file_path)
	}
}

func add_generated_file(generated_file_path string, reflection_file_path string) {
	if is_ignored_file(generated_file_path, ".generated_impl.h") {
		return
	}

	file, err := os.OpenFile(reflection_file_path, os.O_APPEND|os.O_WRONLY, 0600)
	if err != nil {
		fmt.Println("ERROR: merge_generated_reflection.go: failed to open file", generated_file_path)
		os.Exit(1)
	}

	file.WriteString(fmt.Sprintf("#include \"%s\"\n", filepath.Base(generated_file_path)))
	file.Close()
}

func is_ignored_file(generated_file_path string, generated_implementation_file_extension string) bool {
	if !strings.HasSuffix(generated_file_path, generated_implementation_file_extension) {
		return true // ignore these files
	}

	return false
}
