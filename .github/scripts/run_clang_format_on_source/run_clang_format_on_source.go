package main

import (
	"fmt"
	"os"
	"path/filepath"

	"github.com/codeskyblue/go-sh"
)

func main() {
	// Make sure we have enough arguments passed.
	var expected_arg_count = 1
	var args_count = len(os.Args[1:])
	if args_count != expected_arg_count {
		fmt.Println("expected", expected_arg_count, "arguments")
		os.Exit(1)
	}

	// Save arguments.
	var path_to_src = os.Args[1]

	fmt.Println("Collecting source files...")
	var source_files = get_source_file_paths_from_dir(
		path_to_src,
		path_to_src,
		[]string{".cmake", ".scripts", ".generated", "engine_tests"},
		[]string{".gitignore", "CMakeLists.txt"})
	fmt.Println("Collected", len(source_files), "source file(s)")

	// Create a new shell session.
	var session = sh.NewSession()
	session.PipeFail = true
	session.PipeStdErrors = true
	session.SetDir(path_to_src)

	// Run clang-format on each source file.
	fmt.Println()
	fmt.Println("Running clang-format...")
	for _, path := range source_files {
		var err = session.Command("clang-format", "--dry-run", "--Werror", "--style=file", "--verbose", path).Run()
		if err != nil {
			fmt.Println(err)
			os.Exit(1)
		}
	}
	fmt.Println("Processed", len(source_files), "source file(s)")
}

func get_source_file_paths_from_dir(path_to_src string, path_to_dir string, ignored_dir_names []string, ignored_file_names []string) []string {
	// Get directory entries.
	items, err := os.ReadDir(path_to_dir)
	if err != nil {
		fmt.Println(err)
		os.Exit(1)
	}

	var source_files = []string{}

	// Iterate over entries in the directory.
	for _, item := range items {
		if item.IsDir() {
			// Check if this directory is ignored.
			var is_ignored = false
			for _, ignored_name := range ignored_dir_names {
				if item.Name() == ignored_name {
					rel_path, err := filepath.Rel(path_to_src, filepath.Join(path_to_dir, item.Name()))
					if err != nil {
						fmt.Println(err)
						os.Exit(1)
					}
					fmt.Println("ignoring directory", rel_path)
					is_ignored = true
					break
				}
			}

			// Check if directory was ignored.
			if is_ignored {
				continue
			}

			// Append source files from it.
			source_files = append(source_files,
				get_source_file_paths_from_dir(
					path_to_src,
					filepath.Join(path_to_dir, item.Name()),
					ignored_dir_names,
					ignored_file_names)[:]...)

			continue
		}

		// Save relative path to file.
		rel_path, err := filepath.Rel(path_to_src, filepath.Join(path_to_dir, item.Name()))
		if err != nil {
			fmt.Println(err)
			os.Exit(1)
		}

		// Check if this file name is ignored.
		var is_ignored = false
		for _, ignored_name := range ignored_file_names {
			if item.Name() == ignored_name {
				fmt.Println("ignoring file", rel_path)
				is_ignored = true
				break
			}
		}

		// Check if ignored.
		if is_ignored {
			continue
		}

		// Get absolute path.
		abs_path, err := filepath.Abs(filepath.Join(path_to_dir, item.Name()))
		if err != nil {
			fmt.Println(err)
			os.Exit(1)
		}

		// Add file.
		source_files = append(source_files, abs_path)
		fmt.Println("+ adding file", rel_path)
	}

	return source_files
}
