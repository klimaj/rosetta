# :noTabs=true:
# (c) Copyright Rosetta Commons Member Institutions.
# (c) This file is part of the Rosetta software suite and is made available under license.
# (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
# (c) For more information, see http://www.rosettacommons.org. Questions about this can be
# (c) addressed to University of Washington CoMotion, email: license@uw.edu.


__author__ = "Jason C. Klima"


import argparse
import glob
import json
import os
import pyrosetta


def main(tmp_dir):
    pdb_files = glob.glob(os.path.join(tmp_dir, "*.pdb")) + glob.glob(os.path.join(tmp_dir, "*.pdb.gz"))
    assert len(pdb_files) > 0, "PDB files do not exist."
    list_file = os.path.join(tmp_dir, "my_file.list")
    assert os.path.isfile(list_file), "List file does not exist."
    extra_res_fa_files = glob.glob(os.path.join(tmp_dir, "*.params"))
    assert len(extra_res_fa_files) > 0, "Extra residue files do not exist."
    patch_files = glob.glob(os.path.join(tmp_dir, "*.txt"))
    assert len(patch_files) > 0, "Patch files do not exist."
    if not pyrosetta.rosetta.basic.was_init_called():
        pyrosetta.init(
            options="-run:constant_seed 1 -run:jran 1234567 -out:levels core.init:0 basic.random.init_random_generator:0",
            extra_options="-s {0} -l {1} -extra_res_fa {2} -extra_res_fa {3} -extra_patch_fa {4}".format(
                " ".join(pdb_files),
                list_file,
                " ".join(extra_res_fa_files[:-1]),
                extra_res_fa_files[-1],
                " ".join(patch_files),
            ),
            set_logging_handler="logging",
            notebook=None,
            silent=True,
        )
    else:
        raise RuntimeError("PyRosetta is already initialized.")

    init_file = os.path.join(tmp_dir, "my.init")
    pyrosetta.dump_init_file(
        init_file,
        author="Username",
        email="test@example",
        license="LICENSE.PyRosetta.md",
        metadata=[
            "Contains IGU, GNP, CYX extra residues and the 3prime5prime_methyl_phosphate patch file",
            "Version 1.0",
        ],
        overwrite=False,
    )
    pose = pyrosetta.Pose()
    base_res_set = pose.conformation().modifiable_residue_type_set_for_conf().base_residue_types()
    name3_set = set(base_res_set.pop().name3() for _ in range(base_res_set.capacity()))
    with open(os.path.join(tmp_dir, "res_types.json"), "w") as f:
        json.dump(list(name3_set), f)

if __name__ == "__main__":
    print("Running: {0}".format(__file__))
    parser = argparse.ArgumentParser()
    parser.add_argument('--tmp_dir', type=str)
    args = parser.parse_args()
    main(args.tmp_dir)
