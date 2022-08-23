package main

import (
	"fmt"
	"io/ioutil"
	"os"
	"path/filepath"
	"strings"
)

// Expects the following arguments:
// 1. Directory where generated source code is located.
// 2. Name of the file (.h/.cpp) to include all generated files to
// (headers to header, sources to source file).
func main() {
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

	// Check that reflection header file exists.
	var reflection_header_path = filepath.Join(path_to_generated_dir, reflection_file_name) + ".h"
	_, err = os.Stat(reflection_header_path)
	if os.IsNotExist(err) {
		fmt.Println("ERROR: merge_generated_reflection.go: reflection header file",
			reflection_header_path, "does not exist")
		os.Exit(1)
	}

	// Check that reflection source file exists.
	var reflection_source_path = filepath.Join(path_to_generated_dir, reflection_file_name) + ".cpp"
	_, err = os.Stat(reflection_source_path)
	if os.IsNotExist(err) {
		fmt.Println("ERROR: merge_generated_reflection.go: reflection source file",
			reflection_source_path, "does not exist")
		os.Exit(1)
	}

	// Remove old versions of these files.
	err = os.Remove(reflection_header_path)
	if err != nil {
		fmt.Println("ERROR: merge_generated_reflection.go: failed to remove reflection header file: ", err)
		os.Exit(1)
	}
	err = os.Remove(reflection_source_path)
	if err != nil {
		fmt.Println("ERROR: merge_generated_reflection.go: failed to remove reflection source file: ", err)
		os.Exit(1)
	}

	// Create new header file to fill.
	file, err := os.Create(reflection_header_path)
	if err != nil {
		fmt.Println("ERROR: merge_generated_reflection.go: failed to create reflection header file: ", err)
		os.Exit(1)
	}
	file.WriteString("#pragma once\n\n")
	file.Close()

	// Create new source file to fill.
	file, err = os.Create(reflection_source_path)
	if err != nil {
		fmt.Println("ERROR: merge_generated_reflection.go: failed to create reflection source file: ", err)
		os.Exit(1)
	}
	file.WriteString(fmt.Sprintf("#include \"%s\"\n\n", filepath.Base(reflection_header_path)))
	file.Close()

	include_generated_directory(path_to_generated_dir, reflection_header_path, reflection_source_path)
}

func include_generated_directory(path_to_generated_dir string, reflection_header_path string, reflection_source_path string) {
	// Get all files from generated directory.
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

		add_generated_file(filepath.Join(path_to_generated_dir, item.Name()), reflection_header_path, reflection_source_path)
	}
}

func add_generated_file(generated_file_path string, reflection_header_path string, reflection_source_path string) {
	if filepath.Base(generated_file_path) == filepath.Base(reflection_header_path) ||
		filepath.Base(generated_file_path) == filepath.Base(reflection_source_path) ||
		filepath.Base(generated_file_path) == "EntityMacros.h" {
		return // ignore these files
	}

	if strings.HasSuffix(filepath.Base(generated_file_path), ".generated.h") {
		// Header, write to .h.
		file, err := os.OpenFile(reflection_header_path, os.O_APPEND|os.O_WRONLY, 0600)
		if err != nil {
			fmt.Println("ERROR: merge_generated_reflection.go: failed to open file", generated_file_path)
			os.Exit(1)
		}

		file.WriteString(fmt.Sprintf("#include \"%s\"\n", filepath.Base(generated_file_path)))
		file.Close()
	} else if strings.HasSuffix(filepath.Base(generated_file_path), ".generated_impl.h") {
		// Implementation, write to .cpp.
		file, err := os.OpenFile(reflection_source_path, os.O_APPEND|os.O_WRONLY, 0600)
		if err != nil {
			fmt.Println("ERROR: merge_generated_reflection.go: failed to open file", generated_file_path)
			os.Exit(1)
		}

		file.WriteString(fmt.Sprintf("#include \"%s\"\n", filepath.Base(generated_file_path)))
		file.Close()
	} else {
		fmt.Println("ERROR: merge_generated_reflection.go: unexpected file extension", generated_file_path)
		os.Exit(1)
	}
}
