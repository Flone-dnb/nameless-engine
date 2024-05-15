package main

import (
	"archive/zip"
	"fmt"
	"io"
	"io/ioutil"
	"net/http"
	"os"
	"path/filepath"
	"strings"
)

// Expects 1 argument:
// 1. Working directory (the directory where this script is located).
func main() {
	var args_count = len(os.Args[1:])
	if args_count == 0 {
		fmt.Println("ERROR: download_dxc.go: not enough arguments.")
		os.Exit(1)
	}

	var working_directory = os.Args[1]
	var archive_url = "https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.8.2403.2/dxc_2024_03_29.zip"

	download_dxc_build(working_directory, archive_url)
	remove_old_dxc_build(working_directory)
	unzip(filepath.Join(working_directory, get_archive_name(archive_url)), working_directory)
}

func get_archive_name(archive_url string) string {
	return archive_url[strings.LastIndex(archive_url, "/"):]
}

func download_dxc_build(working_directory string, URL string) {
	var filename = filepath.Join(working_directory, get_archive_name(URL))

	var _, err = os.Stat(filename)
	if err == nil {
		// Exists.
		fmt.Println("INFO: download_dxc.go: found DXC build", filename, " - nothing to do")
		os.Exit(0)
	}

	// Not found. See if there are any .zip files and remove them.
	items, _ := ioutil.ReadDir(working_directory)
	for _, item := range items {
		if item.IsDir() {
			continue
		} else {
			if strings.HasSuffix(item.Name(), ".zip") {
				os.Remove(filepath.Join(working_directory, item.Name()))
			}
		}
	}

	fmt.Println("INFO: download_dxc.go: downloading file", filename)

	response, err := http.Get(URL)
	if err != nil {
		fmt.Println("ERROR: download_dxc.go:", err)
		os.Exit(1)
	}
	defer response.Body.Close()

	if response.StatusCode != 200 {
		fmt.Println("ERROR: download_dxc.go: received non 200 response code, actual result:", response.StatusCode)
		os.Exit(1)
	}

	file, err := os.Create(filename)
	if err != nil {
		fmt.Println("ERROR: download_dxc.go: failed to create empty file, error:", err)
		os.Exit(1)
	}
	defer file.Close()

	_, err = io.Copy(file, response.Body)
	if err != nil {
		fmt.Println("ERROR: download_dxc.go: failed to copy downloaded bytes, error:", err)
		os.Exit(1)
	}
}

func remove_old_dxc_build(working_directory string) {
	var dirs_to_check = []string{"bin", "inc", "lib"} // dxc archive contents

	for i := 0; i < len(dirs_to_check); i += 1 {
		var current_path = filepath.Join(working_directory, dirs_to_check[i])
		var _, err = os.Stat(current_path)
		if err == nil {
			// Exists.
			err = os.RemoveAll(current_path)
			if err != nil {
				fmt.Println("ERROR: download_dxc.go: failed to remove old DXC build, error:", err)
				os.Exit(1)
			}
		}
	}

}

func unzip(src string, dest string) {
	r, err := zip.OpenReader(src)
	if err != nil {
		fmt.Println("ERROR: download_dxc.go: open zip reader, error:", err)
		os.Exit(1)
	}
	defer func() {
		if err := r.Close(); err != nil {
			fmt.Println("ERROR: download_dxc.go: error:", err)
			os.Exit(1)
		}
	}()

	os.MkdirAll(dest, 0755)

	// Closure to address file descriptors issue with all the deferred .Close() methods
	extractAndWriteFile := func(f *zip.File) {
		rc, err := f.Open()
		if err != nil {
			fmt.Println("ERROR: download_dxc.go: error:", err)
			os.Exit(1)
		}
		defer func() {
			if err := rc.Close(); err != nil {
				fmt.Println("ERROR: download_dxc.go: error:", err)
				os.Exit(1)
			}
		}()

		path := filepath.Join(dest, f.Name)

		// Check for ZipSlip (Directory traversal)
		if !strings.HasPrefix(path, filepath.Clean(dest)+string(os.PathSeparator)) {
			fmt.Println("ERROR: download_dxc.go: illegal file path:", path)
			os.Exit(1)
		}

		if f.FileInfo().IsDir() {
			os.MkdirAll(path, f.Mode())
		} else {
			os.MkdirAll(filepath.Dir(path), f.Mode())
			f, err := os.OpenFile(path, os.O_WRONLY|os.O_CREATE|os.O_TRUNC, f.Mode())
			if err != nil {
				fmt.Println("ERROR: download_dxc.go: error:", err)
				os.Exit(1)
			}
			defer func() {
				if err := f.Close(); err != nil {
					fmt.Println("ERROR: download_dxc.go: error:", err)
					os.Exit(1)
				}
			}()

			_, err = io.Copy(f, rc)
			if err != nil {
				fmt.Println("ERROR: download_dxc.go: error:", err)
				os.Exit(1)
			}
		}
	}

	for _, f := range r.File {
		extractAndWriteFile(f)
	}
}
