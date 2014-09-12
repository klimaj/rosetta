// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet;
//
// This file is made available under the Rosetta Commons license.
// See http://www.rosettacommons.org/license
// (C) 199x-2007 University of Washington
// (C) 199x-2007 University of California Santa Cruz
// (C) 199x-2007 University of California San Francisco
// (C) 199x-2007 Johns Hopkins University
// (C) 199x-2007 University of North Carolina, Chapel Hill
// (C) 199x-2007 Vanderbilt University

/// @brief Prints the energy difference between a protein+ligand complex and the
/// 	system formed by the protein and the ligand considered in isolation.
///
/// @param[in] -s <COMPLEX>, where <COMPLEX> is the path to the PDB file
/// 	containing the complex.
/// @param[in] -extra_res_fa <LIGPAR>, where <LIGPAR> is the path to the .params
/// 	file describing the ligand.
/// @param[in] -min_first: optional boolean flag asking for minimization of the
/// 	protein+ligand complex before scoring.
///
/// @details: If flag -min_first is supplied, the program prints the minimized
/// 	complex to "min_bou.pdb".
///
/// @author Andrea Bazzoli (bazzoli@ku.edu)

#include <protocols/rigid/RigidBodyMover.hh>
#include <core/optimization/MinimizerOptions.hh>
#include <core/optimization/AtomTreeMinimizer.hh>
#include <core/kinematics/MoveMap.hh>
#include <basic/Tracer.hh>
#include <basic/options/util.hh>
#include <basic/options/option.hh>
#include <basic/options/option_macros.hh>
#include <basic/options/keys/out.OptionKeys.gen.hh>
#include <core/import_pose/import_pose.hh>
#include <devel/init.hh>
#include <core/scoring/ScoreFunctionFactory.hh>
#include <core/scoring/ScoreFunction.hh>
#include <core/io/pdb/pose_io.hh>
#include <core/pose/Pose.hh>
#include <string>

using namespace basic::options::OptionKeys;
using basic::options::option;

OPT_KEY( Boolean, min_first )

static basic::Tracer TR( "apps.pilot.bou-min-ubo-nrg-jump" );


////////////////////////////////////////////////////////////////////////////////
//                                    MAIN                                    //
////////////////////////////////////////////////////////////////////////////////

int main( int argc, char * argv [] )
{
	NEW_OPT( min_first, "First minimize the protein+ligand complex", false);

	devel::init(argc, argv);

	// load bound pose from pdb file
	core::pose::Pose bou_ps;
	std::string const input_pdb_name( basic::options::start_file() );
	core::import_pose::pose_from_pdb( bou_ps, input_pdb_name );

	// create score function
	core::scoring::ScoreFunctionOP scorefxn = core::scoring::get_score_function();

	// minimize bound pose, if requested
	if(option[min_first]) {

		core::optimization::MinimizerOptions minoptions("dfpmin", 0.00001, true);
		minoptions.nblist_auto_update( true );

		core::kinematics::MoveMap mm;
		mm.set_bb(true);
		mm.set_chi(true);
		mm.set_jump(true);

		core::optimization::AtomTreeMinimizer minimizer;
		minimizer.run(bou_ps, mm, *scorefxn, minoptions);

		bou_ps.dump_pdb("min_bou.pdb");
	}

	// score bound pose
	core::Real bou_score = (*scorefxn)(bou_ps);
	TR << "bound pose's energy: " << bou_score << std::endl;

	// create unbound pose
	core::pose::Pose ubo_ps = bou_ps;
	core::Size const rb_jump = ubo_ps.num_jump();
	core::Real const ubo_dist = 1000000.0;
	protocols::rigid::RigidBodyTransMover trans_mover( ubo_ps, rb_jump );
	trans_mover.step_size(ubo_dist);
	trans_mover.apply(ubo_ps);

	// score unboud pose
	core::Real ubo_score = (*scorefxn)(ubo_ps);
	TR << "unbound pose's energy: " << ubo_score << std::endl;

	TR << "energy difference: " << (bou_score - ubo_score) << std::endl;
}
