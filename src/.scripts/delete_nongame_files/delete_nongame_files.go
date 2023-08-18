// This script file removes all non-game related files from the parent directory.
// Expects that parent directory contains game's executable file.

package main

import (
	"bufio"
	"fmt"
	"os"
	"path/filepath"
	"runtime"
	"strings"
)

var log_prefix = "delete_nongame_files.go:"
var res_copy_reminder_file_name = "COPY_UPDATED_RES_DIRECTORY_HERE"
var res_dir_name = "res"
var test_dir_name = "test"             // located at `res/test`
var gitignore_file_name = ".gitignore" // located at `res/.gitignore`

func main() {
	// Get working directory.
	workdir, err := os.Getwd()
	if err != nil {
		fmt.Println(log_prefix, "failed to get working directory")
		os.Exit(1)
	}

	// Get path to directory with game's binary.
	var path_to_binary_dir = filepath.Dir(workdir)

	// Ask for confirmation.
	yes, err := ask_user("the script will now delete non-game related files from the directory \"" +
		path_to_binary_dir + "\" continue? (y/n)")
	if err != nil {
		fmt.Println(log_prefix, "failed to receive user input, error:", err)
		os.Exit(1)
	}

	// Exit if canceled.
	if !yes {
		return
	}

	// Check if `res` directory exists.
	var path_to_res_dir_copy = filepath.Join(path_to_binary_dir, res_dir_name)
	_, err = os.Stat(path_to_res_dir_copy)
	if os.IsNotExist(err) {
		// Ask for confirmation.
		yes2, err := ask_user("unable to find a copy of the \"" + res_dir_name +
			"\" directory in \"" + path_to_binary_dir + "\", it contains some files/directories that " +
			"exported game does not need (like `res/test` directory) and this script can also delete them " +
			"for you, do you want to do it manually after you copy the \"" + res_dir_name + "\" directory later (y) or " +
			"you want to stop and copy this directory right now (n)? (y/n)")
		if err != nil {
			fmt.Println(log_prefix, "failed to receive user input, error:", err)
			os.Exit(1)
		}
		if !yes2 {
			return
		}
	} else {
		// Prepare paths to delete.
		var directories_to_delete_from_res = []string{filepath.Join(path_to_binary_dir, res_dir_name, test_dir_name)}
		var files_to_delete_from_res = []string{filepath.Join(path_to_binary_dir, res_dir_name, gitignore_file_name)}

		// Print warning.
		fmt.Println()
		fmt.Println("the script is about to delete the following files/directories from the copy " +
			"of the \"" + res_dir_name + "\" directory:")
		for _, path := range directories_to_delete_from_res {
			fmt.Println(path)
		}
		for _, path := range files_to_delete_from_res {
			fmt.Println(path)
		}
		fmt.Println()

		// Ask for confirmation.
		yes2, err := ask_user("continue? (y/n)")
		if err != nil {
			fmt.Println(log_prefix, "failed to receive user input, error:", err)
			os.Exit(1)
		}
		if !yes2 {
			return
		}

		// Delete.
		for _, path := range directories_to_delete_from_res {
			remove_dir(path)
		}
		for _, path := range files_to_delete_from_res {
			remove_file(path)
		}
	}

	// Remove "reminder" file.
	var reminder_file_path = filepath.Join(path_to_binary_dir, res_copy_reminder_file_name)
	_, err = os.Stat(reminder_file_path)
	if err != nil {
		remove_file(reminder_file_path)
	}

	// Remove non-game related files/directories.
	remove_dir(filepath.Join(path_to_binary_dir, "CMakeFiles"))
	remove_dir(filepath.Join(path_to_binary_dir, "dep"))
	remove_file(filepath.Join(path_to_binary_dir, "cmake_install.cmake"))
	remove_files_that_ends_with(path_to_binary_dir, "exe.manifest")
	remove_files_that_ends_with(path_to_binary_dir, ".pdb")

	fmt.Println()
	fmt.Println(log_prefix, "you can now remove the directory with this script as it's no longer needed")
}

func remove_dir(path_to_dir string) {
	var dirname = filepath.Base(path_to_dir)

	var err = os.RemoveAll(path_to_dir)
	if err != nil {
		fmt.Println(log_prefix, "failed to remove the directory", path_to_dir, "error:", err)
		os.Exit(1)
	}
	fmt.Println(log_prefix, "removed directory \""+dirname+"\"")
}

func remove_file(path_to_file string) {
	var filename = filepath.Base(path_to_file)

	var err = os.Remove(path_to_file)
	if err != nil {
		fmt.Println(log_prefix, "failed to remove the file", path_to_file, "error:", err)
		os.Exit(1)
	}
	fmt.Println(log_prefix, "removed file \""+filename+"\"")
}

func remove_files_that_ends_with(path_to_binary_dir string, file_suffix string) {
	// Get all files/directories from the binary directory.
	items, err := os.ReadDir(path_to_binary_dir)
	if err != nil {
		fmt.Println(log_prefix, "failed to get directory items, error:", err)
		os.Exit(1)
	}

	// Iterate over all files/directories.
	for _, item := range items {
		// Skip directories.
		if item.IsDir() {
			continue
		}

		// Check suffix.
		var file_name = item.Name()
		if !strings.HasSuffix(file_name, file_suffix) {
			continue
		}

		// Remove file.
		remove_file(filepath.Join(path_to_binary_dir, file_name))
	}
}

// Returns `error` if an error occurred.
// Returns 'true' if the user answered 'yes'.
// Returns 'false' if the user answered 'no'.
func ask_user(question string) (bool, error) {
	for {
		fmt.Println(question)

		var reader = bufio.NewReader(os.Stdin)
		var text, err = reader.ReadString('\n')
		if err != nil {
			return false, err
		}

		if runtime.GOOS == "windows" {
			text = strings.TrimSuffix(text, "\r\n")
		} else {
			text = strings.TrimSuffix(text, "\n")
		}

		text = strings.ToLower(text)
		if text == "y" || text == "yes" {
			return true, nil
		}

		if text == "n" || text == "no" {
			return false, nil
		}

		fmt.Println(log_prefix, text, "is not a valid input. Please, provide a valid input.")
	}
}
