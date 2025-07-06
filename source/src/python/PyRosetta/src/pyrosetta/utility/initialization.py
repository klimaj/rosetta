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
import bz2
import collections
import json
import os
import pickle
import pyrosetta

from pyrosetta.rosetta.core.simple_metrics import get_sm_data
from pyrosetta.rosetta.protocols.rosetta_scripts import XmlObjects


class PyRosettaInitFileParser(object):
    _prefix_string = "[PyRosettaInitStringFile]"
    _prefix_binary = "[PyRosettaInitBinaryFile]"

    def __init__(self):
        self.pose = pyrosetta.Pose()

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
    
    def apply_protocol_settings_metric(self):
        xml_obj = self.get_protocol_settings_metric()
        xml_obj.apply(self.pose)

    def get_simple_metric_data(self):
        sm_data = get_sm_data(self.pose)
        data = dict(sm_data.get_composite_string_metric_data())
        return dict(data["{0}_opt".format(__class__.__name__)])

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

    def encode_file(self, filename):
        results = {}
        if self.is_file_containing_list_of_files(filename):
            result = {}
            with open(filename, "r") as f1:
                for line in f1.read().splitlines():
                    if self.is_text_file(line):
                        with open(line, "r") as f2:
                            result[os.path.basename(line)] = "{0}{1}".format(
                                PyRosettaInitFileParser._prefix_string,
                                base64.b64encode(pickle.dumps(f2.read())).decode(),
                            )
                    else:
                        with open(line, "rb") as f2:
                            result[os.path.basename(line)] = "{0}{1}".format(
                                PyRosettaInitFileParser._prefix_binary,
                                base64.b64encode(f2.read()),
                            )
        elif self.is_text_file(filename):
            with open(filename, "r") as f:
                result = "{0}{1}".format(
                    PyRosettaInitFileParser._prefix_string,
                    base64.b64encode(pickle.dumps(f.read())).decode(),
                )
        else:
            with open(filename, "rb") as f:
                result = "{0}{1}".format(
                    PyRosettaInitFileParser._prefix_binary,
                    base64.b64encode(f.read()),
                )
        results[os.path.basename(filename)] = result

    def init_from_file(self, init_file, output_dir=None):
        if not pyrosetta.rosetta.basic.was_init_called():
            with open(init_file, "r") as f:
                encoded_flags_dict = json.load(f)

            if output_dir is None:
                output_dir = os.path.join(os.getcwd(), "pyrosetta_init_files")
            os.makedirs(output_dir, exist_ok=False)

            # options = ""
            # for option_name, values in encoded_flags_dict.items():
            #     values_formatted = ""
            #     for value in values:
            #         if isinstance(value, str):
            #             if value.startswith(PyRosettaInitFileParser._prefix_string):
            #                 value = value.split(PyRosettaInitFileParser._prefix_string)[1]
            #                 file_content = pickle.loads(base64.b64decode(value, validate=True))
            #                 file = os.path.join()
            #                 with open(file, "w") as f:
            #                     f.write(file_content)

            #                 with open()
            #                 values_formatted += 
            #             elif value.startswith(PyRosettaInitFileParser._prefix_binary):
            #             else:

        else:
            raise RuntimeError(
                "PyRosetta must not be already initialized to initialize from a file."
            )


    def dump_init_file(self, output_filename):
        if pyrosetta.rosetta.basic.was_init_called():
            self.apply_protocol_settings_metric()
            flags_dict = self.get_simple_metric_data()
            encoded_flags_dict = collections.defaultdict(list)
            for option_name, values in flags_dict.items():
                for value in values.split():
                    if os.path.isfile(value):
                        encoded_flags_dict[option_name].append(self.encode_file(value))
                    else:
                        encoded_flags_dict[option_name].append(value)
            with open(output_filename, "w") as f:
                json.dump(encoded_flags_dict, f, indent=4)
        else:
            raise RuntimeError(
                "PyRosetta must be already initialized to dump an initialization file."
            )
