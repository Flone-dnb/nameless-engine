package main

import (
	"fmt"
	"io"
	"io/ioutil"
	"os"
	"path/filepath"
	"strings"
)

// Expects 2 arguments:
// 1. Working directory (typically 'ext' directory).
// 2. Build directory (where resulting binary will be located).
func main() {
	var args_count = len(os.Args[1:])
	if args_count != 2 {
		fmt.Println("ERROR: copy_ext_licenses.go: not enough arguments.")
		os.Exit(1)
	}

	var working_directory = os.Args[1]
	var build_directory = os.Args[2]

	var err error
	_, err = os.Stat(working_directory)
	if os.IsNotExist(err) {
		fmt.Println("ERROR: copy_ext_licenses.go: working directory", working_directory, "does not exist")
		os.Exit(1)
	}

	_, err = os.Stat(build_directory)
	if os.IsNotExist(err) {
		fmt.Println("ERROR: copy_ext_licenses.go: build directory", build_directory, "does not exist")
		os.Exit(1)
	}

	fmt.Println("copy_ext_licenses.go: using working directory:", working_directory)
	fmt.Println("copy_ext_licenses.go: using build directory:", build_directory)

	build_directory = filepath.Join(build_directory, "ext")
	_, err = os.Stat(build_directory)
	if os.IsNotExist(err) {
		err = os.Mkdir(build_directory, os.ModePerm)
		if err != nil {
			fmt.Println("ERROR: copy_ext_licenses.go: failed to create directory",
				build_directory, "error:", err)
			os.Exit(1)
		}
	} else {
		err = os.RemoveAll(build_directory)
		if err != nil {
			fmt.Println("ERROR: copy_ext_licenses.go: failed to remove old directory",
				build_directory, "error:", err)
			os.Exit(1)
		}
		err = os.Mkdir(build_directory, os.ModePerm)
		if err != nil {
			fmt.Println("ERROR: copy_ext_licenses.go: failed to create directory",
				build_directory, "error:", err)
			os.Exit(1)
		}
	}

	var copied_licenses_count = 0

	items, _ := ioutil.ReadDir(working_directory)
	for _, item := range items {
		if !item.IsDir() {
			continue
		}

		var dir_name = item.Name()
		subitems, _ := ioutil.ReadDir(filepath.Join(working_directory, item.Name()))

		var found_license = false
		for _, subitem := range subitems {
			if subitem.IsDir() {
				continue
			}

			if strings.Contains(subitem.Name(), "LICENSE") {
				fmt.Println("copy_ext_licenses.go: found", dir_name, "license file")
				var src = filepath.Join(working_directory, dir_name, subitem.Name())
				var dst = filepath.Join(build_directory, dir_name+".txt")
				copy(src, dst)
				copied_licenses_count += 1
				found_license = true
				break
			}
		}

		if !found_license {
			fmt.Println("ERROR: copy_ext_licenses.go: could not find a license "+
				"file for dependency", dir_name)
			os.Exit(1)
		}
	}

	fmt.Println("SUCCESS: copy_ext_licenses.go: copied", copied_licenses_count, "license file(-s)")
}

func copy(src string, dst string) {
	sourceFileStat, err := os.Stat(src)
	if err != nil {
		fmt.Println("ERROR: copy_ext_licenses.go:", err)
		os.Exit(1)
	}

	if !sourceFileStat.Mode().IsRegular() {
		fmt.Println("ERROR: copy_ext_licenses.go:", src, "is not a file")
		os.Exit(1)
	}

	source, err := os.Open(src)
	if err != nil {
		fmt.Println("ERROR: copy_ext_licenses.go: failed to open file", src, "error:", err)
		os.Exit(1)
	}
	defer source.Close()

	destination, err := os.Create(dst)
	if err != nil {
		fmt.Println("ERROR: copy_ext_licenses.go: failed to create file", dst, "error:", err)
		os.Exit(1)
	}
	defer destination.Close()
	_, err = io.Copy(destination, source)
	if err != nil {
		fmt.Println("ERROR: copy_ext_licenses.go: failed to copy file", src, "to", dst, "error:", err)
		os.Exit(1)
	}
}
