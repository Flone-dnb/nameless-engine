package main

import (
	"fmt"
	"io"
	"io/ioutil"
	"net/http"
	"os"
	"path/filepath"
	"runtime"
	"strings"
)

// Expects 4 arguments:
// 1. Path to the 'resources' directory ('res' directory).
// 2. Path to the 'external' directory ('ext' directory).
// 3. Path to the working directory of your IDE.
// 4. Path to the engine_lib working directory.
// 5. Path to the build directory (where resulting binary will be located).
// 6. Is release build (0 or 1).

// Does:
// - copies license files from 'ext' directory to the build directory,
// - creates a simlink to the 'res' directory in working directory and build directory.
func main() {
	var expected_arg_count = 6
	var args_count = len(os.Args[1:])
	if args_count != expected_arg_count {
		fmt.Println("ERROR: engine_post_build.go: expected", expected_arg_count, "arguments.")
		os.Exit(1)
	}

	var res_directory = os.Args[1]
	var ext_directory = os.Args[2]
	var working_directory = os.Args[3]
	var engine_lib_dir = os.Args[4]
	var build_directory = os.Args[5]
	var is_release = os.Args[6]

	if is_release == "1" {
		fmt.Println("INFO: engine_post_build.go: current build mode is RELEASE.")
	} else if is_release == "0" {
		fmt.Println("INFO: engine_post_build.go: current build mode is DEBUG.")
	} else {
		fmt.Println("ERROR: engine_post_build.go: unknown build mode, expected 0 or 1, received", is_release)
		os.Exit(1)
	}

	copy_ext_licenses(ext_directory, build_directory)
	make_simlink_to_res(res_directory, working_directory, build_directory, engine_lib_dir)

	if runtime.GOOS == "windows" && is_release == "1" {
		add_redist(build_directory)
	}
}

func add_redist(build_directory string) {
	fmt.Println("INFO: engine_post_build.go: downloading redistributable package to the build directory")

	var redist_dir = filepath.Join(build_directory, "redist")
	var _, err = os.Stat(redist_dir)
	if os.IsNotExist(err) {
		err = os.Mkdir(redist_dir, 0755)
		if err != nil {
			fmt.Println("ERROR: engine_post_build.go: failed to create directory", redist_dir, "error:", err)
			os.Exit(1)
		}
	}

	download_file("https://aka.ms/vs/17/release/vc_redist.x64.exe", redist_dir)
}

func download_file(URL string, download_directory string) {
	var filename = filepath.Join(download_directory, URL[strings.LastIndex(URL, "/"):])

	fmt.Println("INFO: engine_post_build.go: downloading file", filename)

	response, err := http.Get(URL)
	if err != nil {
		fmt.Println("ERROR: engine_post_build.go:", err)
		os.Exit(1)
	}
	defer response.Body.Close()

	if response.StatusCode != 200 {
		fmt.Println("ERROR: engine_post_build.go: received non 200 response code, actual result:", response.StatusCode)
		os.Exit(1)
	}

	file, err := os.Create(filename)
	if err != nil {
		fmt.Println("ERROR: engine_post_build.go: failed to create empty file, error:", err)
		os.Exit(1)
	}
	defer file.Close()

	_, err = io.Copy(file, response.Body)
	if err != nil {
		fmt.Println("ERROR: engine_post_build.go: failed to copy downloaded bytes, error:", err)
		os.Exit(1)
	}
}

func make_simlink_to_res(res_directory string, working_directory string, build_directory string, engine_lib_dir string) {
	var err error
	_, err = os.Stat(res_directory)
	if os.IsNotExist(err) {
		fmt.Println("ERROR: engine_post_build.go: res directory", res_directory, "does not exist")
		os.Exit(1)
	}

	_, err = os.Stat(working_directory)
	if os.IsNotExist(err) {
		fmt.Println("ERROR: engine_post_build.go: working directory", working_directory, "does not exist")
		os.Exit(1)
	}

	_, err = os.Stat(build_directory)
	if os.IsNotExist(err) {
		fmt.Println("ERROR: engine_post_build.go: build directory", build_directory, "does not exist")
		os.Exit(1)
	}

	fmt.Println("engine_post_build.go: using res directory:", res_directory)
	fmt.Println("engine_post_build.go: using working directory:", working_directory)
	fmt.Println("engine_post_build.go: using build directory:", build_directory)

	_, err = os.Stat(filepath.Join(working_directory, "res"))
	if os.IsNotExist(err) {
		err = os.Symlink(res_directory, filepath.Join(working_directory, "res"))
		if err != nil {
			fmt.Println("ERROR: engine_post_build.go: failed to create symlink to 'res' in", working_directory, "error:", err)
			if runtime.GOOS == "windows" {
				// Maybe not enough privileges.
				fmt.Println("ERROR: engine_post_build.go: failed to create symlink to 'res' directory. " +
					"In order to create symlinks on Windows administrator rights are requires (make sure you are running your " +
					"IDE with administrator rights).")
			}
			os.Exit(1)
		}
	}

	_, err = os.Stat(filepath.Join(engine_lib_dir, "res"))
	if os.IsNotExist(err) {
		err = os.Symlink(res_directory, filepath.Join(engine_lib_dir, "res"))
		if err != nil {
			fmt.Println("ERROR: engine_post_build.go: failed to create symlink to 'res' in", engine_lib_dir, "error:", err)
			if runtime.GOOS == "windows" {
				// Maybe not enough privileges.
				fmt.Println("ERROR: engine_post_build.go: failed to create symlink to 'res' directory. " +
					"In order to create symlinks on Windows administrator rights are requires (make sure you are running your " +
					"IDE with administrator rights).")
			}
			os.Exit(1)
		}
	}

	_, err = os.Stat(filepath.Join(build_directory, "res"))
	if os.IsNotExist(err) {
		err = os.Symlink(res_directory, filepath.Join(build_directory, "res"))
		if err != nil {
			fmt.Println("ERROR: engine_post_build.go: failed to create symlink to 'res' in", build_directory, "error:", err)
			if runtime.GOOS == "windows" {
				// Maybe not enough privileges.
				fmt.Println("ERROR: engine_post_build.go: failed to create symlink to 'res' directory. " +
					"In order to create symlinks on Windows administrator rights are requires (make sure you are running your " +
					"IDE with administrator rights).")
			}
			os.Exit(1)
		}
	}

	fmt.Println("SUCCESS: engine_post_build.go: symlinks to 'res' directory were created.")
}

func copy_ext_licenses(ext_directory string, build_directory string) {
	var err error
	_, err = os.Stat(ext_directory)
	if os.IsNotExist(err) {
		fmt.Println("ERROR: engine_post_build.go: ext directory", ext_directory, "does not exist")
		os.Exit(1)
	}

	_, err = os.Stat(build_directory)
	if os.IsNotExist(err) {
		fmt.Println("ERROR: engine_post_build.go: build directory", build_directory, "does not exist")
		os.Exit(1)
	}

	fmt.Println("engine_post_build.go: using ext directory:", ext_directory)
	fmt.Println("engine_post_build.go: using build directory:", build_directory)

	build_directory = filepath.Join(build_directory, "ext")
	_, err = os.Stat(build_directory)
	if os.IsNotExist(err) {
		err = os.Mkdir(build_directory, os.ModePerm)
		if err != nil {
			fmt.Println("ERROR: engine_post_build.go: failed to create directory",
				build_directory, "error:", err)
			os.Exit(1)
		}
	} else {
		err = os.RemoveAll(build_directory)
		if err != nil {
			fmt.Println("ERROR: engine_post_build.go: failed to remove old directory",
				build_directory, "error:", err)
			os.Exit(1)
		}
		err = os.Mkdir(build_directory, os.ModePerm)
		if err != nil {
			fmt.Println("ERROR: engine_post_build.go: failed to create directory",
				build_directory, "error:", err)
			os.Exit(1)
		}
	}

	var copied_licenses_count = 0

	items, _ := ioutil.ReadDir(ext_directory)
	for _, item := range items {
		if !item.IsDir() {
			continue
		}

		var dir_name = item.Name()
		subitems, _ := ioutil.ReadDir(filepath.Join(ext_directory, item.Name()))

		var found_license = false
		for _, subitem := range subitems {
			if subitem.IsDir() {
				continue
			}

			if strings.Contains(subitem.Name(), "LICENSE") {
				fmt.Println("INFO: engine_post_build.go: found", dir_name, "license file")
				var src = filepath.Join(ext_directory, dir_name, subitem.Name())
				var dst = filepath.Join(build_directory, dir_name+".txt")
				copy(src, dst)
				copied_licenses_count += 1
				found_license = true
				break
			}
		}

		if !found_license {
			// Look for "COPYING" file.
			for _, subitem := range subitems {
				if subitem.IsDir() {
					continue
				}

				if strings.Contains(subitem.Name(), "COPYING") {
					fmt.Println("INFO: engine_post_build.go: found", dir_name, "license file")
					var src = filepath.Join(ext_directory, dir_name, subitem.Name())
					var dst = filepath.Join(build_directory, dir_name+".txt")
					copy(src, dst)
					copied_licenses_count += 1
					found_license = true
					break
				}
			}

			if !found_license {
				fmt.Println("ERROR: engine_post_build.go: could not find a license "+
					"file for dependency", dir_name)
				os.Exit(1)
			}
		}
	}

	fmt.Println("SUCCESS: engine_post_build.go: copied", copied_licenses_count, "license file(-s)")
}

func copy(src string, dst string) {
	sourceFileStat, err := os.Stat(src)
	if err != nil {
		fmt.Println("ERROR: engine_post_build.go:", err)
		os.Exit(1)
	}

	if !sourceFileStat.Mode().IsRegular() {
		fmt.Println("ERROR: engine_post_build.go:", src, "is not a file")
		os.Exit(1)
	}

	source, err := os.Open(src)
	if err != nil {
		fmt.Println("ERROR: engine_post_build.go: failed to open file", src, "error:", err)
		os.Exit(1)
	}
	defer source.Close()

	destination, err := os.Create(dst)
	if err != nil {
		fmt.Println("ERROR: engine_post_build.go: failed to create file", dst, "error:", err)
		os.Exit(1)
	}
	defer destination.Close()
	_, err = io.Copy(destination, source)
	if err != nil {
		fmt.Println("ERROR: engine_post_build.go: failed to copy file", src, "to", dst, "error:", err)
		os.Exit(1)
	}
}
