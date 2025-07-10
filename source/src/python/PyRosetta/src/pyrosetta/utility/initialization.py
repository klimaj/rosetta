from __future__ import absolute_import
# :noTabs=true:
#
# (c) Copyright Rosetta Commons Member Institutions.
# (c) This file is part of the Rosetta software suite and is made available under license.
# (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
# (c) For more information, see http://www.rosettacommons.org.
# (c) Questions about this can be addressed to University of Washington CoMotion, email: license@uw.edu.

###############################################################################
# PyRosetta initialization files


__author__ = "Jason C. Klima"


import base64
import collections
import datetime
import inspect
import json
import os
import pickle
import pyrosetta
import warnings

from pprint import pprint
from pyrosetta.rosetta.core.simple_metrics import get_sm_data
from pyrosetta.rosetta.protocols.rosetta_scripts import XmlObjects


class PyRosettaInitFileParserBase(object):
    _database_option_name = "in:path:database"
    _init_file_extension = ".init"
    _prefix_string = "[PyRosettaInitStringFile]"
    _prefix_binary = "[PyRosettaInitBinaryFile]"
    _strftime_format = "%Y-%m-%d-%H-%M-%S"

    def get_pyrosetta_build(self):
        return pyrosetta._version_string()


class PyRosettaInitFileWriter(PyRosettaInitFileParserBase):
    def __init__(self, output_filename, **kwargs):
        self.validate_init_was_called()
        self.kwargs = self.setup_kwargs(**kwargs)
        self.output_filename = self.setup_output_filename(output_filename)
        self.encoded_options_dict = self.get_encoded_options_dict()

    def setup_output_filename(self, output_filename):
        if not isinstance(output_filename, str):
            raise TypeError(
                "Output file must be a `str` object. Received: {0}".format(type(output_filename))
            )
        if not output_filename.endswith(".init"):
            raise NameError(
                "Output file must end with the '{0}' filename extension.".format(self._init_file_extension)
            )
        if os.path.isfile(output_filename) and not self.kwargs["overwrite"]:
            raise FileExistsError(
                "Output '{0}' file already exists! Please remove the file and try again: {1}".format(
                    self._init_file_extension, output_filename
                )
            )
        return output_filename

    def setup_kwargs(self, **kwargs):
        for key in ("author", "email", "license"):
            if key in kwargs and kwargs[key] is None:
                kwargs[key] = ""
            elif not isinstance(kwargs[key], str):
                raise TypeError(
                    "The '{0}' keyword argument parameter must be a `str` object. Received: {1}".format(
                        key, type(kwargs[key])
                    )
                )
        if "metadata" in kwargs and kwargs["metadata"] is None:
            kwargs["metadata"] = {}
        else:
            self.assert_metadata_json_serializable(kwargs["metadata"])
        kwargs["pyrosetta_build"] = self.get_pyrosetta_build()
        kwargs["datetime"] = self.get_datetime_now()
        if "overwrite" in kwargs and kwargs["overwrite"] is None:
            kwargs["overwrite"] = False
        elif not isinstance(kwargs["overwrite"], bool):
            raise TypeError(
                "The 'overwrite' keyword argument parameter must be a `bool` object. Received: {1}".format(type(kwargs["overwrite"]))
            )
        return kwargs

    def assert_metadata_json_serializable(self, data):
        try:
            json.dumps(data)
        except:
            raise TypeError("Input 'metadata' keyword argument parameter must be JSON-serializable.")

    def get_datetime_now(self):
        return datetime.datetime.now(datetime.timezone.utc).strftime(self._strftime_format)

    def init_pose(self):
        return pyrosetta.Pose()

    def get_protocol_settings_metric(self):
        return XmlObjects().create_from_string(
        """
        <SIMPLE_METRICS>
        <ProtocolSettingsMetric
            name="protocol_settings"
            custom_type="{0}"
            base_name_only="0"
            get_user_options="1"
            get_script_vars="1"
            skip_corrections="0"/>
        </SIMPLE_METRICS>
        """.format(__class__.__name__)
        ).get_simple_metric("protocol_settings")
    
    def apply_protocol_settings_metric(self, pose):
        xml_obj = self.get_protocol_settings_metric()
        xml_obj.apply(pose)

        return pose

    def get_protocol_settings_dict(self, pose):
        sm_data = get_sm_data(pose)
        data = dict(sm_data.get_composite_string_metric_data())
        return dict(data["{0}_opt".format(__class__.__name__)])

    def get_options_dict(self):
        pose = self.init_pose()
        pose = self.apply_protocol_settings_metric(pose)
        return self.get_protocol_settings_dict(pose)

    def get_encoded_options_dict(self):
        options_dict = self.get_options_dict()
        encoded_options_dict = collections.defaultdict(list)
        for option_name, values in options_dict.items():
            if option_name != self._database_option_name:
                for value in values.split():
                    if os.path.isfile(value):
                        encoded_options_dict[option_name].append(self.encode_file(value))
                    elif os.path.isdir(value):
                        rel_value = os.path.relpath(value, start=os.curdir)
                        if value != rel_value:
                            warnings.warn(
                                "The option '-{0}' with path '{1}' is being saved as the relative path: '{2}'.".format(
                                    option_name, value, rel_value
                                ),
                                UserWarning,
                                stacklevel=2,
                            )
                        encoded_options_dict[option_name].append(rel_value)
                    else:
                        encoded_options_dict[option_name].append(value)
        return encoded_options_dict

    def is_file_containing_list_of_files(self, filename):
        try:
            with open(filename, "r") as f:
                for line in f:
                    return os.path.isfile(line.strip())
        except UnicodeDecodeError:
            return False

    def is_text_file(self, filename):
        try:
            with open(filename, "r") as f:
                for line in f:
                    return True
        except UnicodeDecodeError:
            return False

    def encode_bytestring(self, bytestring):
        return base64.b64encode(bytestring).decode()

    def encode_string(self, string):
        return self.encode_bytestring(pickle.dumps(string))

    def format_encode_bytestring(self, file_handle):
        return "{0}{1}".format(
            PyRosettaInitFileParserBase._prefix_binary,
            self.encode_bytestring(file_handle.read()),
        )

    def format_encode_string(self, file_handle):
        return "{0}{1}".format(
            PyRosettaInitFileParserBase._prefix_string,
            self.encode_string(file_handle.read()),
        )

    def encode_file(self, filename):
        results = {}
        if self.is_file_containing_list_of_files(filename):
            result = {}
            with open(filename, "r") as f1:
                for line in f1.read().splitlines():
                    if self.is_text_file(line):
                        with open(line, "r") as f2:
                            result[os.path.basename(line)] = self.format_encode_string(f2)
                    else:
                        with open(line, "rb") as f2:
                            result[os.path.basename(line)] = self.format_encode_bytestring(f2)
        elif self.is_text_file(filename):
            with open(filename, "r") as f:
                result = self.format_encode_string(f)
        else:
            with open(filename, "rb") as f:
                result = self.format_encode_bytestring(f)
        results[os.path.basename(filename)] = result
        return results

    def validate_init_was_called(self):
        if not pyrosetta.rosetta.basic.was_init_called():
            raise RuntimeError(
                "PyRosetta must be already initialized to dump an initialization file. "
                + "Please run `pyrosetta.init()` with custom options and try again."
            )

    def dump(self):
        overwrite = self.kwargs.pop("overwrite")
        data_dict = {
            **self.kwargs,
            "options": self.encoded_options_dict,
        }
        if (not os.path.isfile(self.output_filename)) or overwrite:
            with open(self.output_filename, "w") as f:
                json.dump(data_dict, f, indent=4)


class PyRosettaInitFileReader(PyRosettaInitFileParserBase):
    def __init__(self, init_file, **kwargs):
        self.validate_init_was_not_called()
        self.init_file = init_file
        self.init_dict = self.setup_init_dict(init_file)
        self.kwargs = self.setup_kwargs(**kwargs)
        self.file_counter = 0

    def setup_kwargs(self, **kwargs):
        if kwargs["dry_run"] is None:
            kwargs["dry_run"] = False
        if not isinstance(kwargs["dry_run"], bool):
            raise ValueError("The 'dry_run' keyword argument parameter must be a `bool` object.")
        if kwargs["output_dir"] is None:
            output_dir = os.path.join(os.getcwd(), "pyrosetta_init_files")
        elif isinstance(kwargs["output_dir"], str):
            output_dir = os.path.abspath(kwargs["output_dir"])
        else:
            raise TypeError("The 'output_dir' keyword argument parameter must be a `str` object.")
        if not os.path.isdir(output_dir):
            kwargs["output_dir"] = output_dir
        else:
            raise IOError(
                "The output directory already exists! Please remove the output directory and try again: {0}".format(output_dir)
            )
        if kwargs["database"] is None:
            kwargs["database"] = pyrosetta._rosetta_database_from_env()
        if not (isinstance(kwargs["database"], str) and os.path.isdir(kwargs["database"])):
            raise RuntimeError("PyRosetta database directory not found: {0}".format(kwargs["database"]))
        fullargspec = inspect.getfullargspec(pyrosetta.init)
        default_init_kwargs = dict(zip(fullargspec.args, fullargspec.defaults))
        if kwargs["set_logging_handler"] is None:
            kwargs["set_logging_handler"] = default_init_kwargs["set_logging_handler"]
        if kwargs["notebook"] is None:
            kwargs["notebook"] = default_init_kwargs["notebook"]
        if kwargs["silent"] is None:
            kwargs["silent"] = default_init_kwargs["silent"]
        return kwargs

    def setup_init_dict(self, init_file):
        if isinstance(init_file, str) and init_file.endswith(".init") and os.path.isfile(init_file):
            with open(self.init_file, "r") as f:
                return json.load(f)
        else:
            raise ValueError(
                "Please provide a valid input PyRosetta '.init' file. Received: {0}".format(init_file)
            )

    def validate_init_was_not_called(self):
        if pyrosetta.rosetta.basic.was_init_called():
            raise RuntimeError(
                "PyRosetta must not be already initialized to initialize from a file. "
                "Please ensure that `pyrosetta.init()` was not already called and try again."
            )

    def get_encoded_options_dict(self):
        encoded_options_dict = self.init_dict["options"]
        encoded_options_dict[self._database_option_name] = [self.kwargs["database"]]
        return encoded_options_dict

    def decode_binary(self, string):
        return base64.b64decode(string, validate=True)

    def decode_string(self, bytestring):
        return pickle.loads(self.decode_binary(bytestring))

    def format_decode_binary(self, value):
        return self.decode_binary(value.split(PyRosettaInitFileParserBase._prefix_binary)[-1])

    def format_decode_string(self, value):
        return self.decode_string(value.split(PyRosettaInitFileParserBase._prefix_string)[-1])

    def setup_new_file(self, option_name, basename):
        file = os.path.join(self.kwargs["output_dir"], option_name.replace(":", "_"), str(self.file_counter), basename)
        self.file_counter += 1
        if not self.kwargs["dry_run"]:
            os.makedirs(os.path.dirname(file), exist_ok=False)
        return file

    def write_file(self, option_name, basename, file_content, mode="w"):
        new_file = self.setup_new_file(option_name, basename)
        if not self.kwargs["dry_run"]:
            with open(new_file, mode) as f:
                f.write(file_content)
        return new_file

    def write_text_file(self, *args):
        return self.write_file(*args, mode="w")

    def write_binary_file(self, *args):
        return self.write_file(*args, mode="wb")

    def get_options_dict(self):
        encoded_options_dict = self.get_encoded_options_dict()
        options_dict = collections.defaultdict(list)
        for option_name, values in encoded_options_dict.items():
            for value in values:
                if isinstance(value, dict):
                    for basename, data in value.items():
                        if isinstance(data, str):
                            if data.startswith(PyRosettaInitFileParserBase._prefix_string):
                                file_content = self.format_decode_string(data)
                                filename = self.write_text_file(option_name, basename, file_content)
                                options_dict[option_name].append(filename)
                            elif data.startswith(PyRosettaInitFileParserBase._prefix_binary):
                                file_content = self.format_decode_binary(data)
                                filename = self.write_binary_file(option_name, basename, file_content)
                                options_dict[option_name].append(filename)
                            else:
                                options_dict[option_name].append(data)
                        elif isinstance(data, dict):
                            file_list = []
                            for subbasename, subdata in data.items():
                                assert isinstance(subdata, str), "Cannot read malformed initialization file: {0}".format(self.init_file)
                                if subdata.startswith(PyRosettaInitFileParserBase._prefix_string):
                                    file_content = self.format_decode_string(subdata)
                                    filename = self.write_text_file(option_name, subbasename, file_content)
                                    file_list.append(filename)
                                elif subdata.startswith(PyRosettaInitFileParserBase._prefix_binary):
                                    file_content = self.format_decode_binary(subdata)
                                    filename = self.write_binary_file(option_name, subbasename, file_content)
                                    file_list.append(filename)
                            file_content = os.linesep.join(file_list) + os.linesep
                            filename = self.write_text_file(option_name, basename, file_content)
                            options_dict[option_name].append(filename)
                elif isinstance(value, str):
                    options_dict[option_name].append(value)
                else:
                    raise RuntimeError("Cannot read malformed initialization file: {0}".format(self.init_file))
        return options_dict

    def get_options(self):
        options_dict = self.get_options_dict()
        return " ".join(
            [
                "-{0} {1}".format(option_name, " ".join(values))
                for option_name, values in options_dict.items()
            ]
        )

    def pyrosetta_build_warning(self):
        original_pyrosetta_build = self.init_dict["pyrosetta_build"]
        current_pyrosetta_build = self.get_pyrosetta_build()
        if original_pyrosetta_build != current_pyrosetta_build:
            _msg = os.linesep.join(
                [
                    "The PyRosetta version that generated the initialization file "
                    + "does not match the current PyRosetta version. Please inspect "
                    + "the input initialization files if you encounter any issues during "
                    + "or after PyRosetta initialization: {0}".format(self.kwargs["output_dir"]),
                    "Original: {0}".format(original_pyrosetta_build),
                    "Current:  {0}".format(current_pyrosetta_build),
                ]
            )
            warnings.warn(_msg, UserWarning, stacklevel=2)

    def print_results(self):
        if not self.kwargs["silent"]:
            if self.kwargs["dry_run"]:
                print(
                    "Dry run PyRosetta initialization from file: {0}".format(self.init_file),
                    "Parsed {0} PyRosetta initialization input files.".format(self.file_counter),
                    sep=os.linesep,
                )
            else:
                print(
                    "Initializing PyRosetta from file: {0}".format(self.init_file),
                    "Parsed {0} PyRosetta initialization input files written to: {1}".format(self.file_counter, self.kwargs["output_dir"]),
                    sep=os.linesep,
                )
            print(
                "Author(s): {0}".format(self.init_dict["author"]),
                "E-mail(s): {0}".format(self.init_dict["email"]),
                "License: {0}".format(self.init_dict["license"]),
                "Metadata: {0}".format(self.init_dict["metadata"]),
                "PyRosetta build: {0}".format(self.init_dict["pyrosetta_build"]),
                "Date/Time created (UTC): {0}".format(
                    datetime.datetime.strptime(
                        self.init_dict["datetime"],
                        self._strftime_format,
                    ).strftime("%b %d, %Y at %I:%M:%S %p")
                ),
                sep=os.linesep,
            )

    def pprint_options(self, options):
        if not self.kwargs["silent"]:
            if self.kwargs["dry_run"]:
                print("PyRosetta initialization options from dry run:")
            else:
                print("PyRosetta initialization options:")
            pprint(options)
            if self.kwargs["dry_run"]:
                print("Skipping PyRosetta initialization...")
            else:
                print("Running PyRosetta initialization...")

    def init(self):
        options = self.get_options()
        self.print_results()
        self.pyrosetta_build_warning()
        pyrosetta_kwargs = dict(
            options=options,
            extra_options="",
            set_logging_handler=self.kwargs["set_logging_handler"],
            notebook=self.kwargs["notebook"],
            silent=self.kwargs["silent"],
        )
        self.pprint_options(options)
        if not self.kwargs["dry_run"]:
            pyrosetta.init(**pyrosetta_kwargs)


class PyRosettaInitFileParser(object):
    @staticmethod
    def init_from_file(
        init_file,
        dry_run=None,
        output_dir=None,
        database=None,
        set_logging_handler=None,
        notebook=None,
        silent=None,
    ):
        """
        Initialize PyRosetta from an '.init' file.

        This method deserializes PyRosetta initialization input files from an input '.init' file into an output directory, and
        then runs `pyrosetta.init` with the cached Rosetta command line options pointing to files written to the output directory.
        Therefore, it may be helpful to enable the 'dry_run' keyword argument to first inspect the Rosetta command line options
        before committing to writing all files to disk and running PyRosetta initialization.

        Args:
            init_file: a required `str` object representing the input '.init' file.

        **kwargs:
            dry_run: An optional `bool` object specifying whether or not to write PyRosetta input files and perform PyRosetta
                initialization. If `True`, then only print the PyRosetta initialization options that would be run if it were `False`.
                Default: False
            output_dir: An optional `str` object representing the output directory in which to decompress PyRosetta input files.
                Default: `./pyrosetta_init_files`
            database: An optional `str` object representing the path to the PyRosetta database. By default, the PyRosetta database
                is found using `pyrosetta._rosetta_database_from_env()`, but if the search fails then the PyRosetta database path
                may be manually input here.
                Default: None
            set_logging_handler: An optional object passed to `pyrosetta.init(set_logging_handler=...)` during PyRosetta initialization.
                If `None`, then the default `pyrosetta.init` keyword argument parameter is used. 
                Default: None
            notebook: An optional object passed to `pyrosetta.init(notebook=...)` during PyRosetta initialization.
                If `None`, then the default `pyrosetta.init` keyword argument parameter is used.
                Default: None
            silent: An optional object passed to `pyrosetta.init(silent=...)` during PyRosetta initialization.
                If `None`, then the default `pyrosetta.init` keyword argument parameter is used.
                Default: None
        """
        return PyRosettaInitFileReader(
            init_file,
            dry_run=dry_run,
            output_dir=output_dir,
            database=database,
            set_logging_handler=set_logging_handler,
            notebook=notebook,
            silent=silent,
        ).init()

    @staticmethod
    def dump_init_file(
        output_filename,
        author=None,
        email=None,
        license=None,
        metadata=None,
        overwrite=None,
    ):
        """
        Write a PyRosetta initialization '.init' file.

        This method uses the `ProtocolSettingsMetric` to get Rosetta command line options and serializes any input files (including
        files containing lists of files) into the output '.init' file. The Rosetta database directory is automatically excluded.
        Only the relative paths of any input directories (from the current working directory) are saved in the Rosetta command
        line options (e.g., '-in:path:bcl /path/to/my/bcl_rosetta' is saved as '-in:path:bcl ./bcl_rosetta'). Therefore, it may be
        helpful to add comments to the 'metadata' keyword argument parameter about specific PyRosetta initialization requirements.

        Args:
            output_filename: a required `str` object representing the output '.init' file.

        **kwargs:
            author: An optional `str` object representing the author's/authors' name(s) or username(s).
                Default: None
            email: An optional `str` object representing the author's/authors' email address(es).
                Default: None
            license: An optional `str` object representing the license(s) for the output '.init' file.
                Default: None
            metadata: An optional JSON-serializable object representing any additional metadata to save to the output '.init' file.
                Default: {}
            overwrite: An optional `bool` object specifying whether or not to overwrite the output '.init' file if it exists.
                If `False`, then raise an error if the output '.init' file already exists.
                Default: False
        """
        return PyRosettaInitFileWriter(
            output_filename,
            author=author,
            email=email,
            license=license,
            metadata=metadata,
            overwrite=overwrite,
        ).dump()
