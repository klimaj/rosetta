# :noTabs=true:
# (c) Copyright Rosetta Commons Member Institutions.
# (c) This file is part of the Rosetta software suite and is made available under license.
# (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
# (c) For more information, see http://www.rosettacommons.org. Questions about this can be
# (c) addressed to University of Washington CoMotion, email: license@uw.edu.


__author__ = "Jason C. Klima"


import argparse
import json
import os
import pyrosetta


def main(tmp_dir):
    init_file = os.path.join(tmp_dir, "my.init")
    init_dir = os.path.join(tmp_dir, "pyrosetta_init_files")
    pyrosetta.init_from_file(
        init_file,
        output_dir=init_dir,
        dry_run=True,
        database=None,
        set_logging_handler=None,
        notebook=None,
        silent=False,
    )
    assert not pyrosetta.rosetta.basic.was_init_called(), "PyRosetta was initialized with `dry_run=True`"
    pyrosetta.init_from_file(
        init_file,
        output_dir=init_dir,
        dry_run=True,
        database=os.path.relpath(pyrosetta._rosetta_database_from_env()),
        set_logging_handler=None,
        notebook=None,
        silent=False,
    )
    assert not pyrosetta.rosetta.basic.was_init_called(), "PyRosetta was initialized with `dry_run=True`"
    pyrosetta.init_from_file(
        init_file,
        output_dir=init_dir,
        dry_run=False,
        database=None,
        set_logging_handler=None,
        notebook=None,
        silent=False,
    )
    pose = pyrosetta.Pose()
    base_res_set = pose.conformation().modifiable_residue_type_set_for_conf().base_residue_types()
    name3_set = set(base_res_set.pop().name3() for _ in range(base_res_set.capacity()))
    with open(os.path.join(tmp_dir, "res_types.json"), "r") as f:
        original_name3_set = set(json.load(f))
    assert name3_set == original_name3_set, "Residue type sets are not identical."

if __name__ == "__main__":
    print("Running: {0}".format(__file__))
    parser = argparse.ArgumentParser()
    parser.add_argument('--tmp_dir', type=str)
    args = parser.parse_args()
    main(args.tmp_dir)
