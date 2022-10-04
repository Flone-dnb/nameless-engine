package main

import (
	"context"
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

	"github.com/codeclysm/extract/v3"
	"github.com/codeskyblue/go-sh"
	"github.com/pelletier/go-toml/v2"
)

// Expects the following arguments:
// 1. Working directory (the directory where this script is located).
// 2. Directory where source code is located.
// 3. Array of included directories that the project uses separated by pipe ('|') sign.
// 4. Array of files to exclude from analyzing.
// 5. Used compiler ID (from CMake).
// 6. Used C++ standard (only number, for example: 20, 23).
func main() {
	var args_count = len(os.Args[1:])
	if args_count < 6 {
		fmt.Println("ERROR: download_and_setup_refureku.go: not enough arguments.")
		os.Exit(1)
	}

	fmt.Println("Setting up Refureku...")

	// Save command line arguments.
	var working_directory = os.Args[1]
	var src_dir = os.Args[2]
	var include_directories = strings.Split(os.Args[3], "|")
	var exclude_files = strings.Split(os.Args[4], "|")
	var compiler_id = os.Args[5]
	var cpp_standard_number = os.Args[6]

	// Change this to update used Refureku version.
	var refureku_version_tag = "v2.3.0b"
	var archive_url = ""
	var base_archive_url = "https://github.com/Flone-dnb/Refureku/releases/download/"
	if runtime.GOOS == "windows" {
		archive_url = base_archive_url + refureku_version_tag + "/rfk_" +
			refureku_version_tag + "_windows.zip"
	} else if runtime.GOOS == "linux" {
		archive_url = base_archive_url + refureku_version_tag + "/rfk_" +
			refureku_version_tag + "_linux.tar.gz"
	} else if runtime.GOOS == "macos" {
		archive_url = base_archive_url + refureku_version_tag + "/rfk_" +
			refureku_version_tag + "_macos.tar.gz"
	}

	// Setup directory "build" to unzip archive there.
	var unzip_dir = filepath.Join(working_directory, "build")
	download_refureku_build(working_directory, archive_url, unzip_dir)
	initialize_refureku_settings(
		filepath.Join(unzip_dir, "RefurekuSettings.toml"),
		src_dir,
		include_directories,
		exclude_files,
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
	var archive, err = os.Open(src)
	if err != nil {
		fmt.Println("ERROR: download_and_setup_refureku.go: failed to read archive file, error:", err)
		os.Exit(1)
	}

	if strings.HasSuffix(src, ".zip") {
		ctx := context.TODO()
		extract.Zip(ctx, archive, dest, nil)
	} else if strings.HasSuffix(src, "tar.gz") {
		ctx := context.TODO()
		extract.Gz(ctx, archive, dest, nil)
	} else {
		fmt.Println("ERROR: download_and_setup_refureku.go: unknown archive extension", src)
		os.Exit(1)
	}

	if runtime.GOOS == "linux" {
		var session = sh.NewSession()
		session.PipeFail = true
		session.PipeStdErrors = true

		// Check that generator exists.
		var refureku_generator_path = filepath.Join(dest, "Bin", "RefurekuGenerator")
		_, err = os.Stat(refureku_generator_path)
		if os.IsNotExist(err) {
			fmt.Println("ERROR: download_and_setup_refureku.go: Refureku generator does not exist at",
				refureku_generator_path)
			os.Exit(1)
		}

		// Allow executing the generator.
		var err = session.Command("chmod", "+x", refureku_generator_path).Run()
		if err != nil {
			fmt.Println("ERROR: download_and_setup_refureku.go: failed to add 'execute' permission on file",
				refureku_generator_path)
			os.Exit(1)
		}
	}
}

func initialize_refureku_settings(
	template_settings_file_path string,
	src_directory string,
	include_directories []string,
	exclude_files []string,
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

	var out_settings_file = filepath.Join(src_directory, ".reflection", "RefurekuSettings.toml")
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
			CppVersion                            int      `toml:"cppVersion"`
			ProjectIncludeDirectories             []string `toml:"projectIncludeDirectories"`
			CompilerExeName                       string   `toml:"compilerExeName"`
			AdditionalClangArguments              string   `toml:"additionalClangArguments"`
			ShouldFailCodeGenerationOnClangErrors bool     `toml:"shouldFailCodeGenerationOnClangErrors"`
			ShouldAbortParsingOnFirstError        bool     `toml:"shouldAbortParsingOnFirstError"`
			ShouldParseAllNamespaces              bool     `toml:"shouldParseAllNamespaces"`
			ShouldParseAllClasses                 bool     `toml:"shouldParseAllClasses"`
			ShouldParseAllStructs                 bool     `toml:"shouldParseAllStructs"`
			ShouldParseAllVariables               bool     `toml:"shouldParseAllVariables"`
			ShouldParseAllFields                  bool     `toml:"shouldParseAllFields"`
			ShouldParseAllFunctions               bool     `toml:"shouldParseAllFunctions"`
			ShouldParseAllMethods                 bool     `toml:"shouldParseAllMethods"`
			ShouldParseAllEnums                   bool     `toml:"shouldParseAllEnums"`
			ShouldParseAllEnumValues              bool     `toml:"shouldParseAllEnumValues"`
			ShouldLogDiagnostic                   bool     `toml:"shouldLogDiagnostic"`
			PropertySeparator                     string   `toml:"propertySeparator"`
			SubPropertySeparator                  string   `toml:"subPropertySeparator"`
			SubPropertyStartEncloser              string   `toml:"subPropertyStartEncloser"`
			SubPropertyEndEncloser                string   `toml:"subPropertyEndEncloser"`
			NamespaceMacroName                    string   `toml:"namespaceMacroName"`
			ClassMacroName                        string   `toml:"classMacroName"`
			StructMacroName                       string   `toml:"structMacroName"`
			VariableMacroName                     string   `toml:"variableMacroName"`
			FieldMacroName                        string   `toml:"fieldMacroName"`
			FunctionMacroName                     string   `toml:"functionMacroName"`
			MethodMacroName                       string   `toml:"methodMacroName"`
			EnumMacroName                         string   `toml:"enumMacroName"`
			EnumValueMacroName                    string   `toml:"enumValueMacroName"`
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
	var reflection_dir_path = filepath.Join(src_directory, ".reflection")

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
	cfg.CodeGenManagerSettings.IgnoredDirectories = []string{generated_dir_path, reflection_dir_path}
	cfg.CodeGenManagerSettings.IgnoredFiles = exclude_files
	cfg.CodeGenUnitSettings.OutputDirectory = generated_dir_path
	cfg.CodeGenUnitSettings.GeneratedHeaderFileNamePattern = "##FILENAME##.generated.h"
	cfg.CodeGenUnitSettings.GeneratedSourceFileNamePattern = "##FILENAME##.generated_impl.h"
	cfg.ParsingSettings.ProjectIncludeDirectories = include_directories
	cfg.ParsingSettings.CompilerExeName = compiler_binary_name
	cfg.ParsingSettings.AdditionalClangArguments = "-Wno-ignored-attributes"
	cfg.ParsingSettings.CppVersion = cpp_standard
	cfg.ParsingSettings.ShouldFailCodeGenerationOnClangErrors = true
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
