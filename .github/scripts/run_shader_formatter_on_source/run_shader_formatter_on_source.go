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
	var path_to_shaders = os.Args[1]
	fi, err := os.Stat(path_to_shaders)
	if err != nil {
		fmt.Println(err)
		os.Exit(1)
	}
	if !fi.Mode().IsDir() {
		fmt.Println("expected the specified path", path_to_shaders, "to be a directory")
		os.Exit(1)
	}

	var path_to_shader_formatter = filepath.Dir(path_to_shaders)
	setup_shader_formatter(path_to_shader_formatter)

	// Create a new shell session.
	var session = sh.NewSession()
	session.PipeFail = true
	session.PipeStdErrors = true
	session.SetDir(path_to_shaders)

	// Run formatter on each source file.
	fmt.Println("Running shader-formatter in path", path_to_shaders)
	err = session.Command("./../shader-formatter", path_to_shaders, "--only-scan").Run()
	if err != nil {
		fmt.Println(err)
		os.Exit(1)
	}
	fmt.Println("Done.")
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
