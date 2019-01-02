# :noTabs=true:
#
# (c) Copyright Rosetta Commons Member Institutions.
# (c) This file is part of the Rosetta software suite and is made available under license.
# (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
# (c) For more information, see http://www.rosettacommons.org.
# (c) Questions about this can be addressed to University of Washington CoMotion, email: license@uw.edu.

import json
import os
import pyrosetta
import random


def output_scorefile(pose, pdb_name, current_name, scorefilepath, scorefxn, nstruct,
                     native_pose=None, additional_decoy_info=None, json_format=False):
    """
    Moved from PyJobDistributor (Jared Adolf-Bryfogle)
    Creates a scorefile if none exists, or appends the current one.
    Calculates and writes CA_rmsd if native pose is given,
    as well as any additional decoy info
    """
    total_score = scorefxn(pose)   # Scores pose and calculates total score.
    
    if json_format:
        entries = {}
        entries.update(
            {
             "pdb_name": str(pdb_name),
             "filename": str(current_name),
             "nstruct": int(nstruct)
             }
        )
        entries.update(dict(pose.scores))
        if native_pose:
            entries["rmsd"] = float(pyrosetta.rosetta.core.scoring.CA_rmsd(native_pose, pose))
        if additional_decoy_info:
            entries["additional_decoy_info"] = str(additional_decoy_info)

        with open(scorefilepath, "a") as f:
            json.dump(entries, f)
            f.write("\n")

    else:
        if not os.path.exists(scorefilepath):
            with open(scorefilepath, "w") as f:
                f.write("pdb name: " + pdb_name + "     nstruct: " + str(nstruct) + "\n")

        score_line = pose.energies().total_energies().weighted_string_of(scorefxn.weights())
        output_line = "filename: " + current_name + " total_score: " + str(round(total_score, 3))

        # Calculates rmsd if native pose is defined.
        if native_pose:
            rmsd = pyrosetta.rosetta.core.scoring.CA_rmsd(native_pose, pose)
            output_line = output_line + " rmsd: " + str(round(rmsd, 3))

        with open(scorefilepath, "a") as f:
            if additional_decoy_info:
                f.write(output_line + " " + score_line + " " + additional_decoy_info + "\n")
            else:
                f.write(output_line + " " + score_line + "\n")


class PyJobDistributor:

    def __init__(self, pdb_name, nstruct, scorefxn, compress=False):
        self.pdb_name = pdb_name
        self.nstruct = nstruct
        self.compress = compress

        self.current_id = None
        self.current_name = None          # Current decoy name
        self.scorefxn = scorefxn          # Used for final score calculation
        self.native_pose = None           # Used for rmsd calculation
        self.additional_decoy_info = None # Used for any additional decoy information you want stored
        self.json_format = False          # Used for JSON formatted scorefile

        self.sequence = list(range(nstruct))
        random.shuffle(self.sequence)
        self.start_decoy()                # Initializes the job distributor

    @property
    def job_complete(self):
        return len(self.sequence) == 0

    def start_decoy(self):
        while self.sequence:

            self.current_id = self.sequence[0]
            self.current_name = self.pdb_name + "_" + str(self.current_id) + (".pdb.gz" if self.compress else ".pdb")
            self.current_in_progress_name = self.current_name + ".in_progress"

            if (not os.path.isfile(self.current_name)) and (not os.path.isfile(self.current_in_progress_name)):
                with open(self.current_in_progress_name, "w") as f:
                    f.write("This decoy is in progress.")
                print("Working on decoy: {}".format(self.current_name))
                break

    def output_decoy(self, pose):
        if os.path.isfile(self.current_name): # decoy already exists, probably written to other process -> moving to next decoy if any

            if os.path.isfile(self.current_in_progress_name):
                os.remove(self.current_in_progress_name)

            if not self.job_complete:
                self.start_decoy()
                self.output_decoy(self, pose)

        else:
            if self.compress:
                s = pyrosetta.rosetta.std.ostringstream()
                pose.dump_pdb(s)

                z = pyrosetta.rosetta.utility.io.ozstream(self.current_name)
                z.write(s.str(), len(s.str()))
                del z

            else:
                pose.dump_pdb(self.current_name)

            score_tag = ".fasc" if pose.is_fullatom() else ".sc"

            scorefile = self.pdb_name + score_tag
            output_scorefile(pose, self.pdb_name, self.current_name, scorefile, self.scorefxn,
                             self.nstruct, self.native_pose, self.additional_decoy_info, self.json_format)

            self.sequence.remove(self.current_id)

            if os.path.isfile(self.current_in_progress_name):
                os.remove(self.current_in_progress_name)

            self.start_decoy()
