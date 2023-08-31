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
	"strings"

	"github.com/codeclysm/extract/v3"
	"github.com/codeskyblue/go-sh"
	"github.com/pelletier/go-toml/v2"
)

var log_prefix = "download_and_setup_refureku.go:"

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

// Expects the following arguments:
// 1. Working directory (the directory where this script is located).
// 2. Root directory where your source code is located (all subdirectories will be recursively scanned).
// 3. Optional (can be empty). Absolute path to the directory with generated source code of the cmake target that's part
// of the engine that your target depends on. For example if your `game_lib` target depends on `engine_lib` target then
// you need to specify path to the "generated" directory of the `engine_lib`. This will result in all included directories,
// of the target you depend on, to be appended to your specified included directories so that Refureku can find all includes.
// 4. Array of included directories that the project uses separated by the pipe "|" character. This array must also include
// all included directories of external dependencies (excluding engine-related targets).
// 5. Array of files located in the source code directory (or subdirectory) to exclude from analyzing.
// 6. Used compiler ID (from CMake).
func main() {
	var args_count = len(os.Args[1:])
	if args_count < 6 {
		fmt.Println(log_prefix, "not enough arguments.")
		os.Exit(1)
	}

	// Save command line arguments.
	var working_directory = os.Args[1]
	var src_dir = os.Args[2]
	var depends_generated_dir = os.Args[3]
	var parsed_include_directories = strings.Split(os.Args[4], "|")
	var exclude_files = strings.Split(os.Args[5], "|")
	var compiler_id = os.Args[6]

	// Remove empty elements from parsed includes
	var include_directories = []string{}
	for i := 0; i < len(parsed_include_directories); i++ {
		if parsed_include_directories[i] != "" {
			include_directories = append(include_directories, parsed_include_directories[i])
		}
	}

	// Make sure all included directories exist.
	for i := 0; i < len(include_directories); i++ {
		var _, err = os.Stat(include_directories[i])
		if os.IsNotExist(err) {
			fmt.Println(log_prefix, "the specified included directory", include_directories[i], "does not exist")
			os.Exit(1)
		}
	}

	// Change this to update used Refureku version.
	var refureku_version_tag = "v2.3.0f"
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
		depends_generated_dir,
		compiler_id)
}

func get_archive_name(archive_url string) string {
	return archive_url[strings.LastIndex(archive_url, "/"):]
}

func download_refureku_build(working_directory string, URL string, unzip_dir string) {
	// Check that working directory path exists.
	var _, err = os.Stat(working_directory)
	if os.IsNotExist(err) {
		fmt.Println(log_prefix, "the specified path to the script file",
			working_directory, "does not exist")
		os.Exit(1)
	}

	// See if up to date build is already downloaded and exit early.
	var archive_path = filepath.Join(working_directory, get_archive_name(URL))
	_, err = os.Stat(archive_path)
	if err == nil {
		// Exists.
		fmt.Println(log_prefix, "found already downloaded Refureku build at",
			archive_path, "- nothing to download")
		return
	}

	// No build found. See if there are any .zip/tar.gz files and remove them.
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

	fmt.Println(log_prefix, "downloading archived build to", archive_path)

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

	file, err := os.Create(archive_path)
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
			fmt.Println(log_prefix, "failed to remove old Refureku build, error:", err)
			os.Exit(1)
		}
	}
}

func unzip(src string, dest string) {
	var archive, err = os.Open(src)
	if err != nil {
		fmt.Println(log_prefix, "failed to read archive file, error:", err)
		os.Exit(1)
	}

	if strings.HasSuffix(src, ".zip") {
		ctx := context.TODO()
		extract.Zip(ctx, archive, dest, nil)
	} else if strings.HasSuffix(src, "tar.gz") {
		ctx := context.TODO()
		extract.Gz(ctx, archive, dest, nil)
	} else {
		fmt.Println(log_prefix, "unknown archive extension", src)
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
			fmt.Println(log_prefix, "Refureku generator does not exist at",
				refureku_generator_path)
			os.Exit(1)
		}

		// Allow executing the generator.
		var err = session.Command("chmod", "+x", refureku_generator_path).Run()
		if err != nil {
			fmt.Println(log_prefix, "failed to add 'execute' permission on file",
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
	depends_generated_dir string,
	compiler_id string) {
	// Check that template file exists.
	var _, err = os.Stat(template_settings_file_path)
	if os.IsNotExist(err) {
		fmt.Println(log_prefix, "the specified template Refureku settings file",
			template_settings_file_path, "does not exist")
		os.Exit(1)
	}

	// Check that directory with source code exists.
	_, err = os.Stat(src_directory)
	if os.IsNotExist(err) {
		fmt.Println(log_prefix, "the specified source files directory",
			src_directory, "does not exist")
		os.Exit(1)
	}

	var refureku_config_file_name = "RefurekuSettings.toml"

	if depends_generated_dir != "" {
		// Append includes of depends target.
		var depends_settings_file = filepath.Join(depends_generated_dir, refureku_config_file_name)
		_, err = os.Stat(depends_settings_file)
		if os.IsNotExist(err) {
			fmt.Println(log_prefix, "the specified Refureku config file of the directory you depend on",
				depends_settings_file, "does not exist")
			os.Exit(1)
		}
		include_directories = append(include_directories, get_included_directories_of_depends_target(depends_settings_file)[:]...)
	}

	var out_settings_file = filepath.Join(src_directory, ".generated", refureku_config_file_name)

	// Don't check if settings file already exists to skip this step.
	// Otherwrite with new settings (maybe this script changed due to git updates).

	in_file, err := os.Open(template_settings_file_path)
	if err != nil {
		fmt.Println(log_prefix, "failed to open Refureku template "+
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
			fmt.Println(log_prefix, "error while reading Refureku template "+
				"settings file - StrictMissingError:", details.String())
		} else {
			fmt.Println(log_prefix, "error while reading Refureku template "+
				"settings file:", err)
		}

		os.Exit(1)
	}

	// Prepare variables for config.
	var generated_dir_path = filepath.Join(src_directory, ".generated")

	// Configure Refureku settings.
	cfg.CodeGenManagerSettings.ToProcessDirectories = []string{src_directory}
	cfg.CodeGenManagerSettings.IgnoredDirectories = []string{generated_dir_path}
	cfg.CodeGenManagerSettings.IgnoredFiles = exclude_files
	cfg.CodeGenManagerSettings.ToProcessFiles = []string{}
	cfg.CodeGenUnitSettings.OutputDirectory = generated_dir_path
	cfg.CodeGenUnitSettings.GeneratedHeaderFileNamePattern = "##FILENAME##.generated.h"
	cfg.CodeGenUnitSettings.GeneratedSourceFileNamePattern = "##FILENAME##.generated_impl.h"
	cfg.ParsingSettings.ProjectIncludeDirectories = include_directories
	cfg.ParsingSettings.CompilerExeName = "clang++"
	cfg.ParsingSettings.AdditionalClangArguments = "-Wno-ignored-attributes"
	cfg.ParsingSettings.CppVersion = 20
	cfg.ParsingSettings.ShouldFailCodeGenerationOnClangErrors = true
	// WARNING: if changing macro names also change them in Doxyfile in PREDEFINED section
	cfg.ParsingSettings.NamespaceMacroName = "RNAMESPACE"
	cfg.ParsingSettings.ClassMacroName = "RCLASS"
	cfg.ParsingSettings.StructMacroName = "RSTRUCT"
	cfg.ParsingSettings.VariableMacroName = "RGLOBALVARIABLE"
	cfg.ParsingSettings.FieldMacroName = "RPROPERTY"
	cfg.ParsingSettings.FunctionMacroName = "RGLOBALFUNCTION"
	cfg.ParsingSettings.MethodMacroName = "RFUNCTION"
	cfg.ParsingSettings.EnumMacroName = "RENUM"
	cfg.ParsingSettings.EnumValueMacroName = "RENUMVALUE"

	// Save new configuration.
	out_file, err := os.Create(out_settings_file)
	if err != nil {
		fmt.Println(log_prefix, "failed to create new Refureku settings "+
			"file:", err)
		os.Exit(1)
	}
	bytes, err := toml.Marshal(cfg)
	if err != nil {
		fmt.Println(log_prefix, err)
		os.Exit(1)
	}
	_, err = out_file.Write(bytes)
	if err != nil {
		fmt.Println(log_prefix, "failed to write to new Refureku settings "+
			"file:", err)
		os.Exit(1)
	}
	out_file.Close()
}

func get_included_directories_of_depends_target(path_to_refureku_settings string) []string {
	in_file, err := os.Open(path_to_refureku_settings)
	if err != nil {
		fmt.Println(log_prefix, "failed to open Refureku "+
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
			fmt.Println(log_prefix, "error while reading Refureku "+
				"settings file - StrictMissingError:", details.String())
		} else {
			fmt.Println(log_prefix, "error while reading Refureku "+
				"settings file:", err)
		}

		os.Exit(1)
	}

	return cfg.ParsingSettings.ProjectIncludeDirectories
}
