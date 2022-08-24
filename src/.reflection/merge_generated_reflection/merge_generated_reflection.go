package main

import (
	"errors"
	"fmt"
	"io/ioutil"
	"os"
	"path/filepath"
	"strings"
	"time"

	"github.com/pelletier/go-toml/v2"
)

type GeneratedMetadata struct {
	Included_generated_file_count int
}

// Expects the following arguments:
// 1. Directory where generated source code is located.
// 2. Name of the file (.h/.cpp) to include all generated files to
// (headers to header, sources to source file).
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

	var generated_metadata_file_path = filepath.Join(path_to_generated_dir, ".generated.toml")
	var generated_file_count = count_generated_files(
		path_to_generated_dir, reflection_header_path, reflection_source_path)
	var previous_generated_file_count = get_previous_generated_file_count(
		generated_metadata_file_path, reflection_header_path, reflection_source_path)

	if generated_file_count != previous_generated_file_count {
		fmt.Println("INFO: merge_generated_reflection.go: generated file count changed, recreating reflection files...")

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

		var metadata GeneratedMetadata
		metadata.Included_generated_file_count = generated_file_count
		save_generated_metadata(generated_metadata_file_path, metadata)
	}

	var time_elapsed = time.Since(time_start)
	fmt.Println("SUCCESS: merge_generated_reflection.go: took", time_elapsed.Milliseconds(), "ms")
}

func count_generated_files(path_to_generated_dir string, reflection_header_path string, reflection_source_path string) int {
	// Get all files from generated directory.
	items, err := ioutil.ReadDir(path_to_generated_dir)
	if err != nil {
		fmt.Println("ERROR: merge_generated_reflection.go:", err)
		os.Exit(1)
	}

	var total_file_count = 0

	for _, item := range items {
		if item.IsDir() {
			fmt.Println("ERROR: merge_generated_reflection.go: found a directory at", filepath.Join(path_to_generated_dir, item.Name()))
			os.Exit(1)
		}

		if is_ignored_file(filepath.Join(path_to_generated_dir, item.Name()), reflection_header_path, reflection_source_path) {
			continue
		}

		total_file_count += 1
	}

	return total_file_count
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
	if is_ignored_file(generated_file_path, reflection_header_path, reflection_source_path) {
		return
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

func is_ignored_file(generated_file_path string, reflection_header_path string, reflection_source_path string) bool {
	if filepath.Base(generated_file_path) == filepath.Base(reflection_header_path) ||
		filepath.Base(generated_file_path) == filepath.Base(reflection_source_path) ||
		filepath.Base(generated_file_path) == "EntityMacros.h" ||
		strings.HasSuffix(generated_file_path, ".toml") {
		return true // ignore these files
	}

	return false
}

func get_previous_generated_file_count(path_to_generated_metadata_file string, reflection_header_path string, reflection_source_path string) int {
	// Check that our toml file exists.
	var _, err = os.Stat(path_to_generated_metadata_file)
	if os.IsNotExist(err) {
		return -1
	}

	in_file, err := os.Open(path_to_generated_metadata_file)
	if err != nil {
		fmt.Println("ERROR: merge_generated_reflection.go: failed to open file", path_to_generated_metadata_file, "error:", err)
		os.Exit(1)
	}

	d := toml.NewDecoder(in_file)
	d.DisallowUnknownFields()

	var cfg GeneratedMetadata
	err = d.Decode(&cfg)
	if err != nil {
		var details *toml.StrictMissingError
		if errors.As(err, &details) {
			fmt.Println("ERROR: merge_generated_reflection.go: error while reading TOML data - StrictMissingError:",
				details.String())
		} else {
			fmt.Println("ERROR: merge_generated_reflection.go: error while reading TOML data", err)
		}

		os.Exit(1)
	}

	return cfg.Included_generated_file_count
}

func save_generated_metadata(path_to_generated_metadata_file string, metadata GeneratedMetadata) {
	out_file, err := os.Create(path_to_generated_metadata_file)
	if err != nil {
		fmt.Println("ERROR: merge_generated_reflection.go: failed to create new file",
			path_to_generated_metadata_file, "error:", err)
		os.Exit(1)
	}
	bytes, err := toml.Marshal(metadata)
	if err != nil {
		fmt.Println("ERROR: merge_generated_reflection.go:", err)
		os.Exit(1)
	}
	_, err = out_file.Write(bytes)
	if err != nil {
		fmt.Println("ERROR: merge_generated_reflection.go: failed to write to new file",
			path_to_generated_metadata_file, "error:", err)
		os.Exit(1)
	}
	out_file.Close()
}
