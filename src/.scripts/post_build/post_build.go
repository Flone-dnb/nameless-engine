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
// 6. Absolute path to directory with `delete_nongame_files` script.

package main

import (
	"fmt"
	"io"
	"net/http"
	"os"
	"path/filepath"
	"runtime"
	"strings"
	"time"

	cp "github.com/otiai10/copy"
)

var log_prefix = "post_build.go:"
var res_copy_reminder_file_name = "COPY_UPDATED_RES_DIRECTORY_HERE"
var res_dir_name = "res"

func main() {
	// Mark start time.
	var time_start = time.Now()

	// Make sure we have enough arguments passed.
	var expected_arg_count = 6
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
	var is_release = false
	var path_to_delete_nongame_files_script_dir = os.Args[6]

	// Parse current build type.
	if os.Args[5] == "1" {
		is_release = true
		fmt.Println(log_prefix, "current build mode is RELEASE.")
	} else if os.Args[5] == "0" {
		is_release = false
		fmt.Println(log_prefix, "current build mode is DEBUG.")
	} else {
		fmt.Println(log_prefix, "unknown build mode, expected 0 or 1, received", is_release)
		os.Exit(1)
	}

	// Print some info.
	fmt.Println(log_prefix, "using the following build directory:", output_build_directory)

	// Copy dynamic libraries.
	copy_ext_libs(ext_directory, working_directory, output_build_directory)

	// Copy external licenses to the build directory (if release build).
	if !is_release {
		fmt.Println(log_prefix, "copying external licenses to the build directory...")
		copy_ext_licenses(ext_directory, output_build_directory)
	} else {
		fmt.Println(log_prefix, "skip copying external licenses step because running DEBUG build")
	}

	// Handle `res` directory.
	if !is_release {
		remove_copy_res_reminder_file(output_build_directory)

		// Create symlinks to the `res` directory.
		make_simlink_to_res(res_directory, working_directory, output_build_directory)
	} else {
		// Remove symlink from directory with executable file so that the application will fail
		// to start which should remind the developer to copy new version of the `res` directory.
		remove_simlink_to_res_from_build_dir(output_build_directory)

		create_copy_res_reminder_file(output_build_directory)
	}

	// Copy MSVC redist if build in release on Windows.
	if runtime.GOOS == "windows" && is_release {
		add_redist(output_build_directory)
	}

	// Handle `delete_nongame_files` script.
	if is_release {
		remove_delete_nongame_files_script_dir(path_to_delete_nongame_files_script_dir, output_build_directory)
		copy_delete_nongame_files_script_dir(path_to_delete_nongame_files_script_dir, output_build_directory)
	} else {
		remove_delete_nongame_files_script_dir(path_to_delete_nongame_files_script_dir, output_build_directory)
	}

	// Print elapsed time.
	var time_elapsed = time.Since(time_start)
	fmt.Println(log_prefix, "done, took", time_elapsed.Milliseconds(), "ms")
}

// Deletes `delete_nongame_files` script directory from the specified output build directory.
func remove_delete_nongame_files_script_dir(path_to_script_dir string, output_build_directory string) {
	// Make sure script directory exists.
	var script_dir_stat, err = os.Stat(path_to_script_dir)
	if os.IsNotExist(err) {
		fmt.Println(log_prefix, "directory", path_to_script_dir, "does not exist")
		os.Exit(1)
	}

	// Make sure script directory path is not a file.
	if !script_dir_stat.Mode().IsDir() {
		fmt.Println(log_prefix, path_to_script_dir, "is not a directory")
		os.Exit(1)
	}

	var script_name = filepath.Base(path_to_script_dir)

	// Make sure output directory exists.
	_, err = os.Stat(output_build_directory)
	if os.IsNotExist(err) {
		fmt.Println(log_prefix, "directory", output_build_directory, "does not exist")
		os.Exit(1)
	}

	// Construct resulting path.
	var dst_script_dir_path = filepath.Join(output_build_directory, script_name)

	// Delete script directory from output directory (if exists).
	_, err = os.Stat(dst_script_dir_path)
	if err == nil {
		// Remove existing directory.
		err = os.RemoveAll(dst_script_dir_path)
		if err != nil {
			fmt.Println(log_prefix, "failed to remove directory", dst_script_dir_path)
			os.Exit(1)
		}
	}
}

func copy_delete_nongame_files_script_dir(path_to_script_dir string, output_build_directory string) {
	// Make sure script directory exists.
	var script_dir_stat, err = os.Stat(path_to_script_dir)
	if os.IsNotExist(err) {
		fmt.Println(log_prefix, "directory", path_to_script_dir, "does not exist")
		os.Exit(1)
	}

	// Make sure script directory path is not a file.
	if !script_dir_stat.Mode().IsDir() {
		fmt.Println(log_prefix, path_to_script_dir, "is not a directory")
		os.Exit(1)
	}

	var script_name = filepath.Base(path_to_script_dir)

	// Make sure output directory exists.
	_, err = os.Stat(output_build_directory)
	if os.IsNotExist(err) {
		fmt.Println(log_prefix, "directory", output_build_directory, "does not exist")
		os.Exit(1)
	}

	// Construct resulting path.
	var dst_script_dir_path = filepath.Join(output_build_directory, script_name)

	// Delete old copied script directory (if exists).
	_, err = os.Stat(dst_script_dir_path)
	if err == nil {
		// Remove existing directory.
		err = os.RemoveAll(dst_script_dir_path)
		if err != nil {
			fmt.Println(log_prefix, "failed to remove directory", dst_script_dir_path)
			os.Exit(1)
		}
	}

	// Copy script directory.
	err = cp.Copy(path_to_script_dir, dst_script_dir_path)
	if err != nil {
		fmt.Println(log_prefix, "failed to copy", path_to_script_dir, "to", dst_script_dir_path)
		os.Exit(1)
	}
}

func remove_copy_res_reminder_file(output_build_directory string) {
	// Prepare path to file.
	var path_to_file = filepath.Join(output_build_directory, res_copy_reminder_file_name)

	// Make sure the file exists.
	var _, err = os.Stat(path_to_file)
	if os.IsNotExist(err) {
		return // nothing to
	}

	// Remove the file.
	err = os.Remove(path_to_file)
	if err != nil {
		fmt.Println(log_prefix, "failed to create file at", path_to_file)
		os.Exit(1)
	}
}

func create_copy_res_reminder_file(output_build_directory string) {
	// Create a "reminder" file.
	err := os.WriteFile(filepath.Join(output_build_directory, res_copy_reminder_file_name), []byte(""), 0755)
	if err != nil {
		fmt.Println(log_prefix, "failed to create file at", output_build_directory)
		os.Exit(1)
	}
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

	// Create symlinks to `res` in the working directory and the output build directory.
	create_symlink(res_directory, filepath.Join(working_directory, res_dir_name))
	create_symlink(res_directory, filepath.Join(output_build_directory, res_dir_name))

	// Check if `Debug`/`Release` directories exist in the output build directory.
	var debug_build_res = filepath.Join(output_build_directory, "Debug")
	var release_build_res = filepath.Join(output_build_directory, "Release")
	_, err = os.Stat(debug_build_res)
	if err == nil {
		create_symlink(res_directory, filepath.Join(debug_build_res, res_dir_name))
	}
	_, err = os.Stat(release_build_res)
	if err == nil {
		create_symlink(res_directory, filepath.Join(release_build_res, res_dir_name))
	}

	fmt.Println(log_prefix, "symlinks to resources directory were created")
}

func remove_simlink_to_res_from_build_dir(output_build_directory string) {
	// Make sure build directory exists.
	var err error
	_, err = os.Stat(output_build_directory)
	if os.IsNotExist(err) {
		fmt.Println(log_prefix, "build directory", output_build_directory, "does not exist")
		os.Exit(1)
	}

	// Build path to symlink.
	var path_to_symlink = filepath.Join(output_build_directory, res_dir_name)
	remove_symlink_if_exists(path_to_symlink)

	// Check if `Release` directory exist in the output build directory.
	var release_build_dir = filepath.Join(output_build_directory, "Release")
	_, err = os.Stat(release_build_dir)
	if err == nil {
		remove_symlink_if_exists(filepath.Join(release_build_dir, res_dir_name))
	}
}

func remove_symlink_if_exists(symlink_location string) {
	// Make sure symlink exists.
	_, err := os.Stat(symlink_location)
	if os.IsNotExist(err) {
		return // does not exist, nothing to remove
	}

	// Remove symlink.
	err = os.Remove(symlink_location)
	if err != nil {
		fmt.Println(log_prefix, "failed to remove symlink at", symlink_location)
		os.Exit(1)
	}
}

func create_symlink(target string, symlink_location string) {
	var err = os.RemoveAll(symlink_location)
	if err != nil {
		fmt.Println(log_prefix, "failed to remove path at", symlink_location)
		os.Exit(1)
	}

	err = os.Symlink(target, symlink_location)
	if err != nil {
		fmt.Println(log_prefix, "failed to create symlink to `res` for", symlink_location, "error:", err)
		if runtime.GOOS == "windows" {
			// Maybe not enough privileges.
			fmt.Println(log_prefix, "failed to create symlink to `res` directory. "+
				"In order to create symlinks on Windows administrator rights are requires (make sure you are running your "+
				"IDE with administrator rights).")
		}
		os.Exit(1)
	}

	fmt.Println(log_prefix, "created symlink at", symlink_location)
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

	items, err := os.ReadDir(ext_directory)
	if err != nil {
		fmt.Println(log_prefix, err)
		os.Exit(1)
	}
	for _, item := range items {
		if !item.IsDir() {
			continue
		}

		var dir_name = item.Name()
		subitems, _ := os.ReadDir(filepath.Join(ext_directory, item.Name()))

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
			// Try again but look for directories right now.
			for _, subitem := range subitems {
				if !subitem.IsDir() {
					continue
				}

				var subdirname = strings.ToUpper(subitem.Name())
				if strings.Contains(subdirname, "LICENSE") {
					fmt.Println(log_prefix, "found", dir_name, "license directory")
					var src = filepath.Join(ext_directory, dir_name, subitem.Name())
					var dst = filepath.Join(build_directory, dir_name)
					var err = cp.Copy(src, dst)
					if err != nil {
						fmt.Println(log_prefix, err)
						os.Exit(1)
					}
					copied_licenses_count += 1
					found_license = true
					break
				}
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

// Copies the `src` file's contents into the `dst` file.
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
