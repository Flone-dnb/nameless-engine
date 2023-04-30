// This script file is expected to be run by all executable cmake targets as a post-build step.

// This script does the following:
// - copies dynamic libraries of some external dependencies to the working directory of your IDE and in the build directory,
// - copies license files from the `ext` directory to the build directory,
// - creates simlinks to the `res` directory in the working directory of your IDE and in the build directory,
// - (if building in Release mode on Windows) adds MSVC redist to the build directory.

// Expects the following arguments:
// 1. Absolute path to the `res` directory.
// 2. Absolute path to the `ext` directory.
// 3. Absolute path to the working directory of your IDE.
// 4. Absolute path to the output build directory (where resulting binary will be located).
// 5. Value 0 or 1 the determines whether this is a release build or not.

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
	"time"
)

var log_prefix = "post_build.go:"

func main() {
	// Mark start time.
	var time_start = time.Now()

	// Make sure we have enough arguments passed.
	var expected_arg_count = 5
	var args_count = len(os.Args[1:])
	if args_count != expected_arg_count {
		fmt.Println(log_prefix, "expected", expected_arg_count, "arguments.")
		os.Exit(1)
	}

	// Save arguments.
	var res_directory = os.Args[1]
	var ext_directory = os.Args[2]
	var working_directory = os.Args[3]
	var output_build_directory = os.Args[4]
	var is_release = os.Args[5]

	// Print current build type.
	if is_release == "1" {
		fmt.Println(log_prefix, "current build mode is RELEASE.")
	} else if is_release == "0" {
		fmt.Println(log_prefix, "current build mode is DEBUG.")
	} else {
		fmt.Println(log_prefix, "unknown build mode, expected 0 or 1, received", is_release)
		os.Exit(1)
	}

	// Copy dynamic libraries.
	copy_ext_libs(ext_directory, working_directory, output_build_directory)

	// Copy external licenses to the build directory (if release build).
	if is_release == "1" {
		fmt.Println(log_prefix, "copying external licenses to the build directory...")
		copy_ext_licenses(ext_directory, output_build_directory)
	} else {
		fmt.Println(log_prefix, "skip copying external licenses step because running DEBUG build.")
	}

	// Create symlinks to the `res` directory.
	make_simlink_to_res(res_directory, working_directory, output_build_directory)

	// Copy MSVC redisk if build in release on Windows.
	if runtime.GOOS == "windows" && is_release == "1" {
		add_redist(output_build_directory)
	}

	// Print elapsed time.
	var time_elapsed = time.Since(time_start)
	fmt.Println(log_prefix, "done, took", time_elapsed.Milliseconds(), "ms")
}

func copy_ext_libs(ext_directory string, working_directory string, build_directory string) {
	fmt.Println(log_prefix, "copying dynamic libraries of some external dependencies to the build directory")

	// Make sure the working directory exists.
	var _, err = os.Stat(working_directory)
	if os.IsNotExist(err) {
		fmt.Println(log_prefix, "working directory", working_directory, "does not exist")
		os.Exit(1)
	}

	// Make sure the build directory exists.
	_, err = os.Stat(build_directory)
	if os.IsNotExist(err) {
		fmt.Println(log_prefix, "build directory", build_directory, "does not exist")
		os.Exit(1)
	}

	// Make sure external directory exists.
	_, err = os.Stat(ext_directory)
	if os.IsNotExist(err) {
		fmt.Println(log_prefix, "external directory", ext_directory, "does not exist")
		os.Exit(1)
	}

	// Copy Refureku dynamic library.
	var refureku_dyn_lib_name = "Refureku.dll"
	if runtime.GOOS != "windows" {
		if runtime.GOOS == "linux" {
			refureku_dyn_lib_name = "libRefureku.so"
		} else {
			fmt.Println(log_prefix, "this OS is not supported")
			os.Exit(1)
		}
	}
	var refureku_dyn_lib_path = filepath.Join(ext_directory, "Refureku", "build", "Bin", refureku_dyn_lib_name)
	copy(refureku_dyn_lib_path, filepath.Join(working_directory, refureku_dyn_lib_name))
	copy(refureku_dyn_lib_path, filepath.Join(build_directory, refureku_dyn_lib_name))

	if runtime.GOOS == "windows" {
		// Copy DXC dynamic libraries.
		var dxc_dyn_lib_name1 = "dxcompiler.dll"
		var dxc_dyn_lib_name2 = "dxil.dll"

		var dxc_dyn_lib_base_path = filepath.Join(ext_directory, "DirectXShaderCompiler", "bin", "x64")

		var dxc_dyn_lib_path1 = filepath.Join(dxc_dyn_lib_base_path, dxc_dyn_lib_name1)
		var dxc_dyn_lib_path2 = filepath.Join(dxc_dyn_lib_base_path, dxc_dyn_lib_name2)

		copy(dxc_dyn_lib_path1, filepath.Join(working_directory, dxc_dyn_lib_name1))
		copy(dxc_dyn_lib_path1, filepath.Join(build_directory, dxc_dyn_lib_name1))

		copy(dxc_dyn_lib_path2, filepath.Join(working_directory, dxc_dyn_lib_name2))
		copy(dxc_dyn_lib_path2, filepath.Join(build_directory, dxc_dyn_lib_name2))
	}
}

func add_redist(build_directory string) {
	fmt.Println(log_prefix, "downloading redistributable package to the build directory")

	var redist_dir = filepath.Join(build_directory, "redist")
	var _, err = os.Stat(redist_dir)
	if os.IsNotExist(err) {
		err = os.Mkdir(redist_dir, 0755)
		if err != nil {
			fmt.Println(log_prefix, "failed to create directory", redist_dir, "error:", err)
			os.Exit(1)
		}
	}

	download_file("https://aka.ms/vs/17/release/vc_redist.x64.exe", redist_dir)
}

func download_file(URL string, download_directory string) {
	var filename = filepath.Join(download_directory, URL[strings.LastIndex(URL, "/"):])

	fmt.Println(log_prefix, "downloading file", filename)

	response, err := http.Get(URL)
	if err != nil {
		fmt.Println(log_prefix, err)
		os.Exit(1)
	}
	defer response.Body.Close()

	if response.StatusCode != 200 {
		fmt.Println(log_prefix, "received non 200 response code, actual result:", response.StatusCode)
		os.Exit(1)
	}

	file, err := os.Create(filename)
	if err != nil {
		fmt.Println(log_prefix, "failed to create empty file, error:", err)
		os.Exit(1)
	}
	defer file.Close()

	_, err = io.Copy(file, response.Body)
	if err != nil {
		fmt.Println(log_prefix, "failed to copy downloaded bytes, error:", err)
		os.Exit(1)
	}
}

func make_simlink_to_res(res_directory string, working_directory string, output_build_directory string) {
	var err error
	_, err = os.Stat(res_directory)
	if os.IsNotExist(err) {
		fmt.Println(log_prefix, "resources directory", res_directory, "does not exist")
		os.Exit(1)
	}

	_, err = os.Stat(working_directory)
	if os.IsNotExist(err) {
		fmt.Println(log_prefix, "working directory", working_directory, "does not exist")
		os.Exit(1)
	}

	_, err = os.Stat(output_build_directory)
	if os.IsNotExist(err) {
		fmt.Println(log_prefix, "build directory", output_build_directory, "does not exist")
		os.Exit(1)
	}

	fmt.Println(log_prefix, "resources directory:", res_directory)
	fmt.Println(log_prefix, "working directory:", working_directory)
	fmt.Println(log_prefix, "build directory:", output_build_directory)

	// Create symlinks to `res` in the working directory and the output build directory.
	create_symlink(res_directory, filepath.Join(working_directory, "res"))
	create_symlink(res_directory, filepath.Join(output_build_directory, "res"))

	fmt.Println(log_prefix, "symlinks to resources directory were created.")
}

func create_symlink(target string, symlink_location string) {
	var _, err = os.Stat(filepath.Dir(symlink_location))
	if os.IsNotExist(err) {
		return // directory does not exist
	}

	var create_symlink = false

	fi, err := os.Lstat(symlink_location) // read symlink
	if os.IsNotExist(err) {
		create_symlink = true
	} else if fi.Mode()&os.ModeSymlink != 0 { // make sure this is symlink
		_, err := os.Readlink(fi.Name())
		if err != nil {
			fmt.Println(log_prefix, "found broken symlink at", symlink_location, "attempting to fix it...")
			os.RemoveAll(symlink_location)
			create_symlink = true
		}
		return // nothing to do
	} else {
		// not a symlink
		fmt.Println(log_prefix, "found broken symlink at", symlink_location, "attempting to fix it...")
		os.RemoveAll(symlink_location)
		create_symlink = true
	}

	if create_symlink {
		err = os.Symlink(target, symlink_location)
		if err != nil {
			fmt.Println(log_prefix, "failed to create symlink to 'res' for", symlink_location, "error:", err)
			if runtime.GOOS == "windows" {
				// Maybe not enough privileges.
				fmt.Println(log_prefix, "failed to create symlink to 'res' directory. "+
					"In order to create symlinks on Windows administrator rights are requires (make sure you are running your "+
					"IDE with administrator rights).")
			}
			os.Exit(1)
		}
	}
}

func copy_ext_licenses(ext_directory string, build_directory string) {
	var err error
	// Check that 'ext' directory exists.
	_, err = os.Stat(ext_directory)
	if os.IsNotExist(err) {
		fmt.Println(log_prefix, "external directory", ext_directory, "does not exist")
		os.Exit(1)
	}

	// Check that build directory exists.
	_, err = os.Stat(build_directory)
	if os.IsNotExist(err) {
		fmt.Println(log_prefix, "build directory", build_directory, "does not exist")
		os.Exit(1)
	}

	var engine_license_file_path = filepath.Join(ext_directory, "..", "LICENSE")
	// Check that engine license file exists.
	_, err = os.Stat(build_directory)
	if os.IsNotExist(err) {
		fmt.Println(log_prefix, "engine license file", engine_license_file_path, "does not exist")
		os.Exit(1)
	}

	fmt.Println(log_prefix, "external directory:", ext_directory)
	fmt.Println(log_prefix, "build directory:", build_directory)

	build_directory = filepath.Join(build_directory, "ext")
	_, err = os.Stat(build_directory)
	if os.IsNotExist(err) {
		err = os.Mkdir(build_directory, os.ModePerm)
		if err != nil {
			fmt.Println(log_prefix, "failed to create directory for external licenses",
				build_directory, "error:", err)
			os.Exit(1)
		}
	} else {
		err = os.RemoveAll(build_directory)
		if err != nil {
			fmt.Println(log_prefix, "failed to remove old directory for external licenses",
				build_directory, "error:", err)
			os.Exit(1)
		}
		err = os.Mkdir(build_directory, os.ModePerm)
		if err != nil {
			fmt.Println(log_prefix, "failed to create directory for external licenses",
				build_directory, "error:", err)
			os.Exit(1)
		}
	}

	var copied_licenses_count = 0

	items, err := ioutil.ReadDir(ext_directory)
	if err != nil {
		fmt.Println(log_prefix, err)
		os.Exit(1)
	}
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

			var filename = strings.ToUpper(subitem.Name())
			if strings.Contains(filename, "LICENSE") ||
				strings.Contains(filename, "COPYING") {
				fmt.Println(log_prefix, "found", dir_name, "license file")
				var src = filepath.Join(ext_directory, dir_name, subitem.Name())
				var dst = filepath.Join(build_directory, dir_name+".txt")
				copy(src, dst)
				copied_licenses_count += 1
				found_license = true
				break
			}
		}

		if !found_license {
			fmt.Println(log_prefix, "could not find a license file for dependency", dir_name)
			os.Exit(1)
		}
	}

	// Copy engine license file.
	fmt.Println(log_prefix, "copying engine license file")
	copy(engine_license_file_path, filepath.Join(build_directory, "nameless-engine.txt"))
	copied_licenses_count += 1

	fmt.Println(log_prefix, "copied", copied_licenses_count, "license file(-s)")
}

// / Copies the `src` file's contents into the `dst` file.
func copy(src string, dst string) {
	sourceFileStat, err := os.Stat(src)
	if err != nil {
		fmt.Println(log_prefix, err)
		os.Exit(1)
	}

	if !sourceFileStat.Mode().IsRegular() {
		fmt.Println(log_prefix, src, "is not a file")
		os.Exit(1)
	}

	source, err := os.Open(src)
	if err != nil {
		fmt.Println(log_prefix, "failed to open file", src, "error:", err)
		os.Exit(1)
	}
	defer source.Close()

	destination, err := os.Create(dst)
	if err != nil {
		fmt.Println(log_prefix, "failed to create file", dst, "error:", err)
		os.Exit(1)
	}
	defer destination.Close()
	_, err = io.Copy(destination, source)
	if err != nil {
		fmt.Println(log_prefix, "failed to copy file", src, "to", dst, "error:", err)
		os.Exit(1)
	}
}
