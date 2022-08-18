package main

import (
	"archive/zip"
	"errors"
	"fmt"
	"io"
	"io/ioutil"
	"net/http"
	"os"
	"path/filepath"
	"runtime"
	"strconv"
	"strings"

	"github.com/pelletier/go-toml/v2"
)

// Expects the following arguments:
// 1. Working directory (the directory where this script is located).
// 2. Directory where source code is located.
// 3. Array of included directories that the project uses separated by pipe ('|') sign.
// 4. Used compiler ID (from CMake).
// 5. Used C++ standard (only number, for example: 20, 23).
func main() {
	var args_count = len(os.Args[1:])
	if args_count < 5 {
		fmt.Println("ERROR: download_and_setup_refureku.go: not enough arguments.")
		os.Exit(1)
	}

	// Save command line arguments.
	var working_directory = os.Args[1]
	var src_dir = os.Args[2]
	var include_directories = strings.Split(os.Args[3], "|")
	var compiler_id = os.Args[4]
	var cpp_standard_number = os.Args[5]

	// INFO: change this to update used Refureku version.
	var refureku_version_tag = "v2.2.0"
	var archive_url = ""
	if runtime.GOOS == "windows" {
		archive_url = "https://github.com/jsoysouvanh/Refureku/releases/download/" +
			refureku_version_tag + "/rfk_" + refureku_version_tag + "_windows.zip"
	} else if runtime.GOOS == "linux" {
		// TODO: there is a different archive (tar)
		fmt.Println("ERROR: download_and_setup_refureku.go: not implemented for Linux.")
		os.Exit(1)
	} else if runtime.GOOS == "macos" {
		// TODO: there is a different archive (tar)
		fmt.Println("ERROR: download_and_setup_refureku.go: not implemented for MacOS.")
		os.Exit(1)
	}

	// Setup directory "build" to unzip archive there.
	var unzip_dir = filepath.Join(working_directory, "build")
	download_refureku_build(working_directory, archive_url, unzip_dir)
	initialize_refureku_settings(
		filepath.Join(unzip_dir, "RefurekuSettings.toml"),
		src_dir,
		include_directories,
		compiler_id,
		cpp_standard_number)
}

func get_archive_name(archive_url string) string {
	return archive_url[strings.LastIndex(archive_url, "/"):]
}

func download_refureku_build(working_directory string, URL string, unzip_dir string) {
	var archive_path = filepath.Join(working_directory, get_archive_name(URL))

	var _, err = os.Stat(archive_path)
	if err == nil {
		// Exists.
		fmt.Println("INFO: download_and_setup_refureku.go: found already downloaded Refureku build at",
			archive_path, "- nothing to download")
		return
	}

	// Not found. See if there are any .zip/tar.gz files and remove them.
	items, _ := ioutil.ReadDir(working_directory)
	for _, item := range items {
		if item.IsDir() {
			continue
		} else {
			if strings.HasSuffix(item.Name(), ".zip") || strings.HasSuffix(item.Name(), ".tar.gz") {
				os.Remove(filepath.Join(working_directory, item.Name()))
			}
		}
	}

	fmt.Println("INFO: download_and_setup_refureku.go: downloading archived build to", archive_path)

	response, err := http.Get(URL)
	if err != nil {
		fmt.Println("ERROR: download_and_setup_refureku.go:", err)
		os.Exit(1)
	}
	defer response.Body.Close()

	if response.StatusCode != 200 {
		fmt.Println("ERROR: download_and_setup_refureku.go: received non 200 response code, actual result:", response.StatusCode)
		os.Exit(1)
	}

	file, err := os.Create(archive_path)
	if err != nil {
		fmt.Println("ERROR: download_and_setup_refureku.go: failed to create empty file, error:", err)
		os.Exit(1)
	}
	defer file.Close()

	_, err = io.Copy(file, response.Body)
	if err != nil {
		fmt.Println("ERROR: download_and_setup_refureku.go: failed to copy downloaded bytes, error:", err)
		os.Exit(1)
	}

	remove_old_refureku_build(working_directory)
	unzip(archive_path, unzip_dir)
}

func remove_old_refureku_build(working_directory string) {
	var current_path = filepath.Join(working_directory, "build")
	var _, err = os.Stat(current_path)
	if err == nil {
		// Exists.
		err = os.RemoveAll(current_path)
		if err != nil {
			fmt.Println("ERROR: download_and_setup_refureku.go: failed to remove old Refureku build, error:", err)
			os.Exit(1)
		}
	}
}

func unzip(src string, dest string) {
	r, err := zip.OpenReader(src)
	if err != nil {
		fmt.Println("ERROR: download_and_setup_refureku.go: open zip reader, error:", err)
		os.Exit(1)
	}
	defer func() {
		if err := r.Close(); err != nil {
			fmt.Println("ERROR: download_and_setup_refureku.go: error:", err)
			os.Exit(1)
		}
	}()

	os.MkdirAll(dest, 0755)

	// Closure to address file descriptors issue with all the deferred .Close() methods
	extractAndWriteFile := func(f *zip.File) {
		rc, err := f.Open()
		if err != nil {
			fmt.Println("ERROR: download_and_setup_refureku.go: error:", err)
			os.Exit(1)
		}
		defer func() {
			if err := rc.Close(); err != nil {
				fmt.Println("ERROR: download_and_setup_refureku.go: error:", err)
				os.Exit(1)
			}
		}()

		path := filepath.Join(dest, f.Name)

		// Check for ZipSlip (Directory traversal)
		if !strings.HasPrefix(path, filepath.Clean(dest)+string(os.PathSeparator)) {
			fmt.Println("ERROR: download_and_setup_refureku.go: illegal file path:", path)
			os.Exit(1)
		}

		if f.FileInfo().IsDir() {
			os.MkdirAll(path, f.Mode())
		} else {
			os.MkdirAll(filepath.Dir(path), f.Mode())
			f, err := os.OpenFile(path, os.O_WRONLY|os.O_CREATE|os.O_TRUNC, f.Mode())
			if err != nil {
				fmt.Println("ERROR: download_and_setup_refureku.go: error:", err)
				os.Exit(1)
			}
			defer func() {
				if err := f.Close(); err != nil {
					fmt.Println("ERROR: download_and_setup_refureku.go: error:", err)
					os.Exit(1)
				}
			}()

			_, err = io.Copy(f, rc)
			if err != nil {
				fmt.Println("ERROR: download_and_setup_refureku.go: error:", err)
				os.Exit(1)
			}
		}
	}

	for _, f := range r.File {
		extractAndWriteFile(f)
	}
}

func initialize_refureku_settings(
	template_settings_file_path string,
	src_directory string,
	include_directories []string,
	compiler_id string,
	cpp_standard_number string) {
	// Check that template file exists.
	var _, err = os.Stat(template_settings_file_path)
	if os.IsNotExist(err) {
		fmt.Println("ERROR: download_and_setup_refureku.go: the specified template Refureku settings file",
			template_settings_file_path, "does not exist")
		os.Exit(1)
	}

	// Check that output directory exists
	_, err = os.Stat(template_settings_file_path)
	if os.IsNotExist(err) {
		fmt.Println("ERROR: download_and_setup_refureku.go: the specified output directory",
			src_directory, "does not exist")
		os.Exit(1)
	}

	var out_settings_file = filepath.Join(src_directory, "RefurekuSettings.toml")
	// Don't check if settings file already exists to skip this step.
	// Otherwrite with new settings (maybe this script changed due to git updates).

	type RefurekuSettings struct {
		CodeGenManagerSettings struct {
			SupportedFileExtensions []string `toml:"supportedFileExtensions"`
			ToProcessDirectories    []string `toml:"toProcessDirectories"`
			ToProcessFiles          []string `toml:"toProcessFiles"`
			IgnoredDirectories      []string `toml:"ignoredDirectories"`
			IgnoredFiles            []string `toml:"ignoredFiles"`
		}
		CodeGenUnitSettings struct {
			OutputDirectory                string `toml:"outputDirectory"`
			GeneratedHeaderFileNamePattern string `toml:"generatedHeaderFileNamePattern"`
			GeneratedSourceFileNamePattern string `toml:"generatedSourceFileNamePattern"`
			ClassFooterMacroPattern        string `toml:"classFooterMacroPattern"`
			HeaderFileFooterMacroPattern   string `toml:"headerFileFooterMacroPattern"`
		}
		ParsingSettings struct {
			CppVersion                     int      `toml:"cppVersion"`
			ProjectIncludeDirectories      []string `toml:"projectIncludeDirectories"`
			CompilerExeName                string   `toml:"compilerExeName"`
			ShouldAbortParsingOnFirstError bool     `toml:"shouldAbortParsingOnFirstError"`
			ShouldParseAllNamespaces       bool     `toml:"shouldParseAllNamespaces"`
			ShouldParseAllClasses          bool     `toml:"shouldParseAllClasses"`
			ShouldParseAllStructs          bool     `toml:"shouldParseAllStructs"`
			ShouldParseAllVariables        bool     `toml:"shouldParseAllVariables"`
			ShouldParseAllFields           bool     `toml:"shouldParseAllFields"`
			ShouldParseAllFunctions        bool     `toml:"shouldParseAllFunctions"`
			ShouldParseAllMethods          bool     `toml:"shouldParseAllMethods"`
			ShouldParseAllEnums            bool     `toml:"shouldParseAllEnums"`
			ShouldParseAllEnumValues       bool     `toml:"shouldParseAllEnumValues"`
			ShouldLogDiagnostic            bool     `toml:"shouldLogDiagnostic"`
			PropertySeparator              string   `toml:"propertySeparator"`
			SubPropertySeparator           string   `toml:"subPropertySeparator"`
			SubPropertyStartEncloser       string   `toml:"subPropertyStartEncloser"`
			SubPropertyEndEncloser         string   `toml:"subPropertyEndEncloser"`
			NamespaceMacroName             string   `toml:"namespaceMacroName"`
			ClassMacroName                 string   `toml:"classMacroName"`
			StructMacroName                string   `toml:"structMacroName"`
			VariableMacroName              string   `toml:"variableMacroName"`
			FieldMacroName                 string   `toml:"fieldMacroName"`
			FunctionMacroName              string   `toml:"functionMacroName"`
			MethodMacroName                string   `toml:"methodMacroName"`
			EnumMacroName                  string   `toml:"enumMacroName"`
			EnumValueMacroName             string   `toml:"enumValueMacroName"`
		}
	}

	in_file, err := os.Open(template_settings_file_path)
	if err != nil {
		fmt.Println("ERROR: download_and_setup_refureku.go: failed to open Refureku template "+
			"settings file:", err)
		os.Exit(1)
	}

	d := toml.NewDecoder(in_file)
	d.DisallowUnknownFields()

	var cfg RefurekuSettings
	err = d.Decode(&cfg)
	if err != nil {
		var details *toml.StrictMissingError
		if errors.As(err, &details) {
			fmt.Println("ERROR: download_and_setup_refureku.go: error while reading Refureku template "+
				"settings file - StrictMissingError:", details.String())
		} else {
			fmt.Println("ERROR: download_and_setup_refureku.go: error while reading Refureku template "+
				"settings file:", err)
		}

		os.Exit(1)
	}

	// Prepare variables for config.
	var generated_dir_path = filepath.Join(src_directory, ".generated")

	compiler_id = strings.ToLower(compiler_id)
	var compiler_binary_name = ""
	if strings.Contains(compiler_id, "msvc") {
		compiler_binary_name = "msvc"
	} else if strings.Contains(compiler_id, "clang") {
		compiler_binary_name = "clang++"
	} else if strings.Contains(compiler_id, "gnu") {
		compiler_binary_name = "g++"
	} else {
		fmt.Println("ERROR: download_and_setup_refureku.go: unknown compiler name", compiler_id)
		os.Exit(1)
	}
	cpp_standard, err := strconv.Atoi(cpp_standard_number)
	if err != nil {
		fmt.Println("ERROR: download_and_setup_refureku.go: failed to convert the specified "+
			"C++ standard", cpp_standard_number, "to integer:", err)
		os.Exit(1)
	}
	if cpp_standard > 20 {
		cpp_standard = 20 // clamp otherwise Refureku complains
	}

	// Configure Refureku settings.
	cfg.CodeGenManagerSettings.ToProcessDirectories = []string{src_directory}
	cfg.CodeGenManagerSettings.IgnoredDirectories = []string{generated_dir_path}
	cfg.CodeGenUnitSettings.OutputDirectory = generated_dir_path
	cfg.CodeGenUnitSettings.GeneratedHeaderFileNamePattern = "##FILENAME##.generated.h"
	cfg.CodeGenUnitSettings.GeneratedSourceFileNamePattern = "##FILENAME##.generated_impl.h"
	cfg.ParsingSettings.ProjectIncludeDirectories = include_directories
	cfg.ParsingSettings.CompilerExeName = compiler_binary_name
	cfg.ParsingSettings.CppVersion = cpp_standard
	// WARNING: if changing macro names also change them in Doxyfile in PREDEFINED section
	cfg.ParsingSettings.NamespaceMacroName = "NENAMESPACE"
	cfg.ParsingSettings.ClassMacroName = "NECLASS"
	cfg.ParsingSettings.StructMacroName = "NESTRUCT"
	cfg.ParsingSettings.VariableMacroName = "NEGLOBALVARIABLE"
	cfg.ParsingSettings.FieldMacroName = "NEPROPERTY"
	cfg.ParsingSettings.FunctionMacroName = "NEGLOBALFUNCTION"
	cfg.ParsingSettings.MethodMacroName = "NEFUNCTION"
	cfg.ParsingSettings.EnumMacroName = "NEENUM"
	cfg.ParsingSettings.EnumValueMacroName = "NEENUMVALUE"

	// Save new configuration.
	out_file, err := os.Create(out_settings_file)
	if err != nil {
		fmt.Println("ERROR: download_and_setup_refureku.go: failed to create new Refureku settings "+
			"file:", err)
		os.Exit(1)
	}
	bytes, err := toml.Marshal(cfg)
	if err != nil {
		fmt.Println("ERROR: download_and_setup_refureku.go:", err)
		os.Exit(1)
	}
	_, err = out_file.Write(bytes)
	if err != nil {
		fmt.Println("ERROR: download_and_setup_refureku.go: failed to write to new Refureku settings "+
			"file:", err)
		os.Exit(1)
	}
	out_file.Close()
}
