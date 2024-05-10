package main

import (
	"fmt"
	"io"
	"net/http"
	"os"
	"path/filepath"
	"runtime"
	"strings"

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
		[]string{".hlsl", ".glsl"},
		[]string{"test"})
	fmt.Println("Collected", len(source_files), "shader file(s)")

	setup_shader_formatter(path_to_src)

	// Create a new shell session.
	var session = sh.NewSession()
	session.PipeFail = true
	session.PipeStdErrors = true
	session.SetDir(path_to_src)

	// Run formatter on each source file.
	fmt.Println()
	fmt.Println("Running shader-formatter...")
	for _, path := range source_files {
		fmt.Println("checking file", path)
		var err = session.Command("./shader-formatter", path, "--only-scan").Run()
		if err != nil {
			fmt.Println(err)
			os.Exit(1)
		}
	}
	fmt.Println("Processed", len(source_files), "shader file(s)")
}

func setup_shader_formatter(path_to_download_dir string) {
	// Prepare URL to download.
	var download_url = "https://github.com/Flone-dnb/shader-formatter/releases/latest/download/shader-formatter"
	if runtime.GOOS == "windows" {
		download_url += ".exe"
	}

	var downloaded_file_path = download_file(download_url, path_to_download_dir)

	if runtime.GOOS != "windows" {
		var session = sh.NewSession()
		session.PipeFail = true
		session.PipeStdErrors = true
		session.SetDir(path_to_download_dir)

		var err = session.Command("chmod", "+x", downloaded_file_path).Run()
		if err != nil {
			fmt.Println(err)
			os.Exit(1)
		}
	}
}

func download_file(URL string, download_directory string) string {
	var filename = filepath.Join(download_directory, URL[strings.LastIndex(URL, "/"):])

	fmt.Println("downloading file", filename)

	response, err := http.Get(URL)
	if err != nil {
		fmt.Println(err)
		os.Exit(1)
	}
	defer response.Body.Close()

	if response.StatusCode != 200 {
		fmt.Println("received non 200 response code, actual result:", response.StatusCode)
		os.Exit(1)
	}

	file, err := os.Create(filename)
	if err != nil {
		fmt.Println("failed to create empty file, error:", err)
		os.Exit(1)
	}
	defer file.Close()

	_, err = io.Copy(file, response.Body)
	if err != nil {
		fmt.Println("failed to copy downloaded bytes, error:", err)
		os.Exit(1)
	}

	return filename
}

func get_source_file_paths_from_dir(path_to_src string, path_to_dir string, include_extensions []string, ignored_dirs []string) []string {
	var source_files = []string{}

	// Check if this directory is ignored.
	for _, ignored_dir_name := range ignored_dirs {
		if filepath.Base(path_to_dir) == ignored_dir_name {
			return source_files
		}
	}

	// Get directory entries.
	items, err := os.ReadDir(path_to_dir)
	if err != nil {
		fmt.Println(err)
		os.Exit(1)
	}

	// Iterate over entries in the directory.
	for _, item := range items {
		if item.IsDir() {
			// Append source files from it.
			source_files = append(source_files,
				get_source_file_paths_from_dir(
					path_to_src,
					filepath.Join(path_to_dir, item.Name()),
					include_extensions,
					ignored_dirs)[:]...)

			continue
		}

		// Save relative path to file.
		rel_path, err := filepath.Rel(path_to_src, filepath.Join(path_to_dir, item.Name()))
		if err != nil {
			fmt.Println(err)
			os.Exit(1)
		}

		// Check if this file should be ignored or not.
		var is_ignored = true
		var extension = filepath.Ext(item.Name())
		for _, included_extension := range include_extensions {
			if extension == included_extension {
				is_ignored = false
				break
			}
		}

		// Check if ignored.
		if is_ignored {
			fmt.Println("ignoring file", rel_path)
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
