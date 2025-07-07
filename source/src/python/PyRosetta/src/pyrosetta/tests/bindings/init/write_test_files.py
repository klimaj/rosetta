# :noTabs=true:
# (c) Copyright Rosetta Commons Member Institutions.
# (c) This file is part of the Rosetta software suite and is made available under license.
# (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
# (c) For more information, see http://www.rosettacommons.org. Questions about this can be
# (c) addressed to University of Washington CoMotion, email: license@uw.edu.


__author__ = "Jason C. Klima"


import argparse
import os
import pyrosetta
import shutil


def main(tmp_dir):
    if not pyrosetta.rosetta.basic.was_init_called():
        pyrosetta.init(
            options="-run:constant_seed 1 -ex2",
            extra_options="-out:levels core.init:0 basic.random.init_random_generator:0",
            set_logging_handler="logging",
            silent=True,
        )
    else:
        raise RuntimeError("PyRosetta is already initialized.")

    list_file = os.path.join(tmp_dir, "my_file.list")
    with open(list_file, "w") as f:
        for i in range(10):
            pdb_file = os.path.join(tmp_dir, "tmp_{0}.pdb".format(i))
            f.write(pdb_file + os.linesep)
            pose = pyrosetta.pose_from_sequence("A" * i)
            pyrosetta.dump_pdb(pose, pdb_file)

    database = pyrosetta._rosetta_database_from_env()
    if database is None:
        raise RuntimeError("Could not find the PyRosetta database.")
    extra_files = [
        os.path.join(database, "chemical/residue_type_sets/fa_standard/residue_types/nucleic/rna_nonnatural/IGU.params"),
        os.path.join(database, "chemical/residue_type_sets/fa_standard/residue_types/nucleic/dna/GNP.params"),
        os.path.join(database, "chemical/residue_type_sets/fa_standard/residue_types/sidechain_conjugation/CYX.params"),
        os.path.join(database, "chemical/residue_type_sets/fa_standard/patches/nucleic/rna/3prime5prime_methyl_phosphate.txt"),
    ]
    for src in extra_files:
        if not os.path.isfile(src):
            raise FileNotFoundError("File not found in the PyRoseta database: {0}".format(src))
        dst = os.path.join(tmp_dir, os.path.basename(src))
        shutil.copy(src, dst)

if __name__ == "__main__":
    print("Running: {0}".format(__file__))
    parser = argparse.ArgumentParser()
    parser.add_argument('--tmp_dir', type=str)
    args = parser.parse_args()
    main(args.tmp_dir)
